#include "deck_link.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "cJSON.h"
#include "deck_model.h"
#include "lvgl.h"

namespace agent_deck::link {

namespace {

// How often the link's timer looks at the clock: well inside a resend and a lost bridge.
constexpr uint32_t kTickMs = 250;
// The deepest JSON a line may hold: a record is one object of plain values, and cJSON recurses on LVGL's stack.
constexpr int kMaxDepth = 2;

struct Pending {
    uint32_t id = 0;
    bool allow = false;
    bool is_answer = false;  // a question's: `part` and `chosen`, not `allow`
    uint8_t part = 0;
    uint8_t chosen = 0;
    bool used = false;
    int sends = 0;
    uint32_t next_ms = 0;
};

struct State {
    SendFn send = nullptr;
    bool up = false;
    uint32_t heard_ms = 0;  // lv_tick_get() at the last good line
    std::array<Pending, model::kMaxRequests> pending{};
    lv_timer_t *timer = nullptr;
};

State s_link;
char s_hello[kMaxShortOut] = {};

bool SendJson(const char *json)
{
    char line[kMaxShortOut];
    const size_t n = Encode(line, sizeof(line), json);
    return n > 0 && s_link.send != nullptr && s_link.send(line, n);
}

void SayHello()
{
    SendJson(s_hello);
}

// A hello field: 1 to kMaxHelloField characters of a–z, 0–9 and '-', so the line needs no escaping.
bool Plain(const char *s)
{
    if (s == nullptr) {
        return false;
    }
    size_t n = 0;
    for (; s[n] != '\0'; ++n) {
        const char c = s[n];
        if (n >= kMaxHelloField || !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            return false;
        }
    }
    return n > 0;
}

void SendPending(Pending &p)
{
    char json[80];
    if (p.is_answer) {
        std::snprintf(json, sizeof(json), "{\"type\":\"answer\",\"id\":%lu,\"part\":%u,\"chosen\":%u}", static_cast<unsigned long>(p.id),
                      static_cast<unsigned>(p.part), static_cast<unsigned>(p.chosen));
    } else {
        std::snprintf(json, sizeof(json), "{\"type\":\"decision\",\"id\":%lu,\"allow\":%s}", static_cast<unsigned long>(p.id),
                      p.allow ? "true" : "false");
    }
    SendJson(json);
    ++p.sends;
    p.next_ms = lv_tick_get() + kResendMs;
}

// The bridge gone: what it said is stale, so the deck forgets it — standby as it is — and says it is connecting again.
void Lose()
{
    s_link.up = false;
    for (Pending &p : s_link.pending) {
        p.used = false;
    }
    model::Forget();
    model::SetLink(false);
    SayHello();
}

void OnTick(lv_timer_t *)
{
    const uint32_t now = lv_tick_get();
    if (s_link.up && lv_tick_diff(now, s_link.heard_ms) > kLostMs) {
        Lose();
        return;
    }
    for (Pending &p : s_link.pending) {
        if (p.used && static_cast<int32_t>(now - p.next_ms) >= 0) {
            if (p.sends >= kMaxSends) {
                p.used = false;  // unheard: the terminal still asks, and the hook gives it back in time
            } else {
                SendPending(p);
            }
        }
    }
}

// A record's number, clamped to [lo, hi] before the cast, so a stray value is never undefined behaviour.
double Clamped(const cJSON *json, const char *key, double fallback, double lo, double hi)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(json, key);
    return cJSON_IsNumber(v) ? std::clamp(v->valuedouble, lo, hi) : fallback;
}

int Number(const cJSON *json, const char *key, int fallback)
{
    return static_cast<int>(Clamped(json, key, fallback, -2147483648.0, 2147483647.0));
}

uint32_t Count(const cJSON *json, const char *key)
{
    return static_cast<uint32_t>(Clamped(json, key, 0, 0, 4294967295.0));
}

uint16_t Id16(const cJSON *json, const char *key)
{
    return static_cast<uint16_t>(Clamped(json, key, 0, 0, 65535));
}

// The deepest nesting a line's JSON reaches, strings skipped.
int Depth(const char *json, size_t len)
{
    int now = 0;
    int most = 0;
    bool in_string = false;
    for (size_t i = 0; i < len; ++i) {
        const char c = json[i];
        if (in_string) {
            if (c == '\\') {
                ++i;
            } else if (c == '"') {
                in_string = false;
            }
        } else if (c == '"') {
            in_string = true;
        } else if (c == '{' || c == '[') {
            most = std::max(most, ++now);
        } else if (c == '}' || c == ']') {
            --now;
        }
    }
    return most;
}

const char *Text(const cJSON *json, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(json, key);
    return cJSON_IsString(v) && v->valuestring != nullptr ? v->valuestring : "";
}

// What fits of `src`, never half a UTF-8 character: a cut backs up to its lead byte.
void Copy(char *dst, size_t cap, const char *src)
{
    size_t n = strnlen(src, cap - 1);
    if (src[n] != '\0') {
        while (n > 0 && (static_cast<unsigned char>(src[n]) & 0xC0) == 0x80) {
            --n;
        }
    }
    std::memcpy(dst, src, n);
    dst[n] = '\0';
}

// A record's `limit`: the window whose limit ended a turn, `unknown` until the bridge has read which; none when absent.
model::Limit LimitOf(const cJSON *r)
{
    const char *l = Text(r, "limit");
    return l[0] == '\0'                        ? model::Limit::kNone
           : std::strcmp(l, "five_hour") == 0 ? model::Limit::kFiveHour
           : std::strcmp(l, "seven_day") == 0 ? model::Limit::kSevenDay
                                              : model::Limit::kUnknown;
}

void ApplyAgent(const cJSON *r)
{
    model::Agent a{};
    a.id = Id16(r, "id");
    a.parent = Id16(r, "parent");
    Copy(a.name, sizeof(a.name), Text(r, "name"));
    Copy(a.kind, sizeof(a.kind), Text(r, "kind"));
    const char *state = Text(r, "state");
    a.state = std::strcmp(state, "needs_you") == 0 ? model::AgentState::kNeedsYou
              : std::strcmp(state, "done") == 0    ? model::AgentState::kDone
              : std::strcmp(state, "idle") == 0    ? model::AgentState::kIdle
                                                   : model::AgentState::kWorking;
    a.background = cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(r, "active"));  // an older bridge says nothing
    a.since_s = Count(r, "since_s");
    Copy(a.detail, sizeof(a.detail), Text(r, "detail"));
    a.context_pct = static_cast<int8_t>(Clamped(r, "context_pct", -1, -1, 100));
    a.tokens = Count(r, "tokens");
    Copy(a.model, sizeof(a.model), Text(r, "model"));
    a.limit = LimitOf(r);
    model::SetAgent(a);
}

void ApplyQuestion(const cJSON *r)
{
    model::Question q{};
    q.id = Count(r, "id");
    q.agent = Id16(r, "agent");
    q.part = static_cast<uint8_t>(Clamped(r, "part", 0, 0, 255));
    Copy(q.tool, sizeof(q.tool), Text(r, "tool"));
    q.question = Text(r, "question");
    q.multi = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(r, "multi"));
    const cJSON *option = nullptr;
    cJSON_ArrayForEach(option, cJSON_GetObjectItemCaseSensitive(r, "options"))
    {
        if (cJSON_IsString(option) && q.option_count < model::kMaxOptions) {
            q.options[q.option_count++] = option->valuestring;
        }
    }
    if (!model::OpenQuestion(q)) {
        Answered(q.id, q.part, 0);  // no room: the bridge holds it for an answer, so the terminal must be told now
    }
}

void ApplyAck(const cJSON *r)
{
    // `part` too: every part of a question shares its id, and this may be the ack for the last one.
    const uint32_t id = Count(r, "id");
    const int part = Number(r, "part", -1);
    for (Pending &p : s_link.pending) {
        if (p.used && p.id == id && (!p.is_answer || part < 0 || p.part == part)) {
            p.used = false;
        }
    }
}

bool Apply(const cJSON *r)
{
    const char *type = Text(r, "type");
    if (std::strcmp(type, "agent") == 0) {
        ApplyAgent(r);
    } else if (std::strcmp(type, "remove") == 0) {
        model::RemoveAgent(Id16(r, "id"));
    } else if (std::strcmp(type, "usage") == 0) {
        model::SetUsage({static_cast<int8_t>(Clamped(r, "five_hour_pct", -1, -1, 100)), Number(r, "five_hour_resets_min", -1),
                         static_cast<int8_t>(Clamped(r, "seven_day_pct", -1, -1, 100)), Number(r, "seven_day_resets_min", -1), LimitOf(r)});
    } else if (std::strcmp(type, "request") == 0) {
        model::Request q{};
        q.id = Count(r, "id");
        q.agent = Id16(r, "agent");
        Copy(q.tool, sizeof(q.tool), Text(r, "tool"));
        q.body = Text(r, "body");
        q.cut = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(r, "cut"));
        model::OpenRequest(q);  // a fifth is refused: the bridge's hold gives it back to the terminal
    } else if (std::strcmp(type, "question") == 0) {
        ApplyQuestion(r);
    } else if (std::strcmp(type, "close") == 0) {
        model::CloseRequest(Count(r, "id"), std::strcmp(Text(r, "reason"), "timed_out") == 0 ? model::CloseReason::kTimedOut
                                                                                              : model::CloseReason::kAnsweredElsewhere);
    } else if (std::strcmp(type, "standby") == 0) {
        model::SetStandby(true);  // not the active deck: the agents and usage cleared, the standby page, no cue
    } else if (std::strcmp(type, "reset") == 0) {
        model::Reset();  // the snapshot's first line takes the deck off standby — never the hello, which a standby deck gets too
    } else if (std::strcmp(type, "hello") == 0) {
        model::BridgeHello();  // a `reset` or `standby` follows: the connected cue waits for it
        SayHello();            // the bridge has just opened the port: the deck says it listens, and the bridge sends everything
    } else if (std::strcmp(type, "ack") == 0) {
        ApplyAck(r);
    } else if (std::strcmp(type, "ping") != 0) {
        return false;
    }
    return true;
}

// The slot an answer to `id` takes: an earlier part's of the same question, which it replaces — the bridge has moved on from that
// part — else a free one; nullptr with neither.
Pending *SlotFor(uint32_t id, bool is_answer)
{
    Pending *free = nullptr;
    for (Pending &p : s_link.pending) {
        if (is_answer && p.used && p.is_answer && p.id == id) {
            return &p;
        }
        if (!p.used && free == nullptr) {
            free = &p;
        }
    }
    return free;
}

}  // namespace

uint32_t Crc32(const char *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint8_t>(data[i]);
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

size_t Encode(char *out, size_t cap, const char *json)
{
    const size_t len = std::strlen(json);
    const int n = std::snprintf(out, cap, "%s%08lx %s", kPrefix, static_cast<unsigned long>(Crc32(json, len)), json);
    return n > 0 && static_cast<size_t>(n) < cap ? static_cast<size_t>(n) : 0;
}

bool Init(SendFn send, const Hello &who)
{
    if (!Plain(who.id) || !Plain(who.board) || !Plain(who.link)) {
        return false;
    }
    std::snprintf(s_hello, sizeof(s_hello), R"({"type":"hello","version":%d,"id":"%s","board":"%s","link":"%s","body":%u})", kHelloVersion,
                  who.id, who.board, who.link, static_cast<unsigned>(model::kBodyBytes));
    if (s_link.timer != nullptr) {
        lv_timer_delete(s_link.timer);
    }
    s_link = {};
    s_link.send = send;
    s_link.timer = lv_timer_create(OnTick, kTickMs, nullptr);
    if (s_link.timer == nullptr) {
        return false;
    }
    SayHello();
    return true;
}

bool Take(const char *line, size_t len)
{
    // "xxxxxxxx {json}": the CRC, a space, the JSON it covers.
    if (len < 10 || line[8] != ' ') {
        return false;
    }
    char hex[9];
    std::memcpy(hex, line, 8);
    hex[8] = '\0';
    char *end = nullptr;
    const uint32_t want = static_cast<uint32_t>(std::strtoul(hex, &end, 16));
    const char *json = line + 9;
    const size_t json_len = len - 9;
    if (end != hex + 8 || Crc32(json, json_len) != want || Depth(json, json_len) > kMaxDepth) {
        return false;
    }
    cJSON *r = cJSON_ParseWithLength(json, json_len);
    if (r == nullptr) {
        return false;
    }
    const bool known = Apply(r);
    cJSON_Delete(r);
    if (known) {
        s_link.heard_ms = lv_tick_get();
        if (!s_link.up) {
            s_link.up = true;
            model::SetLink(true);
        }
    }
    return known;
}

void Decided(uint32_t id, bool allow)
{
    Pending *p = SlotFor(id, false);
    if (p == nullptr) {
        LV_LOG_WARN("deck link: no room for the decision on %lu; the bridge's hold ends it", static_cast<unsigned long>(id));
        return;
    }
    *p = {id, allow, false, 0, 0, true, 0, 0};
    SendPending(*p);
}

void Answered(uint32_t id, uint8_t part, uint8_t chosen)
{
    Pending *p = SlotFor(id, true);
    if (p == nullptr) {
        LV_LOG_WARN("deck link: no room for the answer to %lu; the bridge's hold ends it", static_cast<unsigned long>(id));
        return;
    }
    *p = {id, false, true, part, chosen, true, 0, 0};
    SendPending(*p);
}

bool Use()
{
    return SendJson("{\"type\":\"use\"}");
}

bool Up()
{
    return s_link.up;
}

}  // namespace agent_deck::link
