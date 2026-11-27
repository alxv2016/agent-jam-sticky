// Host test of the agent deck's link to the bridge (components/agent_deck/deck_link.cpp) against the real model (deck_model.cpp), on the
// computer with no board: the line's CRC and framing, the hello, each record the bridge sends landing in the model, the lines dropped (a
// wrong CRC, deep JSON, an unknown type), the bridge's hello holding the connected cue until `reset` or `standby` says this deck's role,
// decisions and answers sent and sent again until their ack, a question with no room answered for the terminal, the bridge lost after
// kLostMs with standby kept, and Connect's `use` (STICKY_AGENT_DECK §10).
// run.sh builds LVGL's sources from managed_components once, into build/host/lvgl, and the sources named below.
// host-sources: components/agent_deck/deck_model.cpp components/agent_deck/deck_link.cpp managed_components/espressif__cjson/cJSON/cJSON.c
// Build and run: bash tests/host/run.sh deck_link

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../components/agent_deck/deck_link.h"
#include "../../components/agent_deck/deck_model.h"
#include "lvgl.h"

using namespace agent_deck;

namespace {

int s_failures = 0;
int s_checks = 0;

void Check(bool ok, const std::string &what)
{
    ++s_checks;
    if (!ok) {
        ++s_failures;
        std::printf("FAIL  %s\n", what.c_str());
    }
}

uint32_t s_ms = 100000;
uint32_t Ticks() { return s_ms; }
void Advance(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 10) {
        s_ms += 10;
        lv_timer_handler();
    }
}

// What the deck wrote to the cable, one line each.
std::vector<std::string> s_sent;
bool Send(const char *line, size_t len)
{
    s_sent.emplace_back(line, len);
    return true;
}

// The model's changes, as a screen hears them.
std::vector<model::Event> s_events;
void Listen(const model::Change &change, void *) { s_events.push_back(change.event); }

bool Saw(model::Event event)
{
    for (model::Event e : s_events) {
        if (e == event) {
            return true;
        }
    }
    return false;
}

// One record from the bridge, framed as the bridge frames it: the prefix stripped, as uart_line hands it on.
bool Feed(const std::string &json)
{
    char line[link::kMaxShortOut * 8];
    const size_t n = link::Encode(line, sizeof(line), json.c_str());
    const size_t prefix = std::strlen(link::kPrefix);
    return n > prefix && link::Take(line + prefix, n - prefix);
}

bool SentHas(const std::string &words)
{
    for (const std::string &s : s_sent) {
        if (s.find(words) != std::string::npos) {
            return true;
        }
    }
    return false;
}

size_t SentCount(const std::string &words)
{
    size_t n = 0;
    for (const std::string &s : s_sent) {
        n += s.find(words) != std::string::npos;
    }
    return n;
}

void TestFraming()
{
    std::printf("framing\n");
    const char *ping = R"({"type":"ping"})";
    Check(link::Crc32(ping, std::strlen(ping)) == 0x497c309cu, "the CRC is zlib's: the bridge's own ping is 497c309c");
    char line[64];
    const size_t n = link::Encode(line, sizeof(line), ping);
    Check(n > 0 && std::string(line, n) == "@deck 497c309c {\"type\":\"ping\"}", "a line is the prefix, the CRC, a space and the JSON");
    Check(link::Encode(line, 10, ping) == 0, "a line that does not fit is not made");
}

void TestHello()
{
    std::printf("the hello\n");
    Check(!link::Init(Send, {"3F2A", "sticky", "usb"}), "a hello field outside a-z, 0-9 and '-' is refused");
    Check(!link::Init(Send, {"3f2a", "reterminal-sticky", "usb"}), "and one past 16 characters");
    s_sent.clear();
    Check(link::Init(Send, {"3f2a", "sticky", "usb"}), "a good hello starts the link");
    Check(s_sent.size() == 1 && SentHas(R"("type":"hello","version":5,"id":"3f2a","board":"sticky","link":"usb","body":16384)"),
          "and says hello at once: version 5, the deck's id, board and link, the 16 KB body it takes");
    Check(!link::Up() && !model::LinkUp(), "no bridge heard yet: the link is down");
}

void TestRecords()
{
    std::printf("records\n");
    s_events.clear();
    Check(Feed(R"({"type":"agent","id":1,"parent":0,"name":"Claude Code","state":"working","since_s":12,"detail":"Bash: npm test","context_pct":8,"tokens":4200,"model":"Opus"})"),
          "an agent record is taken");
    const model::Agent *a = model::FindAgent(1);
    Check(a != nullptr && std::strcmp(a->name, "Claude Code") == 0 && a->state == model::AgentState::kWorking && a->since_s == 12 &&
              a->context_pct == 8 && a->tokens == 4200 && std::strcmp(a->detail, "Bash: npm test") == 0,
          "and lands in the model with its fields");
    Check(link::Up() && model::LinkUp() && Saw(model::Event::kLinkAnnounced), "the first good line brings the link up, announced at once");
    Feed(R"({"type":"agent","id":2,"parent":1,"name":"Explore","kind":"Explore","state":"needs_you","active":false,"limit":"five_hour"})");
    a = model::FindAgent(2);
    Check(a != nullptr && a->parent == 1 && a->background && a->limit == model::Limit::kFiveHour && a->state == model::AgentState::kNeedsYou,
          "a sub-agent of a previous chat, needing you, its turn ended by the five-hour limit");
    Feed(R"({"type":"usage","five_hour_pct":96,"five_hour_resets_min":120,"seven_day_pct":58,"seven_day_resets_min":4304,"limit":"unknown"})");
    const model::Usage &u = model::GetUsage();
    Check(model::UsageKnown() && u.five_hour_pct == 96 && u.seven_day_resets_min == 4304 && u.limit == model::Limit::kUnknown, "usage");
    Feed(R"({"type":"request","id":7,"agent":1,"tool":"Bash","body":"npm test\n  -- --watch=false","cut":false})");
    const model::Item *r = model::FindItem(7);
    Check(r != nullptr && !r->is_question && r->body == "npm test\n  -- --watch=false" && std::strcmp(r->tool, "Bash") == 0, "a request queued");
    Feed(R"({"type":"question","id":8,"agent":1,"part":0,"tool":"Branch 1/2","question":"Which base branch?","multi":false,"options":["main: The release branch","develop"]})");
    const model::Item *q = model::FindItem(8);
    Check(q != nullptr && q->is_question && q->option_count == 2 && q->options[1] == "develop", "a question queued with its options");
    Feed(R"({"type":"close","id":7,"reason":"timed_out"})");
    Check(model::FindItem(7) == nullptr, "a close takes the request off the queue");
    Feed(R"({"type":"remove","id":2})");
    Check(model::FindAgent(2) == nullptr, "a remove takes the agent off the deck");
}

void TestDropped()
{
    std::printf("lines dropped\n");
    const size_t agents = model::AgentCount();
    char line[128];
    const char *json = R"({"type":"remove","id":1})";
    const size_t n = link::Encode(line, sizeof(line), json);
    line[n - 3] = '2';  // the JSON changed under its CRC
    const size_t prefix = std::strlen(link::kPrefix);
    Check(!link::Take(line + prefix, n - prefix) && model::AgentCount() == agents, "a wrong CRC is dropped");
    Check(!Feed(R"({"type":"agent","id":1,"x":{"y":{"z":1}}})") && model::FindAgent(1) != nullptr, "JSON deeper than two levels is dropped");
    Check(!Feed(R"({"type":"mystery"})"), "an unknown type is dropped");
    Check(!link::Take("short", 5), "a line too short to hold a CRC is dropped");
}

void TestRole()
{
    std::printf("the bridge's hello and the deck's role\n");
    s_sent.clear();
    s_events.clear();
    model::SetLink(false);
    Check(Feed(R"({"type":"hello"})") && SentHas(R"("type":"hello")"), "the bridge's hello: the deck says hello back");
    model::SetLink(true);
    Check(!Saw(model::Event::kLinkAnnounced), "after it, the link up is not announced: the deck waits to hear its role");
    Feed(R"({"type":"standby"})");
    Check(model::Standby() == model::StandbyPage::kStandby && !Saw(model::Event::kLinkAnnounced), "standby: the standby page, and no cue");
    Feed(R"({"type":"reset"})");
    Check(model::Standby() == model::StandbyPage::kNone && Saw(model::Event::kLinkAnnounced),
          "the snapshot's reset takes the deck off standby and announces the link");
    Feed(R"({"type":"ping"})");
    Advance(2000);
    Feed(R"({"type":"reset"})");
    Check(model::LinkedMs() == lv_tick_get(), "a reset to a deck long active starts the snapshot's quiet again: no boop for each agent resent");
}

void TestAnswers()
{
    std::printf("answers out\n");
    Feed(R"({"type":"reset"})");
    Feed(R"({"type":"agent","id":1,"parent":0,"name":"Claude Code","state":"needs_you"})");
    Feed(R"({"type":"request","id":21,"agent":1,"tool":"Bash","body":"ls"})");
    s_sent.clear();
    model::Answer(21, true);
    Check(SentCount(R"({"type":"decision","id":21,"allow":true})") == 1, "a decision goes out at once");
    for (int i = 0; i < 3; ++i) {
        Feed(R"({"type":"ping"})");
        Advance(1000);
    }
    Check(SentCount(R"("type":"decision","id":21)") >= 3, "and again every second until the bridge acknowledges it");
    Feed(R"({"type":"ack","id":21})");
    const size_t after_ack = SentCount(R"("type":"decision","id":21)");
    Feed(R"({"type":"ping"})");
    Advance(2000);
    Check(SentCount(R"("type":"decision","id":21)") == after_ack, "an ack stops the resends");

    Feed(R"({"type":"question","id":22,"agent":1,"part":0,"tool":"Plan 1/2","question":"Which?","options":["a","b"]})");
    model::AnswerQuestion(22, 0, 2);
    Check(SentHas(R"({"type":"answer","id":22,"part":0,"chosen":2})"), "a question's answer goes out with its part and the bits chosen");
    s_sent.clear();
    for (int i = 0; i < 12; ++i) {
        Feed(R"({"type":"ping"})");
        Advance(1000);
    }
    Check(SentCount(R"("type":"answer","id":22)") == link::kMaxSends - 1, "unacknowledged, it is sent kMaxSends times in all, then let go");

    // A later part replaces an earlier one's resends, even with a free slot ahead of it.
    Feed(R"({"type":"reset"})");
    Feed(R"({"type":"agent","id":1,"parent":0,"name":"Claude Code","state":"needs_you"})");
    Feed(R"({"type":"request","id":24,"agent":1,"tool":"Bash","body":"ls"})");
    Feed(R"({"type":"question","id":25,"agent":1,"part":0,"tool":"Plan 1/2","question":"Which?","options":["a","b"]})");
    model::Answer(24, false);
    model::AnswerQuestion(25, 0, 1);
    Check(SentHas(R"({"type":"decision","id":24,"allow":false})") && SentHas(R"({"type":"answer","id":25,"part":0,"chosen":1})"),
          "a decision, then a question's first part, each sent");
    Feed(R"({"type":"ack","id":24})");
    Feed(R"({"type":"question","id":25,"agent":1,"part":1,"tool":"Plan 2/2","question":"And?","options":["c","d"]})");
    model::AnswerQuestion(25, 1, 2);
    s_sent.clear();
    Feed(R"({"type":"ping"})");
    Advance(1000);
    Check(SentCount(R"({"type":"answer","id":25,"part":1,"chosen":2})") == 1 && SentCount(R"("id":25,"part":0)") == 0,
          "part 1's answer resends in place of part 0's");
    Feed(R"({"type":"ack","id":25,"part":1})");

    // A queue of four is full: a fifth question has no room, and the terminal is told at once.
    for (uint32_t id = 31; id <= 34; ++id) {
        Feed(std::string(R"({"type":"request","id":)") + std::to_string(id) + R"(,"agent":1,"tool":"Bash","body":"x"})");
    }
    s_sent.clear();
    Feed(R"({"type":"question","id":35,"agent":1,"part":0,"tool":"Q","question":"?","options":["a"]})");
    Check(model::FindItem(35) == nullptr && SentHas(R"({"type":"answer","id":35,"part":0,"chosen":0})"),
          "a question with no room is answered 0: left to the terminal");
}

void TestLost()
{
    std::printf("the bridge lost\n");
    Feed(R"({"type":"reset"})");
    Feed(R"({"type":"agent","id":1,"parent":0,"name":"Claude Code","state":"working"})");
    Feed(R"({"type":"standby"})");
    s_sent.clear();
    Advance(link::kLostMs + 500);
    Check(!link::Up() && !model::LinkUp() && model::AgentCount() == 0, "no line for kLostMs: the link is down and what it said forgotten");
    Check(model::Standby() == model::StandbyPage::kStandby, "standby kept: only a reset takes the deck off it");
    Check(SentHas(R"("type":"hello")"), "and the deck says hello again");
    Feed(R"({"type":"reset"})");
    s_sent.clear();
    Check(link::Use() && SentHas(R"({"type":"use"})"), "Connect's use goes out once");
}

}  // namespace

int main()
{
    lv_init();
    lv_tick_set_cb(Ticks);
    model::Subscribe(Listen, nullptr);
    model::SetListeners(link::Decided, link::Answered);
    TestFraming();
    TestHello();
    TestRecords();
    TestDropped();
    TestRole();
    TestAnswers();
    TestLost();
    std::printf("%d checks, %d failed\n", s_checks, s_failures);
    return s_failures == 0 ? 0 : 1;
}
