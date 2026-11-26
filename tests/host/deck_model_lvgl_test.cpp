// Host test of the agent deck's rules, its model and its mock day (components/agent_deck: deck_rules.h, deck_model.cpp, deck_mock.cpp), on the
// computer with no board: the words (a span, tokens, a reset, the clock, a state), the carousel's page order and wrap, the overview's count
// and when it is a control, the move each state plays, where a request leaves to, who waits and how a row says it; the model's records —
// agents in arrival order, the room made past twelve, the queue refused at a fifth, an answer sent to the link — and its observers; and the day
// the mock plays on LVGL's timer, step by step, loop after loop; the settings, the battery's words, the Wi-Fi record and its asks, and the
// standby page's states (STICKY_AGENT_DECK §3, §4.7–§4.9); and lvgl_display::InPsram (in_psram.h), which keeps the tables in PSRAM on the
// board: its defaults, one object a call, aligned.
// run.sh builds LVGL's sources from managed_components once, into build/host/lvgl, and the sources named below.
// host-sources: components/agent_deck/deck_model.cpp components/agent_deck/deck_mock.cpp
// Build and run: bash tests/host/run.sh deck_model

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../components/agent_deck/deck_mock.h"
#include "../../components/agent_deck/deck_model.h"
#include "../../components/agent_deck/deck_rules.h"
#include "in_psram.h"
#include "lvgl.h"

using namespace agent_deck;
using widgets::MascotMove;

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

template <class F>
std::string Said(F format)
{
    char out[96] = {};
    format(out, sizeof out);
    return out;
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

// --- The words ---------------------------------------------------------------------------------------------------------

void TestWords()
{
    std::printf("words\n");
    const auto span = [](uint32_t s) { return Said([&](char *o, size_t n) { rules::FormatSpan(o, n, s); }); };
    Check(span(0) == "0s" && span(12) == "12s" && span(59) == "59s", "a span under a minute is seconds");
    Check(span(60) == "1m" && span(4 * 60 + 59) == "4m" && span(3599) == "59m", "a minute to an hour is minutes, rounded down");
    Check(span(3600) == "1h" && span(86399) == "23h" && span(86400) == "1d" && span(3 * 86400 + 5) == "3d", "hours, then days");

    const auto tokens = [](uint32_t t) { return Said([&](char *o, size_t n) { rules::FormatTokens(o, n, t); }); };
    Check(tokens(950) == "950" && tokens(0) == "0" && tokens(999) == "999", "tokens under a thousand are whole");
    Check(tokens(1000) == "1k" && tokens(9800) == "9.8k" && tokens(18400) == "18.4k" && tokens(99999) == "99.9k",
          "a thousand up: a tenth, rounded down, none when it is zero");
    Check(tokens(66000) == "66k" && tokens(100000) == "100k" && tokens(126000) == "126k" && tokens(895999) == "895k",
          "from a hundred thousand whole thousands, as the deck's");
    Check(tokens(1200000) == "1.2M" && tokens(2000000) == "2M", "millions");

    const auto reset = [](int32_t m) { return Said([&](char *o, size_t n) { rules::FormatReset(o, n, m); }); };
    Check(reset(72) == "Resets 1h 12m" && reset(5880) == "Resets 4d 2h" && reset(12) == "Resets 12m" && reset(60) == "Resets 1h 0m",
          "a reset is days and hours, hours and minutes, or minutes");
    Check(reset(-1).empty(), "a window not known has no reset line");

    std::tm when{};
    when.tm_year = 2025 - 1900;
    when.tm_mon = 10;
    when.tm_mday = 7;
    when.tm_wday = 5;
    when.tm_hour = 14;
    when.tm_min = 32;
    char clock[40];
    Check(rules::FormatClock(clock, sizeof clock, when) && std::string(clock) == "Fri 7 Nov  2:32 PM", "the clock reads as the deck's");
    when.tm_hour = 0;
    when.tm_min = 5;
    rules::FormatClock(clock, sizeof clock, when);
    Check(std::string(clock) == "Fri 7 Nov  12:05 AM", "midnight is 12:05 AM");
    when.tm_hour = 12;
    rules::FormatClock(clock, sizeof clock, when);
    Check(std::string(clock) == "Fri 7 Nov  12:05 PM", "noon is 12:05 PM");
    std::tm never{};
    never.tm_year = 70;
    Check(!rules::FormatClock(clock, sizeof clock, never) && clock[0] == '\0', "a clock never set says nothing");

    const auto state = [](model::AgentState s, bool limit, uint32_t sec) {
        return Said([&](char *o, size_t n) { rules::FormatState(o, n, s, limit, sec); });
    };
    Check(state(model::AgentState::kWorking, false, 4 * 60) == "Working for 4m", "Working for 4m");
    Check(state(model::AgentState::kNeedsYou, false, 12) == "Needs you for 12s", "Needs you for 12s");
    Check(state(model::AgentState::kDone, true, 11) == "Stopped for 11s", "a turn the limit ended is Stopped, not Done");
    Check(state(model::AgentState::kDone, false, 7200) == "Done for 2h", "Done for 2h");
}

// --- The carousel and what a page says ------------------------------------------------------------------------------------

// The carousel's page order and the count's chip.
void TestRulesPages()
{
    uint16_t out[12];
    {
        const uint16_t ids[] = {1, 2, 3, 4}, parents[] = {0, 1, 0, 3};
        Check(rules::PageOrder(ids, parents, 4, out) == 4 && out[0] == 1 && out[1] == 2 && out[2] == 3 && out[3] == 4,
              "pages in the order they arrived when each session is followed by its sub-agents");
    }
    {
        const uint16_t ids[] = {1, 3, 2, 5}, parents[] = {0, 0, 1, 9};
        const size_t n = rules::PageOrder(ids, parents, 4, out);
        Check(n == 4 && out[0] == 1 && out[1] == 2 && out[2] == 3 && out[3] == 5,
              "a session's sub-agents follow it though they arrived after another session; one with no session comes last");
    }
    Check(rules::TurnTo(0, 1, 4) == 1 && rules::TurnTo(3, 1, 4) == 0 && rules::TurnTo(0, -1, 4) == 3 && rules::TurnTo(0, 1, 1) == 0,
          "pages wrap round: past the last the overview, back from the overview the last");

    rules::Chip chip = rules::ChipFor(1, 2, 3, true, true);
    Check(chip.shown && chip.control && std::string(chip.text) == "1 needs you", "one waiting: the count is a control while a request queues");
    chip = rules::ChipFor(2, 0, 0, true, true);
    Check(std::string(chip.text) == "2 need you", "more than one: 2 need you");
    chip = rules::ChipFor(1, 0, 0, true, false);
    Check(chip.shown && !chip.control, "Needs you with nothing queued is the plain line");
    chip = rules::ChipFor(0, 2, 1, true, false);
    Check(std::string(chip.text) == "2 working" && !chip.control, "working: a reading");
    chip = rules::ChipFor(0, 0, 3, true, false);
    Check(std::string(chip.text) == "3 done", "done");
    chip = rules::ChipFor(0, 0, 0, false, false);
    Check(chip.shown && std::string(chip.text) == "Connecting...", "no agent and no link: Connecting...");
    chip = rules::ChipFor(0, 0, 0, true, false);
    Check(!chip.shown, "no agent with the link up: nothing");
}

// The mascot's moves, where a closed request leaves to, and the needs-you list.
void TestRulesMoves()
{
    uint16_t out[12];
    Check(rules::OverviewMove(1, 2) == MascotMove::kJump && rules::OverviewMove(0, 2) == MascotMove::kWork &&
              rules::OverviewMove(0, 0) == MascotMove::kIdle,
          "the overview's mascot: jump while anyone needs you, else work, else idle");
    using S = model::AgentState;
    Check(rules::AgentMove(S::kWorking, false, false, false) == MascotMove::kWork &&
              rules::AgentMove(S::kNeedsYou, false, false, false) == MascotMove::kNeedsYou,
          "a page's move follows its state");
    Check(rules::AgentMove(S::kDone, false, false, false) == MascotMove::kFinish &&
              rules::AgentMove(S::kDone, false, false, true) == MascotMove::kCelebrate,
          "done: the finish or the celebrate, by the page's coin");
    Check(rules::AgentMove(S::kDone, false, true, false) == MascotMove::kIdle, "a turn the plan's limit ended idles from the start");
    Check(rules::AgentMove(S::kDone, true, false, false) == MascotMove::kIdle && rules::AgentMove(S::kIdle, false, false, false) == MascotMove::kIdle,
          "a previous chat with nothing to do, and an idle one, idle");

    Check(rules::LeaveFor(false, 0) == rules::LeaveTo::kAgentPage && rules::LeaveFor(true, 0) == rules::LeaveTo::kAgentPage,
          "no other request waiting: the carousel on the asking agent's page, from the list or not");
    Check(rules::LeaveFor(false, 2) == rules::LeaveTo::kList, "others waiting: the needs-you list opens");
    Check(rules::LeaveFor(true, 1) == rules::LeaveTo::kBackToList, "opened from the list: back to it");

    const uint16_t queue[] = {3, 5, 3, 6, 5};
    Check(rules::WaitingAgents(queue, 5, out) == 3 && out[0] == 3 && out[1] == 5 && out[2] == 6,
          "each waiting agent once, the one with the oldest request first");
    const auto waiting = [](const char *kind, bool previous, const char *asks, uint32_t s) {
        return Said([&](char *o, size_t n) { rules::FormatWaiting(o, n, kind, previous, asks, s); });
    };
    Check(waiting(nullptr, false, "Bash", 24) == "Bash \xE2\x80\xA2 24s", "a row says what it asks and how long");
    Check(waiting("Sub-agent", false, "Edit", 12) == "Sub-agent \xE2\x80\xA2 Edit \xE2\x80\xA2 12s", "a sub-agent's type first");
    Check(waiting(nullptr, true, "Question", 60) == "previous chat \xE2\x80\xA2 Question \xE2\x80\xA2 1m", "a previous chat's agent says so");
}

void TestRules()
{
    std::printf("carousel, count, moves, leaving, the list\n");
    TestRulesPages();
    TestRulesMoves();
}

// --- The model ---------------------------------------------------------------------------------------------------------------

model::Agent Agent(uint16_t id, uint16_t parent, const char *name, model::AgentState state)
{
    model::Agent a{};
    a.id = id;
    a.parent = parent;
    std::snprintf(a.name, sizeof a.name, "%s", name);
    a.state = state;
    return a;
}

model::Request Request(uint32_t id, uint16_t agent, const char *tool, const char *body)
{
    model::Request r{};
    r.id = id;
    r.agent = agent;
    std::snprintf(r.tool, sizeof r.tool, "%s", tool);
    r.body = body;
    return r;
}

struct Heard {
    std::vector<model::Change> changes;
    int decisions = 0;
    uint32_t decided = 0;
    bool allowed = false;
    int answers = 0;
    uint8_t chosen = 255;
};
Heard s_heard;

void Listen(const model::Change &change, void *) { s_heard.changes.push_back(change); }
void Decision(uint32_t id, bool allow)
{
    ++s_heard.decisions;
    s_heard.decided = id;
    s_heard.allowed = allow;
}
void Answer(uint32_t, uint8_t, uint8_t chosen)
{
    ++s_heard.answers;
    s_heard.chosen = chosen;
}

// Agents arrive, update, leave and close up; past twelve one gives its place.
void TestModelAgents()
{
    model::SetAgent(Agent(1, 0, "alpha", model::AgentState::kWorking));
    model::SetAgent(Agent(2, 1, "Explore", model::AgentState::kWorking));
    model::SetAgent(Agent(3, 0, "beta", model::AgentState::kIdle));
    Check(model::AgentCount() == 3 && model::AgentAt(0)->id == 1 && model::AgentAt(2)->id == 3, "agents stand in the order they arrived");
    Check(s_heard.changes.size() == 3 && s_heard.changes[0].added && s_heard.changes[0].was == model::AgentState::kIdle,
          "a new agent is heard as added");
    model::SetAgent(Agent(1, 0, "alpha", model::AgentState::kNeedsYou));
    Check(model::AgentCount() == 3 && s_heard.changes.back().event == model::Event::kAgent && !s_heard.changes.back().added &&
              s_heard.changes.back().was == model::AgentState::kWorking,
          "a record for a known agent updates it in place and says what it was");
    model::RemoveAgent(2);
    Check(model::AgentCount() == 2 && model::FindAgent(2) == nullptr && model::AgentAt(1)->id == 3, "an agent leaves and the rest close up in order");

    // Twelve agents; a thirteenth takes the oldest idle one's place, else the oldest done one's.
    model::Reset();
    for (uint16_t i = 1; i <= model::kMaxAgents; ++i) {
        model::SetAgent(Agent(i, 0, "x", i == 5 ? model::AgentState::kIdle : i == 7 ? model::AgentState::kDone : model::AgentState::kWorking));
    }
    model::SetAgent(Agent(20, 0, "new", model::AgentState::kWorking));
    Check(model::AgentCount() == model::kMaxAgents && model::FindAgent(5) == nullptr && model::FindAgent(20) != nullptr &&
              model::FindAgent(7) != nullptr,
          "past twelve the oldest idle agent goes first");
    model::SetAgent(Agent(21, 0, "newer", model::AgentState::kWorking));
    Check(model::FindAgent(7) == nullptr && model::FindAgent(21) != nullptr, "then the oldest done one");
    model::SetAgent(Agent(22, 0, "full", model::AgentState::kWorking));
    Check(model::FindAgent(22) == nullptr && model::AgentCount() == model::kMaxAgents, "with none idle or done a new agent is refused");
}

// The queue: requests in arrival order, at most four, closed and answered.
void TestModelQueue()
{
    model::Reset();
    s_heard = {};
    model::SetAgent(Agent(1, 0, "alpha", model::AgentState::kNeedsYou));
    Check(model::OpenRequest(Request(11, 1, "Bash", "ls")) && model::OpenRequest(Request(12, 1, "Edit", "a\nb")), "requests queue");
    Check(!model::OpenRequest(Request(11, 1, "Bash", "again")), "a request id already queued is refused");
    Check(model::OpenRequest(Request(13, 1, "Read", "x")) && model::OpenRequest(Request(14, 1, "Read", "y")), "four queue");
    Check(!model::OpenRequest(Request(15, 1, "Read", "z")), "a fifth is refused and left to the terminal");
    Check(model::QueueCount() == 4 && model::QueueAt(0)->id == 11 && model::QueueAt(3)->id == 14, "the queue is in arrival order, the oldest first");
    Check(model::QueuePosition(13) == 3 && model::QueuePosition(99) == 0, "a request knows its place: 3 of 4");
    Check(model::OldestFor(1)->id == 11 && model::OldestFor(9) == nullptr, "the oldest request an agent has queued");
    Check(model::FindItem(12)->body == "a\nb" && std::string(model::FindItem(12)->tool) == "Edit", "a request keeps its tool and its body");
    model::CloseRequest(11, model::CloseReason::kTimedOut);
    Check(model::QueueCount() == 3 && model::FindItem(11) == nullptr && model::OldestFor(1)->id == 12, "closed, it leaves the queue");
    Check(s_heard.changes.back().event == model::Event::kRequestClosed && s_heard.changes.back().id == 11 &&
              s_heard.changes.back().reason == model::CloseReason::kTimedOut,
          "and the close is heard with its reason");
    model::Answer(12, true);
    Check(s_heard.decisions == 1 && s_heard.decided == 12 && s_heard.allowed && model::FindItem(12) == nullptr,
          "an answer leaves the queue and goes to the link");
    model::Answer(12, false);
    Check(s_heard.decisions == 1, "an answer for one that is gone is nothing");
}

// A question queues with its options and is answered by a bit for each.
void TestModelQuestion()
{
    model::Question q{};
    q.id = 30;
    q.agent = 1;
    q.part = 1;
    std::snprintf(q.tool, sizeof q.tool, "Plan 2/2");
    q.question = "Which?";
    q.options[0] = "a: first";
    q.options[1] = "b";
    q.option_count = 2;
    model::CloseRequest(13, model::CloseReason::kTimedOut);
    Check(model::OpenQuestion(q) && model::FindItem(30)->is_question && model::FindItem(30)->options[0] == "a: first", "a question queues with its options");
    model::AnswerQuestion(30, 1, 3);
    Check(s_heard.answers == 1 && s_heard.chosen == 3 && model::FindItem(30) == nullptr, "its answer is a bit for each option chosen");
    q.id = 31;
    q.option_count = 0;
    Check(!model::OpenQuestion(q), "a question with no option is refused");
}

// Forgetting everything closes what is open as timed out; the link and the observers.
void TestModelObservers(int handle)
{
    s_heard.changes.clear();
    model::Reset();
    bool timed_out = false;
    for (const model::Change &c : s_heard.changes) {
        timed_out = timed_out || (c.event == model::Event::kRequestClosed && c.reason == model::CloseReason::kTimedOut);
    }
    Check(timed_out && model::AgentCount() == 0 && model::QueueCount() == 0 && !model::UsageKnown(), "a reset closes the queue as timed out and forgets the rest");

    model::SetLink(true);
    Check(model::LinkUp() && s_heard.changes.back().event == model::Event::kLink, "the link is told");
    const size_t seen = s_heard.changes.size();
    model::SetLink(true);
    Check(s_heard.changes.size() == seen, "the same link again says nothing");
    model::Unsubscribe(handle);
    model::SetLink(false);
    Check(s_heard.changes.size() == seen, "an unsubscribed screen hears nothing");
    int taken = 0;
    for (size_t i = 0; i <= model::kMaxObservers; ++i) {
        taken += model::Subscribe(Listen, nullptr) > 0;
    }
    Check(taken == static_cast<int>(model::kMaxObservers), "the observers are few, and a full list says so");
    for (int h = 1; h <= static_cast<int>(model::kMaxObservers); ++h) {
        model::Unsubscribe(h);
    }
}

void TestModel()
{
    std::printf("model\n");
    model::Reset();
    s_heard = {};
    model::SetListeners(Decision, Answer);
    const int handle = model::Subscribe(Listen, nullptr);
    Check(handle > 0, "a screen can subscribe");
    TestModelAgents();
    TestModelQueue();
    TestModelQuestion();
    TestModelObservers(handle);
    model::SetListeners(nullptr, nullptr);
    model::Reset();
}

// --- Settings, the battery, Wi-Fi and standby -------------------------------------------------------------------------------------

int s_wifi_asks = 0;
model::WifiAsk s_wifi_ask = model::WifiAsk::kConnect;
int s_wifi_arg = -1;
int s_connects = 0;

// The battery's words, the time zones and the standby page's words.
void TestSettingsWords()
{
    // The battery's words: the charger first, then the deck's own from 80 % and 20 %.
    const auto battery = [](int pct, bool charging) { return Said([&](char *o, size_t n) { rules::FormatBattery(o, n, pct, charging); }); };
    Check(battery(84, false) == "Charged \xE2\x80\xA2 84%", "off the charger at 84 %: Charged \xE2\x80\xA2 84%");
    Check(battery(62, true) == "Charging \xE2\x80\xA2 62%", "on it at 62 %: Charging \xE2\x80\xA2 62%");
    Check(battery(84, true) == "Charging \xE2\x80\xA2 84%", "on the charger the word is Charging whatever the percent");
    Check(battery(50, false) == "On battery \xE2\x80\xA2 50%" && battery(12, false) == "Low battery \xE2\x80\xA2 12%", "off it under 80 % and under 20 %");
    Check(battery(-1, false) == "--%" && battery(140, false) == "Charged \xE2\x80\xA2 100%", "no reading, and a reading past the top");
    Check(std::strcmp(rules::kTimeZones[0], "Pacific") == 0 && std::strcmp(rules::kTimeZones[14], "Australia West") == 0 && model::kTimeZones == 15,
          "fifteen time zones, Pacific first");
    Check(std::string(rules::StandbyChip(model::StandbyPage::kStandby)) == "On standby" &&
              std::string(rules::StandbyChip(model::StandbyPage::kConnecting)) == "Connecting" &&
              std::string(rules::StandbyChip(model::StandbyPage::kTimedOut)) == "On standby" &&
              std::string(rules::StandbyChip(model::StandbyPage::kRefused)) == "On standby",
          "the chip says On standby, Connecting while it waits");
    Check(std::string(rules::StandbyWords(model::StandbyPage::kStandby)) == "Another deck is active." &&
              std::string(rules::StandbyWords(model::StandbyPage::kConnecting)) == "Asking your Mac to make this deck active." &&
              std::string(rules::StandbyWords(model::StandbyPage::kTimedOut)) == "Your Mac did not answer. Try again." &&
              std::string(rules::StandbyWords(model::StandbyPage::kRefused)) == "Another deck is on the cable.",
          "the line under the mascot, one for each state");
}

// Settings and the battery's reading.
void TestSettingsValues()
{
    // Settings: Auto sleep in fives between 5 and 60, a zone in the list, the switch.
    Check(model::AutoSleepMinutes() == 30 && model::TimeZone() == 0 && model::WifiOn(), "30 min, Pacific and Wi-Fi on at first");
    model::SetAutoSleepMinutes(43);
    Check(model::AutoSleepMinutes() == 45 && s_heard.changes.back().event == model::Event::kSettings, "a value off the list reads as the nearest: 43 is 45");
    model::SetAutoSleepMinutes(1);
    Check(model::AutoSleepMinutes() == 5, "never under 5");
    model::SetAutoSleepMinutes(99);
    Check(model::AutoSleepMinutes() == 60, "never over 60");
    model::SetAutoSleepMinutes(30);
    model::SetTimeZone(99);
    Check(model::TimeZone() == 14, "a zone past the list is the last");
    model::SetTimeZone(0);
    model::SetBattery({84, false});
    Check(model::GetBattery().percent == 84 && s_heard.changes.back().event == model::Event::kBattery, "a battery reading is heard");
    const size_t heard = s_heard.changes.size();
    model::SetBattery({84, false});
    Check(s_heard.changes.size() == heard, "the same reading again says nothing");
}

void TestSettingsWifi()
{
    // Wi-Fi: the switch tells the link; the screens' asks reach it; the record is the link's.
    s_wifi_asks = 0;
    model::SetWifiListener([](model::WifiAsk ask, int arg) {
        ++s_wifi_asks;
        s_wifi_ask = ask;
        s_wifi_arg = arg;
    });
    model::SetWifiOn(false);
    Check(!model::WifiOn() && s_wifi_asks == 1 && s_wifi_ask == model::WifiAsk::kSwitch && s_wifi_arg == 0, "the switch off tells the link");
    model::SetWifiOn(true);
    Check(s_wifi_arg == 1, "and on");
    model::AskWifi(model::WifiAsk::kPick, 2);
    Check(s_wifi_ask == model::WifiAsk::kPick && s_wifi_arg == 2, "a screen's ask reaches the link with its number");
    model::Wifi wifi;
    wifi.stage = model::WifiStage::kJoined;
    std::snprintf(wifi.network.name, sizeof wifi.network.name, "Studio");
    model::SetWifi(wifi);
    Check(model::GetWifi().stage == model::WifiStage::kJoined && s_heard.changes.back().event == model::Event::kWifi, "the record the link writes is read back and heard");
}

void TestSettingsStandby()
{
    // Standby (AGENT_DECK 4.13).
    model::SetConnectListener([]() { ++s_connects; });
    model::SetAgent([] { model::Agent a; a.id = 1; std::snprintf(a.name, sizeof a.name, "web-shop"); return a; }());
    model::Request request;
    request.id = 77;
    request.agent = 1;
    std::snprintf(request.tool, sizeof request.tool, "Bash");
    request.body = "ls";
    model::OpenRequest(request);
    s_heard = {};
    model::SetStandby(true);
    bool silent = false;
    for (const model::Change &c : s_heard.changes) {
        silent = silent || (c.event == model::Event::kRequestClosed && c.id == 77 && c.reason == model::CloseReason::kStandby);
    }
    Check(model::Standby() == model::StandbyPage::kStandby && model::AgentCount() == 0 && model::QueueCount() == 0 && !model::UsageKnown() && silent &&
              s_heard.changes.back().event == model::Event::kStandby,
          "standby forgets the agents, closes a request with no timed-out word, and is heard");
    model::StandbyTick();
    Check(model::Standby() == model::StandbyPage::kStandby, "a tick on standby changes nothing");
    model::ConnectPressed();
    Check(model::Standby() == model::StandbyPage::kConnecting && s_connects == 1, "Connect: connecting, the link asked");
    model::ConnectPressed();
    Check(s_connects == 1, "Connect while connecting does nothing");
    s_ms += model::kConnectWaitMs - 100;
    model::StandbyTick();
    Check(model::Standby() == model::StandbyPage::kConnecting, "14.9 s on it is still waiting");
    s_ms += 200;
    model::StandbyTick();
    Check(model::Standby() == model::StandbyPage::kTimedOut, "15 s with no snapshot: standby again, timed out");
    model::ConnectPressed();
    Check(model::Standby() == model::StandbyPage::kConnecting && s_connects == 2, "Connect again from timed out");
    model::SetStandby(true);
    Check(model::Standby() == model::StandbyPage::kRefused, "a standby while connecting: refused");
    model::ConnectPressed();
    model::Reset();
    Check(model::Standby() == model::StandbyPage::kNone && s_heard.changes.back().event == model::Event::kStandby, "the snapshot's reset takes it off, and says so");
    model::SetStandby(true);
    model::SetStandby(false);
    Check(model::Standby() == model::StandbyPage::kNone, "standby false is off");
}

void TestSettingsModel()
{
    std::printf("settings, battery, Wi-Fi and standby\n");
    TestSettingsWords();
    model::Reset();
    s_heard = {};
    const int handle = model::Subscribe(Listen, nullptr);
    TestSettingsValues();
    TestSettingsWifi();
    TestSettingsStandby();
    model::Unsubscribe(handle);
    model::SetWifiListener(nullptr);
    model::SetConnectListener(nullptr);
}

// --- Where the model's tables live: lvgl_display::InPsram, which holds them in PSRAM on the board ------------------------------

struct Table {
    bool used = false;
    int32_t room = -1;
    std::array<uint32_t, 3> keys = {7, 8, 9};
};

void TestInPsram()
{
    std::printf("the tables in PSRAM\n");
    Table &a = lvgl_display::InPsram<Table>();
    Table &b = lvgl_display::InPsram<Table>();
    Check(!a.used && a.room == -1 && a.keys[2] == 9, "a table made in PSRAM holds its type's defaults, as a static of it would");
    Check(&a != &b, "each call makes its own table");
    Check(reinterpret_cast<uintptr_t>(&lvgl_display::InPsram<std::array<uint64_t, 2>>()) % alignof(uint64_t) == 0, "aligned for its type");
    a.room = 3;
    Check(b.room == -1, "and one written leaves the other as it was");
}

// --- The mock day ------------------------------------------------------------------------------------------------------------

void TestDayData()
{
    std::printf("the day's steps\n");
    size_t n = 0;
    const mock::Step *day = mock::Day(&n);
    bool ordered = true;
    for (size_t i = 1; i < n; ++i) {
        ordered = ordered && day[i].at_ms >= day[i - 1].at_ms;
    }
    Check(n > 40 && ordered, "the steps stand in time order");
    Check(day[n - 1].at_ms < mock::kDayMs, "the day ends inside its loop");
    int opens[8] = {}, closes[8] = {};
    for (size_t i = 0; i < n; ++i) {
        if (day[i].op == mock::Op::kRequest || day[i].op == mock::Op::kQuestion) {
            ++opens[day[i].request];
        }
        if (day[i].op == mock::Op::kClose) {
            ++closes[day[i].request];
            Check(day[i].when == mock::When::kOpen, "a close is sent only while its request still waits");
        }
    }
    bool paired = true;
    for (int r = 1; r <= 7; ++r) {
        paired = paired && opens[r] == 1 && closes[r] == 1;
    }
    Check(paired, "each of the day's seven requests opens once and has the close the bridge would send");
    Check(mock::RequestId(0, 1) != mock::RequestId(1, 1) && mock::RequestId(5, 7) != mock::RequestId(6, 7) && mock::RequestId(0, 1) >= 1000,
          "a request's id is another on the next loop, so one day never answers another's");
    // No made-up name is a real one: the day's folders and hosts are the deck's own examples.
    for (size_t i = 0; i < n; ++i) {
        if (day[i].op == mock::Op::kAgent) {
            Check(std::strstr(day[i].agent.name, "/") == nullptr, std::string("an agent's name holds no path: ") + day[i].agent.name);
        }
    }
}

// The day's start to 88.5 s: the first request allowed, the question left to time out, request 3 denied.
void TestDayRequests()
{
    Check(mock::Start(), "the mock's timer starts");
    Check(mock::Start(), "and starting again is nothing");
    Advance(300);
    Check(model::LinkUp() && model::UsageKnown() && model::GetUsage().five_hour_pct == 23, "the link is up and the usage is in at the start");
    Check(model::AgentCount() == 1 && std::string(model::AgentAt(0)->name) == "agent-deck-studio", "the first agent arrives at 200 ms");
    Advance(18000 - 300 + 200);
    Check(model::QueueCount() == 1 && model::QueueAt(0)->id == mock::RequestId(0, 1) && std::string(model::QueueAt(0)->tool) == "Bash",
          "at 18.1 s web-shop asks to run Bash");
    Check(model::FindAgent(3)->state == model::AgentState::kNeedsYou, "and its agent needs you");
    const uint32_t first = model::QueueAt(0)->id;

    // Allowed on the deck: the agent goes back to work a tick later.
    model::Answer(first, true);
    Check(model::QueueCount() == 0, "the answer leaves the queue");
    Advance(300);
    Check(model::FindAgent(3)->state == model::AgentState::kWorking && std::string(model::FindAgent(3)->detail) == "Bash: npm run deploy" &&
              model::FindAgent(3)->context_pct == 36,
          "an allowed request: the agent is back at work on the same tool");
    Advance(48000 - 18600);
    bool closed_heard = false;
    for (const model::Change &c : s_heard.changes) {
        closed_heard = closed_heard || (c.event == model::Event::kRequestClosed && c.id == first);
    }
    Check(!closed_heard, "an answered request is never closed by the day's timeout");

    // The question at 32.1 s was left alone: the bridge's hold runs out at 58 s.
    Advance(58100 - 48000 + 100);
    bool question_closed = false;
    for (const model::Change &c : s_heard.changes) {
        question_closed = question_closed || (c.event == model::Event::kRequestClosed && c.id == mock::RequestId(0, 2) &&
                                              c.reason == model::CloseReason::kTimedOut);
    }
    Check(question_closed, "a question nobody answered times out at 58 s");

    // Request 3 at 88.1 s: denied this time.
    Advance(88200 - 58200);
    Check(model::QueueCount() == 1 && std::string(model::QueueAt(0)->tool) == "Edit", "at 88.1 s agent-deck-studio asks to Edit");
    model::Answer(model::QueueAt(0)->id, false);
    Advance(300);
    Check(model::FindAgent(1)->state == model::AgentState::kWorking && std::string(model::FindAgent(1)->detail) == "Thinking",
          "a denied request: the agent thinks on");
}

// The burst, standby, and the loop round to the next day.
void TestDayEnd()
{
    // The burst, left alone, and the day's end.
    Advance(115700 - 88500);
    Check(model::QueueCount() == 4, "four agents ask within two seconds and the queue holds them");
    Check(model::OldestFor(3) != nullptr && model::QueueAt(0)->agent == 3, "the oldest of them is web-shop's");
    Advance(150100 - 115700);
    Check(model::QueueCount() == 0, "left alone, the bridge's hold runs out at 150 s and the four close");
    // The battery: on the charger at 62 % at the start, 73 % at 80 s, off it at 84 % from 110 s.
    Check(model::GetBattery().percent == 84 && !model::GetBattery().charging, "the battery reads Charged at 84 % from 110 s");
    Advance(158100 - 150100);
    Check(model::Standby() == model::StandbyPage::kStandby && model::AgentCount() == 0, "at 158 s the bridge puts the deck on standby");
    model::ConnectPressed();
    Advance(2900);
    Check(model::Standby() == model::StandbyPage::kConnecting, "Connect waits for the Mac");
    Advance(400);
    Check(model::Standby() == model::StandbyPage::kNone, "which answers in 3 s with the snapshot: the day begins again");
    Advance(500);
    Check(model::GetBattery().percent == 62 && model::GetBattery().charging, "the day's first battery reading is the charger's 62 %");
    Check(model::AgentCount() == 1 && std::string(model::AgentAt(0)->name) == "agent-deck-studio",
          "the next day's first agent arrives");
    // Left alone on standby, the loop's own snapshot ends it.
    Advance(158000 - 500 - 400 + 100);
    Check(model::Standby() == model::StandbyPage::kStandby, "the next day's standby");
    Advance(mock::kDayMs - 158000 + 400);
    Check(model::Standby() == model::StandbyPage::kNone && model::AgentCount() == 1, "left alone, the loop's snapshot ends it");
    Advance(18500);
    Check(model::QueueCount() == 1 && model::QueueAt(0)->id == mock::RequestId(2, 1), "and the first request of the loop after that has its own id: the Mac's snapshot began one, the loop's end another");
}

void TestDayPlays()
{
    std::printf("the day played on the timer\n");
    model::Reset();
    s_heard = {};
    const int handle = model::Subscribe(Listen, nullptr);
    TestDayRequests();
    TestDayEnd();
    mock::Stop();
    model::Unsubscribe(handle);
    model::Reset();
}

}  // namespace

int main()
{
    lv_init();
    lv_tick_set_cb(Ticks);
    TestWords();
    TestRules();
    TestModel();
    TestSettingsModel();
    TestDayData();
    TestDayPlays();
    TestInPsram();
    std::printf("%d checks, %d failed\n", s_checks, s_failures);
    return s_failures == 0 ? 0 : 1;
}
