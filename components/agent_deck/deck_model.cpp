#include "deck_model.h"

#include <algorithm>
#include <cstring>

#include "in_psram.h"
#include "lvgl.h"

namespace agent_deck::model {
namespace {

struct Slot {
    bool used = false;
    Agent agent;
    uint32_t received_ms = 0;
};

// The records' tables live in PSRAM (lvgl_display::InPsram): only the LVGL task touches the model.
std::array<Slot, kMaxAgents> &s_agents = lvgl_display::InPsram<std::array<Slot, kMaxAgents>>();
std::array<Item, kMaxRequests> &s_queue = lvgl_display::InPsram<std::array<Item, kMaxRequests>>();
uint32_t s_seq = 0;
Usage s_usage;
bool s_usage_known = false;
bool s_link = false;
bool s_sound = true;
int s_auto_sleep = kAutoSleepStart;
int s_zone = 0;
bool s_wifi_on = true;
Battery s_battery;
Wifi &s_wifi = lvgl_display::InPsram<Wifi>();
WifiFn s_wifi_listener = nullptr;
bool s_standby = false;
bool s_connecting = false;
bool s_connect_failed = false;
bool s_connect_refused = false;
uint32_t s_connect_ms = 0;
ConnectFn s_connect_listener = nullptr;
DecisionFn s_decision = nullptr;
AnswerFn s_answer = nullptr;

struct Subscriber {
    Observer fn = nullptr;
    void *user = nullptr;
};
std::array<Subscriber, kMaxObservers> s_observers;

// Tells every subscriber; one that unsubscribes inside its own call is left alone.
void Tell(const Change &change)
{
    const auto observers = s_observers;
    for (const Subscriber &o : observers) {
        if (o.fn != nullptr) {
            o.fn(change, o.user);
        }
    }
}

void CopyText(char *to, size_t size, const char *from)
{
    std::snprintf(to, size, "%s", from != nullptr ? from : "");
}

Slot *SlotOf(uint16_t id)
{
    for (Slot &s : s_agents) {
        if (s.used && s.agent.id == id) {
            return &s;
        }
    }
    return nullptr;
}

// The place a new agent takes: a free slot, else the oldest idle agent's, else the oldest done agent's. nullptr when none can go.
Slot *RoomForAgent()
{
    for (Slot &s : s_agents) {
        if (!s.used) {
            return &s;
        }
    }
    for (AgentState victim : {AgentState::kIdle, AgentState::kDone}) {
        for (Slot &s : s_agents) {
            if (s.agent.state == victim) {
                return &s;
            }
        }
    }
    return nullptr;
}

// The agents stand in the order they arrived: a slot is a place in a list that closes up as one leaves, so index order is arrival order.
void CloseUp()
{
    std::stable_partition(s_agents.begin(), s_agents.end(), [](const Slot &s) { return s.used; });
}

Item *ItemOf(uint32_t id)
{
    for (Item &i : s_queue) {
        if (i.used && i.id == id) {
            return &i;
        }
    }
    return nullptr;
}

Item *FreeItem()
{
    for (Item &i : s_queue) {
        if (!i.used) {
            return &i;
        }
    }
    return nullptr;
}

}  // namespace

int Subscribe(Observer observer, void *user)
{
    for (size_t i = 0; i < s_observers.size(); ++i) {
        if (s_observers[i].fn == nullptr) {
            s_observers[i] = {observer, user};
            return static_cast<int>(i) + 1;
        }
    }
    return 0;
}

void Unsubscribe(int handle)
{
    if (handle >= 1 && handle <= static_cast<int>(s_observers.size())) {
        s_observers[static_cast<size_t>(handle) - 1] = {};
    }
}

void SetAgent(const Agent &agent)
{
    if (agent.id == 0) {
        return;
    }
    Change change{Event::kAgent, agent.id};
    Slot *slot = SlotOf(agent.id);
    if (slot == nullptr) {
        slot = RoomForAgent();
        if (slot == nullptr) {
            return;
        }
        if (slot->used) {
            const uint16_t gone = slot->agent.id;
            RemoveAgent(gone);
            slot = RoomForAgent();
            if (slot == nullptr || slot->used) {
                return;
            }
        }
        change.added = true;
    } else {
        change.was = slot->agent.state;
    }
    slot->used = true;
    slot->agent = agent;
    slot->received_ms = lv_tick_get();
    CloseUp();
    Tell(change);
}

void RemoveAgent(uint16_t id)
{
    Slot *slot = SlotOf(id);
    if (slot == nullptr) {
        return;
    }
    *slot = {};
    CloseUp();
    Tell({Event::kAgentRemoved, id});
}

void SetUsage(const Usage &usage)
{
    s_usage = usage;
    s_usage_known = true;
    Tell({Event::kUsage});
}

bool OpenRequest(const Request &request)
{
    Item *item = request.id != 0 ? FreeItem() : nullptr;
    if (item == nullptr || ItemOf(request.id) != nullptr) {
        return false;
    }
    *item = {};
    item->used = true;
    item->id = request.id;
    item->agent = request.agent;
    CopyText(item->tool, sizeof item->tool, request.tool);
    const char *body = request.body != nullptr ? request.body : "";
    const size_t length = std::min(std::strlen(body), kBodyBytes);
    item->body.assign(body, length);
    item->cut = request.cut || std::strlen(body) > kBodyBytes;
    item->seq = ++s_seq;
    item->opened_ms = lv_tick_get();
    Tell({Event::kRequestOpened, request.id});
    return true;
}

bool OpenQuestion(const Question &question)
{
    if (question.option_count == 0 || question.option_count > kMaxOptions || question.id == 0) {
        return false;
    }
    Item *item = FreeItem();
    if (item == nullptr || ItemOf(question.id) != nullptr) {
        return false;
    }
    *item = {};
    item->used = true;
    item->is_question = true;
    item->id = question.id;
    item->part = question.part;
    item->agent = question.agent;
    CopyText(item->tool, sizeof item->tool, question.tool);
    item->question = std::string(question.question != nullptr ? question.question : "").substr(0, kQuestionChars);
    item->option_count = question.option_count;
    for (size_t i = 0; i < question.option_count; ++i) {
        item->options[i] = std::string(question.options[i] != nullptr ? question.options[i] : "").substr(0, kOptionChars);
    }
    item->multi = question.multi;
    item->seq = ++s_seq;
    item->opened_ms = lv_tick_get();
    Tell({Event::kRequestOpened, question.id});
    return true;
}

void CloseRequest(uint32_t id, CloseReason reason)
{
    Item *item = ItemOf(id);
    if (item == nullptr) {
        return;
    }
    *item = {};
    Change change{Event::kRequestClosed, id};
    change.reason = reason;
    Tell(change);
}

void SetLink(bool up)
{
    if (s_link == up) {
        return;
    }
    s_link = up;
    Tell({Event::kLink});
}

void Reset()
{
    // A request on view closes as timed out: the terminal still asks.
    for (const Item &item : std::array<Item, kMaxRequests>(s_queue)) {
        if (item.used) {
            CloseRequest(item.id, CloseReason::kTimedOut);
        }
    }
    while (s_agents[0].used) {  // the rest close up as each goes
        RemoveAgent(s_agents[0].agent.id);
    }
    s_usage = {};
    s_usage_known = false;
    Tell({Event::kUsage});
    // The snapshot's reset is the one call that takes the deck off standby.
    if (s_standby || s_connecting || s_connect_failed || s_connect_refused) {
        s_standby = s_connecting = s_connect_failed = s_connect_refused = false;
        Tell({Event::kStandby});
    }
}

void SetStandby(bool on)
{
    if (on && s_standby && s_connecting) {
        // `standby` while Connect waits: the bridge refused the `use` — another deck is on the cable.
        s_connecting = false;
        s_connect_refused = true;
        Tell({Event::kStandby});
        return;
    }
    if (on == s_standby) {
        return;
    }
    s_standby = on;
    if (on) {
        // Not the active deck: what was sent is not this deck's to show. A request on view closes with no word and no cue.
        for (const Item &item : std::array<Item, kMaxRequests>(s_queue)) {
            if (item.used) {
                CloseRequest(item.id, CloseReason::kStandby);
            }
        }
        for (Slot &slot : s_agents) {
            slot = {};
        }
        s_usage = {};
        s_usage_known = false;
    }
    s_connecting = s_connect_failed = s_connect_refused = false;
    Tell({Event::kStandby});
}

void SetListeners(DecisionFn decision, AnswerFn answer)
{
    s_decision = decision;
    s_answer = answer;
}

size_t AgentCount()
{
    size_t n = 0;
    for (const Slot &s : s_agents) {
        n += s.used;
    }
    return n;
}

const Agent *AgentAt(size_t index)
{
    return index < s_agents.size() && s_agents[index].used ? &s_agents[index].agent : nullptr;
}

const Agent *FindAgent(uint16_t id)
{
    const Slot *slot = SlotOf(id);
    return slot != nullptr ? &slot->agent : nullptr;
}

uint32_t AgentReceivedMs(uint16_t id)
{
    const Slot *slot = SlotOf(id);
    return slot != nullptr ? slot->received_ms : 0;
}

const Usage &GetUsage()
{
    return s_usage;
}

bool UsageKnown()
{
    return s_usage_known;
}

bool LinkUp()
{
    return s_link;
}

size_t QueueCount()
{
    size_t n = 0;
    for (const Item &i : s_queue) {
        n += i.used;
    }
    return n;
}

const Item *QueueAt(size_t index)
{
    std::array<const Item *, kMaxRequests> ordered{};
    size_t n = 0;
    for (const Item &i : s_queue) {
        if (i.used) {
            ordered[n++] = &i;
        }
    }
    std::sort(ordered.begin(), ordered.begin() + static_cast<std::ptrdiff_t>(n),
              [](const Item *a, const Item *b) { return a->seq < b->seq; });
    return index < n ? ordered[index] : nullptr;
}

const Item *FindItem(uint32_t id)
{
    return ItemOf(id);
}

const Item *OldestFor(uint16_t agent)
{
    for (size_t i = 0; i < QueueCount(); ++i) {
        const Item *item = QueueAt(i);
        if (item != nullptr && item->agent == agent) {
            return item;
        }
    }
    return nullptr;
}

size_t QueuePosition(uint32_t id)
{
    for (size_t i = 0; i < QueueCount(); ++i) {
        const Item *item = QueueAt(i);
        if (item != nullptr && item->id == id) {
            return i + 1;
        }
    }
    return 0;
}

void Answer(uint32_t id, bool allow)
{
    Item *item = ItemOf(id);
    if (item == nullptr || item->is_question) {
        return;
    }
    *item = {};
    if (s_decision != nullptr) {
        s_decision(id, allow);
    }
    Change change{Event::kRequestAnswered, id};
    change.allow = allow;
    Tell(change);
}

void AnswerQuestion(uint32_t id, uint8_t part, uint8_t chosen)
{
    Item *item = ItemOf(id);
    if (item == nullptr || !item->is_question) {
        return;
    }
    *item = {};
    if (s_answer != nullptr) {
        s_answer(id, part, chosen);
    }
    Change change{Event::kRequestAnswered, id};
    change.allow = chosen != 0;
    Tell(change);
}

bool SoundOn()
{
    return s_sound;
}

void SetSoundOn(bool on)
{
    if (s_sound == on) {
        return;
    }
    s_sound = on;
    Tell({Event::kSound});
}

int AutoSleepMinutes()
{
    return s_auto_sleep;
}

void SetAutoSleepMinutes(int minutes)
{
    // Steps of five between the ends; a value off the list reads as the nearest one.
    minutes = std::clamp((minutes + kAutoSleepStep / 2) / kAutoSleepStep * kAutoSleepStep, kAutoSleepMin, kAutoSleepMax);
    if (minutes != s_auto_sleep) {
        s_auto_sleep = minutes;
        Tell({Event::kSettings});
    }
}

int TimeZone()
{
    return s_zone;
}

void SetTimeZone(int index)
{
    index = std::clamp(index, 0, kTimeZones - 1);
    if (index != s_zone) {
        s_zone = index;
        Tell({Event::kSettings});
    }
}

bool WifiOn()
{
    return s_wifi_on;
}

void SetWifiOn(bool on)
{
    if (s_wifi_on == on) {
        return;
    }
    s_wifi_on = on;
    Tell({Event::kSettings});
    AskWifi(WifiAsk::kSwitch, on ? 1 : 0);
}

void SetBattery(const Battery &battery)
{
    if (battery.percent != s_battery.percent || battery.charging != s_battery.charging) {
        s_battery = battery;
        Tell({Event::kBattery});
    }
}

const Battery &GetBattery()
{
    return s_battery;
}

const Wifi &GetWifi()
{
    return s_wifi;
}

void SetWifi(const Wifi &wifi)
{
    s_wifi = wifi;
    Tell({Event::kWifi});
}

void SetWifiListener(WifiFn listener)
{
    s_wifi_listener = listener;
}

void AskWifi(WifiAsk ask, int arg)
{
    if (s_wifi_listener != nullptr) {
        s_wifi_listener(ask, arg);
    }
}

StandbyPage Standby()
{
    return !s_standby ? StandbyPage::kNone : s_connecting ? StandbyPage::kConnecting : s_connect_failed ? StandbyPage::kTimedOut
           : s_connect_refused ? StandbyPage::kRefused : StandbyPage::kStandby;
}

void ConnectPressed()
{
    const StandbyPage page = Standby();
    if (page != StandbyPage::kStandby && page != StandbyPage::kTimedOut && page != StandbyPage::kRefused) {
        return;
    }
    s_connecting = true;
    s_connect_failed = false;
    s_connect_refused = false;
    s_connect_ms = lv_tick_get();
    if (s_connect_listener != nullptr) {
        s_connect_listener();
    }
    Tell({Event::kStandby});
}

void StandbyTick()
{
    if (s_standby && s_connecting && lv_tick_diff(lv_tick_get(), s_connect_ms) >= kConnectWaitMs) {
        s_connecting = false;
        s_connect_failed = true;
        Tell({Event::kStandby});
    }
}

void SetConnectListener(ConnectFn listener)
{
    s_connect_listener = listener;
}

}  // namespace agent_deck::model
