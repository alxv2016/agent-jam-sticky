#include "deck_mock.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <utility>

#include "in_psram.h"
#include "lvgl.h"

namespace agent_deck::mock {
namespace {

using model::Agent;
using model::AgentState;
using model::CloseReason;
using model::Limit;
using model::Usage;

constexpr int kRequests = 7;

constexpr const char *kDeployBody = "cd web-shop && npm run build && npm run deploy -- --prod --region eu-west "
                                    "--confirm && curl -fsS https://shop.example.com/health";
constexpr const char *kEditBody = "components/app_ui/deck_home.cpp\n@@ -28,3 +28,3 @@\n-constexpr uint32_t kTickMs = 500;\n"
                                  "+constexpr uint32_t kTickMs = 250;\n /* for 4m counts on while a page is up */";

template <size_t N>
constexpr void Put(char (&to)[N], const char *from)
{
    size_t i = 0;
    for (; from[i] != '\0' && i + 1 < N; ++i) {
        to[i] = from[i];
    }
    to[i] = '\0';
}

// An agent record as the bridge sends it.
constexpr Agent A(uint16_t id, uint16_t parent, const char *name, AgentState state, uint32_t since_s, const char *detail, int8_t context,
                  uint32_t tokens, const char *model, bool background = false, Limit limit = Limit::kNone)
{
    Agent a{};
    a.id = id;
    a.parent = parent;
    Put(a.name, name);
    a.state = state;
    a.since_s = since_s;
    Put(a.detail, detail);
    a.context_pct = context;
    a.tokens = tokens;
    Put(a.model, model);
    a.background = background;
    a.limit = limit;
    return a;
}

constexpr Usage U(int8_t five, int32_t five_min, int8_t seven, int32_t seven_min, Limit limit = Limit::kNone)
{
    Usage u{};
    u.five_hour_pct = five;
    u.five_hour_resets_min = five_min;
    u.seven_day_pct = seven;
    u.seven_day_resets_min = seven_min;
    u.limit = limit;
    return u;
}

constexpr Step Rec(uint32_t at, Op op, const Agent &agent)
{
    Step s{};
    s.at_ms = at;
    s.when = When::kAlways;
    s.op = op;
    s.agent = agent;
    return s;
}
constexpr Step Use(uint32_t at, const Usage &usage)
{
    Step s{};
    s.at_ms = at;
    s.when = When::kAlways;
    s.op = Op::kUsage;
    s.usage = usage;
    return s;
}
constexpr Step Gone(uint32_t at, uint16_t id)
{
    Step s{};
    s.at_ms = at;
    s.when = When::kAlways;
    s.op = Op::kRemove;
    s.id = id;
    return s;
}
constexpr Step Ask(uint32_t at, int n, uint16_t agent, const char *tool, const char *body)
{
    Step s{};
    s.at_ms = at;
    s.when = When::kAlways;
    s.request = n;
    s.op = Op::kRequest;
    s.req = {agent, tool, body};
    return s;
}
constexpr Step Close(uint32_t at, When when, int n, CloseReason reason)
{
    Step s{};
    s.at_ms = at;
    s.when = when;
    s.request = n;
    s.op = Op::kClose;
    s.reason = reason;
    return s;
}
constexpr Step Fixed(uint32_t at, Op op)
{
    Step s{};
    s.at_ms = at;
    s.when = When::kAlways;
    s.op = op;
    return s;
}

constexpr Step Bat(uint32_t at, int percent, bool charging)
{
    Step s{};
    s.at_ms = at;
    s.when = When::kAlways;
    s.op = Op::kBattery;
    s.battery_pct = percent;
    s.charging = charging;
    return s;
}

constexpr AgentState kWork = AgentState::kWorking, kNeeds = AgentState::kNeedsYou, kDone = AgentState::kDone, kIdle = AgentState::kIdle;

// Some names and lines are longer than their room on purpose, so the stepped lines show on the glass from the first minute
// (STICKY_AGENT_DECK §4.11): agent-deck-studio's reads and edits, the build, docs-site-api-reference-migration's name and fetch.
// THE DAY (AGENT_DECK 18): each step a record at its time into the day. The ids `#n` are request n's, made unique per loop. A step
// marked kOpen is sent only while request n still waits — the timeout or the terminal's answer the bridge would report.
constexpr Step kDay[] = {
    Fixed(0, Op::kReset),
    Fixed(0, Op::kLink),
    Bat(0, 62, true),  // on the charger, the percent climbing; unplugged at 110 s
    Use(0, U(23, 184, 41, 5760)),
    Rec(200, Op::kAgent, A(1, 0, "agent-deck-studio", kWork, 0, "Thinking", 6, 12000, "")),
    Rec(3000, Op::kAgent, A(1, 0, "agent-deck-studio", kWork, 3, "Read: components/agent_deck/home_screen.cpp", 9, 18400, "")),
    Rec(6000, Op::kAgent, A(2, 1, "Explore", kWork, 0, "Grep: kSafeSquare", -1, 4200, "")),
    Rec(8000, Op::kAgent, A(3, 0, "web-shop", kWork, 0, "Bash: npm test", 31, 62000, "")),
    Rec(11000, Op::kAgent, A(1, 0, "agent-deck-studio", kWork, 11, "Edit: components/design_tokens/include/design_tokens.h", 14, 28000, "")),
    Rec(13000, Op::kAgent, A(2, 1, "Explore", kWork, 7, "Read: theme.h", -1, 9800, "")),
    Rec(16000, Op::kAgent, A(2, 1, "Explore", kDone, 0, "", -1, 15400, "Haiku 5.5")),
    Rec(18000, Op::kAgent, A(3, 0, "web-shop", kNeeds, 0, "Bash: npm run deploy", 33, 66000, "")),
    Ask(18100, 1, 3, "Bash", kDeployBody),
    Gone(22000, 2),
    Rec(24000, Op::kAgent, A(4, 3, "Plan", kWork, 0, "Read: web-shop/packages/checkout/package.json", -1, 5100, "")),
    Use(30000, U(31, 179, 42, 5755)),
    Rec(32000, Op::kAgent, A(1, 0, "agent-deck-studio", kNeeds, 0, "Question: Which base branch?", 18, 36000, "")),
    {32100, When::kAlways, 2, Op::kQuestion, {}, {}, 0, CloseReason::kAnsweredElsewhere, {},
     {1, 0, "Branch", "Which base branch?", {"main: the release branch", "develop: where the work lands"}, 2, false}, 0, false},
    Rec(38000, Op::kAgent, A(4, 3, "Plan", kDone, 0, "", -1, 11800, "Sonnet 5.5")),
    Gone(44000, 4),
    // Not answered on the deck in 30 s: the bridge's hold ran out, and the terminal asks.
    Close(48000, When::kOpen, 1, CloseReason::kTimedOut),
    Rec(48100, Op::kAgent, A(3, 0, "web-shop", kWork, 0, "Bash: npm run deploy", 36, 71000, "")),
    Rec(54000, Op::kAgent, A(3, 0, "web-shop", kDone, 0, "", 38, 75000, "Sonnet 5.5 \xE2\x80\xA2 high")),
    Close(58000, When::kOpen, 2, CloseReason::kTimedOut),
    Rec(58100, Op::kAgent, A(1, 0, "agent-deck-studio", kWork, 0, "Bash: idf.py build && python3 tests/bench/flash.py", 22, 44000, "")),
    // THE LONG STRETCH OF WORK: 30 s of typing with nothing asking for the hand, so the mascot's sweat shows on most days.
    Rec(64000, Op::kAgent, A(1, 0, "agent-deck-studio", kWork, 6, "Edit: deck_home.cpp", 27, 54000, "")),
    Rec(75000, Op::kAgent, A(3, 0, "web-shop", kIdle, 0, "", 38, 75000, "Sonnet 5.5 \xE2\x80\xA2 high", true)),
    Use(80000, U(38, 172, 43, 5748)),
    Bat(80000, 73, true),
    Rec(88000, Op::kAgent, A(1, 0, "agent-deck-studio", kNeeds, 0, "Edit: deck_home.cpp", 27, 54000, "")),
    Ask(88100, 3, 1, "Edit", kEditBody),
    // Not answered on the deck: allowed in the terminal 15 s on.
    Close(103000, When::kOpen, 3, CloseReason::kAnsweredElsewhere),
    Rec(104000, Op::kAgent, A(1, 0, "agent-deck-studio", kWork, 0, "Bash: tests/sim/run.sh", 33, 66000, "")),
    Rec(108000, Op::kAgent, A(1, 0, "agent-deck-studio", kDone, 0, "", 35, 70000, "Opus 5.5 \xE2\x80\xA2 high")),
    // THE BURST: four agents ask within two seconds, two sessions and two sub-agents, so the overview's count opens the needs-you
    // list of all four, the first row's request at 1 of 4. Left alone, the bridge's hold runs out at 150 s.
    Bat(110000, 84, false),
    Rec(110000, Op::kAgent, A(5, 0, "docs-site-api-reference-migration", kWork, 0, "Read: docs/reference/api/v2/endpoints.md", 12, 24000, "")),
    Rec(111000, Op::kAgent, A(6, 5, "Plan", kWork, 0, "Grep: endpoints", -1, 3800, "")),
    Rec(112000, Op::kAgent, A(7, 1, "Explore", kWork, 0, "Glob: components/*", -1, 2600, "")),
    Rec(114000, Op::kAgent, A(3, 0, "web-shop", kNeeds, 0, "Bash: npm run deploy", 40, 78000, "")),
    Ask(114100, 4, 3, "Bash", kDeployBody),
    Rec(114500, Op::kAgent, A(5, 0, "docs-site-api-reference-migration", kNeeds, 0, "WebFetch: https://docs.example.com/api/v2/endpoints", 14, 27000, "")),
    Ask(114600, 5, 5, "WebFetch", "https://docs.example.com/api/v2/endpoints"),
    Rec(115000, Op::kAgent, A(6, 5, "Plan", kNeeds, 0, "Edit: deck_home.cpp", -1, 5200, "")),
    Ask(115100, 6, 6, "Edit", kEditBody),
    Rec(115500, Op::kAgent, A(7, 1, "Explore", kNeeds, 0, "Bash: ls components", -1, 3100, "")),
    Ask(115600, 7, 7, "Bash", "ls -la components/app_ui"),
    // Not answered on the deck in 35 s: the bridge's hold ran out, and the terminals ask.
    Close(150000, When::kOpen, 4, CloseReason::kTimedOut),
    Close(150000, When::kOpen, 5, CloseReason::kTimedOut),
    Close(150000, When::kOpen, 6, CloseReason::kTimedOut),
    Close(150000, When::kOpen, 7, CloseReason::kTimedOut),
    Gone(152000, 6),
    Gone(152000, 7),
    Rec(153000, Op::kAgent, A(5, 0, "docs-site-api-reference-migration", kDone, 0, "", 16, 30000, "Sonnet 5.5")),
    // THE PLAN'S LIMIT — the day ends on it: web-shop's turn stops on the 5-hour window.
    Rec(154000, Op::kAgent, A(3, 0, "web-shop", kDone, 0, "", 41, 80000, "Sonnet 5.5 \xE2\x80\xA2 high", false, Limit::kFiveHour)),
    Use(154000, U(100, 112, 45, 5740, Limit::kFiveHour)),
    // THE BRIDGE PUTS THIS DECK ON STANDBY — another deck is the active one (AGENT_DECK 4.13). Connect asks the Mac, which answers with the
    // snapshot three seconds on: the day begins again. Left alone, the next loop's snapshot ends it.
    Fixed(158000, Op::kStandby),
};

// What the bridge sends once the deck answers request n: the agent back at work, as its next hook would say. The denied record is
// the allowed one when a request has none (a question's 0 leaves it to the terminal, which the mock takes as answered there).
struct After {
    int request;
    Agent allowed;
    Agent denied;
    bool has_denied;
};
constexpr After kAfter[] = {
    {1, A(3, 0, "web-shop", kWork, 0, "Bash: npm run deploy", 36, 71000, ""), A(3, 0, "web-shop", kWork, 0, "Thinking", 34, 68000, ""), true},
    {2, A(1, 0, "agent-deck-studio", kWork, 0, "Thinking", 19, 38000, ""), {}, false},
    {3, A(1, 0, "agent-deck-studio", kWork, 0, "Edit: deck_home.cpp", 28, 56000, ""),
     A(1, 0, "agent-deck-studio", kWork, 0, "Thinking", 28, 55000, ""), true},
    {4, A(3, 0, "web-shop", kWork, 0, "Bash: npm run deploy", 42, 79000, ""), A(3, 0, "web-shop", kWork, 0, "Thinking", 41, 78500, ""), true},
    {5, A(5, 0, "docs-site-api-reference-migration", kWork, 0, "WebFetch: docs.example.com", 15, 28000, ""),
     A(5, 0, "docs-site-api-reference-migration", kWork, 0, "Thinking", 15, 27500, ""), true},
    {6, A(6, 5, "Plan", kWork, 0, "Edit: deck_home.cpp", -1, 5600, ""), A(6, 5, "Plan", kWork, 0, "Thinking", -1, 5400, ""), true},
    {7, A(7, 1, "Explore", kWork, 0, "Bash: ls components", -1, 3400, ""), A(7, 1, "Explore", kWork, 0, "Thinking", -1, 3300, ""), true},
};

// What the deck said, waiting for the next tick: the mock never answers from inside the model's own call.
struct Heard {
    uint32_t id;
    bool allow;
};

struct Mock {
    bool started = false;
    uint32_t day_ms = 0;  // lv_tick_get() when this day began
    uint32_t loop = 0;    // days played, for the ids
    size_t next = 0;      // the next step of kDay
    std::array<bool, kRequests + 1> open{};  // request n sent and not yet answered or closed
    std::array<Heard, 8> heard{};
    size_t heard_count = 0;
    lv_timer_t *timer = nullptr;
    // The Wi-Fi radio and the Mac, as the link will answer: what the screens asked, heard on the next tick, and the answers due.
    std::array<std::pair<model::WifiAsk, int>, 4> wifi_heard{};
    size_t wifi_heard_count = 0;
    uint32_t wifi_due_ms = 0;      // when the join ends; 0 for none
    bool wifi_fail_first = false;  // the join in progress fails once: a wrong password
    uint32_t connect_due_ms = 0;   // when the Mac answers a Connect with the snapshot; 0 for none
};
Mock &s_mock = lvgl_display::InPsram<Mock>();  // in PSRAM (lvgl_display::InPsram): the LVGL task's alone

// The Mac's networks, strongest first (AGENT_DECK 4.9), and the one whose password the first join of is refused.
constexpr const char *kRefusing = "Studio-Upstairs-Guest-5G";
constexpr uint32_t kWifiStepMs = 3000;       // the join, the mock's: the radio's own length comes with the link
constexpr uint32_t kConnectAnswerMs = 3000;  // the Mac's answer to a Connect, the mock's

void PutNetwork(model::Network &to, const char *name, bool locked, int signal)
{
    std::snprintf(to.name, sizeof to.name, "%s", name);
    to.locked = locked;
    to.signal = static_cast<uint8_t>(signal);
}

void PutFact(model::Fact &to, const char *name, const char *value)
{
    std::snprintf(to.name, sizeof to.name, "%s", name);
    std::snprintf(to.value, sizeof to.value, "%s", value);
}

// The record of a deck joined to `network`, with the joined page's facts.
model::Wifi JoinedWifi(const model::Network &network)
{
    model::Wifi w = model::GetWifi();
    w.stage = model::WifiStage::kJoined;
    w.network = network;
    const char *facts[][2] = {{"IP", "192.168.1.42"}, {"Host", "agentdeck-3f2a.local"}, {"Bridge", "Wi-Fi"},
                              {"Router", "192.168.1.1"}, {"Subnet", "255.255.255.0"}, {"MAC", "02:3F:2A:00:00:42"}};
    w.fact_count = 0;
    for (const auto &f : facts) {
        PutFact(w.facts[w.fact_count++], f[0], f[1]);
    }
    return w;
}

model::Wifi Offered(model::Wifi w)
{
    w.network_count = 0;
    PutNetwork(w.networks[w.network_count++], "Lab-2G-Upstairs-Annex", true, 3);
    PutNetwork(w.networks[w.network_count++], "Studio", true, 3);
    PutNetwork(w.networks[w.network_count++], "Studio-Guest", false, 2);
    PutNetwork(w.networks[w.network_count++], kRefusing, true, 1);
    return w;
}

void OnWifiAsk(model::WifiAsk ask, int arg)
{
    if (s_mock.wifi_heard_count < s_mock.wifi_heard.size()) {
        s_mock.wifi_heard[s_mock.wifi_heard_count++] = {ask, arg};
    }
}

void OnConnectAsk()
{
    s_mock.connect_due_ms = lv_tick_get() + kConnectAnswerMs;
}

// What the radio and the Mac do with an ask: the answers the link will send, written to the model a tick after the screen asked.
void WifiAnswer(model::WifiAsk ask, int arg)
{
    model::Wifi w = model::GetWifi();
    const uint32_t now = lv_tick_get();
    switch (ask) {
    case model::WifiAsk::kConnect:
    case model::WifiAsk::kScan:
        w = Offered(w);
        w.stage = model::WifiStage::kChoosing;
        break;
    case model::WifiAsk::kPick:
        if (arg < 0 || static_cast<size_t>(arg) >= w.network_count) {
            return;
        }
        w.network = w.networks[static_cast<size_t>(arg)];
        w.stage = model::WifiStage::kJoining;
        s_mock.wifi_fail_first = std::strcmp(w.network.name, kRefusing) == 0;
        s_mock.wifi_due_ms = now + kWifiStepMs;
        break;
    case model::WifiAsk::kRetry:
        w.stage = model::WifiStage::kJoining;
        s_mock.wifi_fail_first = false;
        s_mock.wifi_due_ms = now + kWifiStepMs;
        break;
    case model::WifiAsk::kDone:
        w = JoinedWifi(w.network);
        break;
    case model::WifiAsk::kCancel:
    case model::WifiAsk::kDisconnect:
    case model::WifiAsk::kForget:
        w.stage = model::WifiStage::kNotJoined;
        s_mock.wifi_due_ms = 0;
        break;
    case model::WifiAsk::kSwitch:
        if (arg != 0) {
            return;  // switched on: the radio stays where it was, not joined
        }
        w.stage = model::WifiStage::kNotJoined;
        s_mock.wifi_due_ms = 0;
        break;
    }
    model::SetWifi(w);
}

int RequestOf(uint32_t id)
{
    for (int n = 1; n <= kRequests; ++n) {
        if (RequestId(s_mock.loop, n) == id) {
            return n;
        }
    }
    return 0;
}

void OnDecision(uint32_t id, bool allow)
{
    if (s_mock.heard_count < s_mock.heard.size()) {
        s_mock.heard[s_mock.heard_count++] = {id, allow};
    }
}

void OnAnswer(uint32_t id, uint8_t, uint8_t chosen)
{
    OnDecision(id, chosen != 0);
}

void BeginDay(uint32_t now)
{
    s_mock.day_ms = now;
    s_mock.next = 0;
    s_mock.open = {};
}

// An answer from the deck: the agent goes back to work, as the bridge's next hook would say.
void Answered(const Heard &h)
{
    const int n = RequestOf(h.id);
    if (n == 0 || !s_mock.open[static_cast<size_t>(n)]) {
        return;  // a resend, or another day's
    }
    s_mock.open[static_cast<size_t>(n)] = false;
    for (const After &after : kAfter) {
        if (after.request == n) {
            model::SetAgent(h.allow || !after.has_denied ? after.allowed : after.denied);
        }
    }
}

void Play(const Step &s)
{
    switch (s.op) {
    case Op::kReset: model::Reset(); break;
    case Op::kLink: model::SetLink(true); break;
    case Op::kUsage: model::SetUsage(s.usage); break;
    case Op::kAgent: model::SetAgent(s.agent); break;
    case Op::kRemove: model::RemoveAgent(s.id); break;
    case Op::kRequest: {
        model::Request r{};
        r.id = RequestId(s_mock.loop, s.request);
        r.agent = s.req.agent;
        std::snprintf(r.tool, sizeof r.tool, "%s", s.req.tool);
        r.body = s.req.body;
        model::OpenRequest(r);
        break;
    }
    case Op::kQuestion: {
        model::Question q{};
        q.id = RequestId(s_mock.loop, s.request);
        q.agent = s.question.agent;
        q.part = s.question.part;
        std::snprintf(q.tool, sizeof q.tool, "%s", s.question.tool);
        q.question = s.question.question;
        for (size_t i = 0; i < s.question.count; ++i) {
            q.options[i] = s.question.options[i];
        }
        q.option_count = s.question.count;
        q.multi = s.question.multi;
        model::OpenQuestion(q);
        break;
    }
    case Op::kClose: model::CloseRequest(RequestId(s_mock.loop, s.request), s.reason); break;
    case Op::kBattery: model::SetBattery({s.battery_pct, s.charging}); break;
    case Op::kStandby: model::SetStandby(true); break;
    }
}

void Tick(lv_timer_t *)
{
    const uint32_t now = lv_tick_get();
    for (size_t i = 0; i < s_mock.heard_count; ++i) {
        Answered(s_mock.heard[i]);
    }
    s_mock.heard_count = 0;
    const size_t asks = s_mock.wifi_heard_count;
    s_mock.wifi_heard_count = 0;
    const auto heard = s_mock.wifi_heard;
    for (size_t i = 0; i < asks; ++i) {
        WifiAnswer(heard[i].first, heard[i].second);
    }
    if (s_mock.wifi_due_ms != 0 && lv_tick_diff(now, s_mock.wifi_due_ms) < (1u << 31)) {
        s_mock.wifi_due_ms = 0;
        model::Wifi w = model::GetWifi();
        w.stage = s_mock.wifi_fail_first ? model::WifiStage::kFailed : model::WifiStage::kPaired;
        w.failure = model::WifiFailure::kWrongPassword;
        model::SetWifi(w);
    }
    if (s_mock.connect_due_ms != 0 && lv_tick_diff(now, s_mock.connect_due_ms) < (1u << 31)) {
        // The Mac's snapshot: the day begins again, its reset taking the deck off standby.
        s_mock.connect_due_ms = 0;
        ++s_mock.loop;
        BeginDay(now);
    }
    const uint32_t into = lv_tick_diff(now, s_mock.day_ms);
    while (s_mock.next < std::size(kDay) && kDay[s_mock.next].at_ms <= into) {
        const Step &s = kDay[s_mock.next++];
        const bool open = s.request > 0 && s_mock.open[static_cast<size_t>(s.request)];
        if (s.when == When::kOpen && !open) {
            continue;
        }
        if (s.op == Op::kRequest || s.op == Op::kQuestion) {
            s_mock.open[static_cast<size_t>(s.request)] = true;  // the step that opens it
        } else if (s.op == Op::kClose) {
            s_mock.open[static_cast<size_t>(s.request)] = false;
        }
        Play(s);
    }
    if (into >= kDayMs) {
        ++s_mock.loop;
        BeginDay(now);
    }
}

}  // namespace

uint32_t RequestId(uint32_t loop, int n)
{
    return (loop % 1000) * 10 + static_cast<uint32_t>(n) + 1000;
}

const Step *Day(size_t *count)
{
    *count = std::size(kDay);
    return kDay;
}

bool Start()
{
    if (s_mock.timer != nullptr) {
        return true;
    }
    s_mock = {};
    model::SetListeners(OnDecision, OnAnswer);
    model::SetWifiListener(OnWifiAsk);
    model::SetConnectListener(OnConnectAsk);
    if (model::GetWifi().stage == model::WifiStage::kNotJoined && model::GetWifi().fact_count == 0) {
        model::Network studio;
        PutNetwork(studio, "Studio", true, 3);
        model::SetWifi(JoinedWifi(studio));  // the deck starts on its network
    }
    s_mock.timer = lv_timer_create(Tick, kTickMs, nullptr);
    if (s_mock.timer == nullptr) {
        return false;
    }
    BeginDay(lv_tick_get());
    s_mock.started = true;
    return true;
}

void Stop()
{
    if (s_mock.timer != nullptr) {
        lv_timer_delete(s_mock.timer);
        s_mock.timer = nullptr;
    }
}

}  // namespace agent_deck::mock
