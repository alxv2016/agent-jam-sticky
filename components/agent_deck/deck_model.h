// The agent deck's UI-side model: the records its screens show, kept in the shape the link will call — SetAgent, RemoveAgent,
// SetUsage, OpenRequest, OpenQuestion, CloseRequest, SetLink — so the logic port swaps the mock day (deck_mock.h) for the link without
// touching a screen (STICKY_AGENT_DECK §3). The records are the agent deck's own ("AGENT_DECK n" in this component is the deck's own doc, section n, in
// its repo, as widgets/mascot.h cites it; AGENT_DECK 6 is its data): plain data, ASCII with ° and •.
// Every call runs under the LVGL lock, on LVGL's task; a screen subscribes to the changes while it is on top and reads the model again
// when one comes, or when it enters.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace agent_deck::model {

inline constexpr size_t kMaxAgents = 12;    // page dots' 13 less the overview
inline constexpr size_t kMaxRequests = 4;   // queued; a fifth is refused and left to the terminal
inline constexpr size_t kMaxOptions = 4;    // AskUserQuestion's own limit
inline constexpr size_t kNameChars = 40;
inline constexpr size_t kDetailChars = 64;
inline constexpr size_t kModelChars = 32;
inline constexpr size_t kKindChars = 32;
inline constexpr size_t kToolChars = 48;
inline constexpr size_t kBodyBytes = 16 * 1024;
inline constexpr size_t kOptionChars = 400;
inline constexpr size_t kQuestionChars = 400;

enum class AgentState : uint8_t { kWorking, kNeedsYou, kDone, kIdle };

// The plan's limit that ended a turn, as the bridge read it.
enum class Limit : uint8_t { kNone, kUnknown, kFiveHour, kSevenDay };

struct Agent {
    uint16_t id = 0;      // non-zero
    uint16_t parent = 0;  // 0 for a session; a sub-agent's session otherwise
    char name[kNameChars + 1] = {};   // a session's folder; a sub-agent's task
    char kind[kKindChars + 1] = {};   // a sub-agent's type; empty for a session
    AgentState state = AgentState::kIdle;
    bool background = false;  // a previous chat's: its mascot is grey
    uint32_t since_s = 0;     // in this state, as of the record; the screens count it on from the record's arrival
    char detail[kDetailChars + 1] = {};  // the last tool and its subject: `Bash: npm test`
    int8_t context_pct = -1;  // -1: unknown, or a sub-agent
    uint32_t tokens = 0;      // 0 when Claude Code has not said
    char model[kModelChars + 1] = {};
    Limit limit = Limit::kNone;
};

// Each -1 when the status line carries no plan limits.
struct Usage {
    int8_t five_hour_pct = -1;
    int32_t five_hour_resets_min = -1;
    int8_t seven_day_pct = -1;
    int32_t seven_day_resets_min = -1;
    Limit limit = Limit::kNone;
};

struct Request {
    uint32_t id = 0;     // non-zero
    uint16_t agent = 0;  // the agent asking
    char tool[kToolChars + 1] = {};
    const char *body = "";  // copied; up to kBodyBytes — a longer one is cut here and marked
    bool cut = false;
};

struct Question {
    uint32_t id = 0;     // non-zero; a request's id, shared by every part of one call
    uint16_t agent = 0;
    uint8_t part = 0;
    char tool[kToolChars + 1] = {};  // the question's header with its place in the call: `Plan 2/2`
    const char *question = "";
    const char *options[kMaxOptions] = {};  // `label: description`, copied
    uint8_t option_count = 0;
    bool multi = false;  // any number of answers, else one
};

// kStandby: the bridge put this deck on standby, which closes a request on the glass with no page and no cue (AGENT_DECK 4.13).
enum class CloseReason : uint8_t { kAnsweredElsewhere, kTimedOut, kStandby };

// What waits in the queue, a request or a question.
struct Item {
    bool used = false;
    bool is_question = false;
    uint32_t id = 0;
    uint8_t part = 0;
    uint16_t agent = 0;
    char tool[kToolChars + 1] = {};
    std::string body;                 // a request's text
    bool cut = false;
    std::string question;             // a question's text
    std::array<std::string, kMaxOptions> options;
    uint8_t option_count = 0;
    bool multi = false;
    uint32_t seq = 0;                 // arrival order: the oldest has the lowest
    uint32_t opened_ms = 0;           // lv_tick_get() when it was queued
};

// What a change was, for a screen that heard it.
enum class Event : uint8_t {
    kAgent,           // an agent added or updated; id is the agent's
    kAgentRemoved,    // id is the agent's
    kUsage,
    kRequestOpened,   // a request or question queued; id is its id
    kRequestClosed,   // closed elsewhere or timed out; id, reason
    kRequestAnswered, // answered on the deck; id, allow
    kLink,
    kSound,           // the sound setting changed
    kSettings,        // Auto sleep, the time zone or the Wi-Fi switch changed
    kBattery,
    kWifi,            // the Wi-Fi record changed
    kStandby,         // the standby page's state changed
};
struct Change {
    Event event;
    uint32_t id = 0;
    CloseReason reason = CloseReason::kAnsweredElsewhere;
    bool allow = false;
    // For kAgent: the state it had before this record (kIdle for a new agent), and whether it is new.
    AgentState was = AgentState::kIdle;
    bool added = false;
};

using Observer = void (*)(const Change &change, void *user);
// Up to kMaxObservers at once; 0 when full, else a handle for Unsubscribe.
inline constexpr size_t kMaxObservers = 5;  // the screen on top, the one under it as it leaves, the buzzer's sound setting, and room
int Subscribe(Observer observer, void *user);
void Unsubscribe(int handle);

// --- The link's side ------------------------------------------------------------------------------------------------------

// Adds or replaces the agent with the record's id; past kMaxAgents the oldest kIdle agent goes first, then kDone.
void SetAgent(const Agent &agent);
void RemoveAgent(uint16_t id);
void SetUsage(const Usage &usage);
// Queues a request, past kMaxRequests refused (false) and left to the terminal. It never opens by itself.
bool OpenRequest(const Request &request);
// Queues a question as a request is queued; false when refused or it holds no option.
bool OpenQuestion(const Question &question);
// Answered in the terminal, or timed out: a screen showing it says so; the queue forgets it.
void CloseRequest(uint32_t id, CloseReason reason);
// Whether the bridge is there. Down with no agent, the overview says "Connecting...".
void SetLink(bool up);
// Every agent, request and the usage forgotten — a request on view closes as timed out (the link lost). It is the snapshot's `reset` too,
// the only call that takes the deck off standby.
void Reset();

// This deck is not the active one (AGENT_DECK 4.13, the bridge's `standby`): the requests queued close silently, the agents and the usage
// are forgotten, and the standby page shows. `on` false is the same as a Reset's end of standby, without the rest. A `standby` that comes
// while Connect waits is the bridge's refusal: another deck is on the cable.
void SetStandby(bool on);

// Where the screens' answers go, on LVGL's task: the link passes them on. A request's: allow or deny. A question's: bit n for
// option n, 0 to leave it to the terminal.
using DecisionFn = void (*)(uint32_t id, bool allow);
using AnswerFn = void (*)(uint32_t id, uint8_t part, uint8_t chosen);
void SetListeners(DecisionFn decision, AnswerFn answer);

// --- The screens' side ----------------------------------------------------------------------------------------------------

size_t AgentCount();
const Agent *AgentAt(size_t index);       // in the order the agents arrived
const Agent *FindAgent(uint16_t id);
// When the agent's record arrived (lv_tick_get()): its `since_s` counts on from there.
uint32_t AgentReceivedMs(uint16_t id);
const Usage &GetUsage();
bool UsageKnown();                         // a usage record has arrived
bool LinkUp();

size_t QueueCount();
const Item *QueueAt(size_t index);         // in arrival order, the oldest first
const Item *FindItem(uint32_t id);
// The oldest item queued for this agent, or nullptr.
const Item *OldestFor(uint16_t agent);
// The agent's place in the queue among the items: 1 of N, N = QueueCount().
size_t QueuePosition(uint32_t id);

// A request answered on the deck: it leaves the queue and the decision goes to the link. A no-op for an unknown id.
void Answer(uint32_t id, bool allow);
// A question's answer: it leaves the queue, `chosen` goes to the link — 0 leaves it to the terminal.
void AnswerQuestion(uint32_t id, uint8_t part, uint8_t chosen);

// The sound setting the home foot's toggle and the settings share: the buzzer on or off.
bool SoundOn();
void SetSoundOn(bool on);

// --- Settings, battery, standby and Wi-Fi (STICKY_AGENT_DECK §4.7–§4.9) --------------------------------------------------------------

// Auto sleep, in minutes: 5 to 60 in steps of 5, 30 at first (AGENT_DECK 4.4).
inline constexpr int kAutoSleepMin = 5;
inline constexpr int kAutoSleepMax = 60;
inline constexpr int kAutoSleepStep = 5;
inline constexpr int kAutoSleepStart = 30;
int AutoSleepMinutes();
void SetAutoSleepMinutes(int minutes);

// The time zone, an index into the fifteen of rules::kTimeZones, Pacific at first. Kept here; the clock follows it when the logic port
// applies it (STICKY_AGENT_DECK §9).
inline constexpr int kTimeZones = 15;
int TimeZone();
void SetTimeZone(int index);

// The Wi-Fi switch of the settings. Off, the radio leaves its network (the saved one stays); the link is told (WifiAsk::kSwitch).
bool WifiOn();
void SetWifiOn(bool on);

// The battery as the link reads it: -1 for no reading yet.
struct Battery {
    int percent = -1;
    bool charging = false;
};
void SetBattery(const Battery &battery);
const Battery &GetBattery();

// The Wi-Fi page's record (AGENT_DECK 4.9): which page it shows, what it is on, the networks the Mac offered and the joined network's
// facts. The link writes it with SetWifi; the screens ask the link through AskWifi and read what comes back.
inline constexpr size_t kSsidChars = 32;
inline constexpr size_t kMaxNetworks = 8;
inline constexpr size_t kMaxFacts = 6;
enum class WifiStage : uint8_t {
    kNotJoined,  // not connected: Connect asks the Mac for its network
    kChoosing,   // the Mac's networks, to pick one
    kJoining,    // step 2 of 3
    kFailed,     // a step failed: `failure` says which
    kPaired,     // the third step done: Done
    kJoined,     // the network and its facts
};
enum class WifiFailure : uint8_t { kWrongPassword };  // the second step; the deck's other failures come with the link
struct Network {
    char name[kSsidChars + 1] = {};
    bool locked = false;
    uint8_t signal = 0;  // 0 to 3 bars
};
struct Fact {
    char name[12] = {};
    char value[40] = {};
};
struct Wifi {
    WifiStage stage = WifiStage::kNotJoined;
    WifiFailure failure = WifiFailure::kWrongPassword;
    Network network;  // the one joined, being joined, paired or failed
    std::array<Network, kMaxNetworks> networks{};
    size_t network_count = 0;
    std::array<Fact, kMaxFacts> facts{};
    size_t fact_count = 0;
};
const Wifi &GetWifi();
void SetWifi(const Wifi &wifi);

// What a Wi-Fi screen asks of the link: the page's buttons and the network row. `arg` is the network's index for kPick, the switch's state
// (1 on) for kSwitch.
enum class WifiAsk : uint8_t { kConnect, kScan, kPick, kCancel, kRetry, kDone, kDisconnect, kForget, kSwitch };
using WifiFn = void (*)(WifiAsk ask, int arg);
void SetWifiListener(WifiFn listener);
void AskWifi(WifiAsk ask, int arg = 0);

// The standby page's state (AGENT_DECK 4.13): none while the deck is active; standby; connecting from a Connect until the snapshot or
// kConnectWaitMs; timed out, standby again with the words saying the Mac did not answer; refused, standby again at once on a `standby`
// that comes while connecting, the words saying another deck is on the cable.
inline constexpr uint32_t kConnectWaitMs = 15000;  // AGENT_DECK 4.13: how long Connect waits for the snapshot
enum class StandbyPage : uint8_t { kNone, kStandby, kConnecting, kTimedOut, kRefused };
StandbyPage Standby();
// Connect, from the page's button: connecting, and the link asked to make this deck active. Nothing unless the page is standby, timed
// out or refused.
void ConnectPressed();
// From the page's tick and as it is entered: kConnectWaitMs past a Connect with no snapshot, the page is timed out.
void StandbyTick();
using ConnectFn = void (*)();
void SetConnectListener(ConnectFn listener);

}  // namespace agent_deck::model
