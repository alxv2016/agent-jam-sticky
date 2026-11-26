// The mock day: the agent deck's made-up day (AGENT_DECK 18, deck_mock.cpp's kDay) as C structs calling the model, on one lv_timer, looping —
// the stand-in until the link is ported, so the screens run on records as the bridge would send them. Made-up names only. It listens
// for the screens' answers as the deck's mock does and acknowledges them a tick later: the request's agent goes back to work.

#pragma once

#include <cstdint>

#include "deck_model.h"

namespace agent_deck::mock {

inline constexpr uint32_t kDayMs = 180000;  // the day loops this often: its last 22 s are the deck on standby
inline constexpr uint32_t kTickMs = 100;    // how often the mock looks at the day

// One step of the day: a record at its time into the day. `when` gates it on a request: kOpen only while that request still waits (the
// timeout or the terminal's answer the bridge would report), kAnswered only once the deck answered it.
enum class When : uint8_t { kAlways, kOpen };
enum class Op : uint8_t { kReset, kLink, kUsage, kAgent, kRemove, kRequest, kQuestion, kClose, kBattery, kStandby };

struct RequestStep {
    uint16_t agent;
    const char *tool;
    const char *body;
};
struct QuestionStep {
    uint16_t agent;
    uint8_t part;
    const char *tool;
    const char *question;
    const char *options[model::kMaxOptions];
    uint8_t count;
    bool multi;
};
struct Step {
    uint32_t at_ms;
    When when;
    int request;  // the request n the step opens (kRequest, kQuestion), or `when` asks about (kClose); 0 for none
    Op op;
    model::Agent agent;
    model::Usage usage;
    uint16_t id;  // kRemove: the agent
    model::CloseReason reason;
    RequestStep req;
    QuestionStep question;
    int battery_pct;  // kBattery: the reading
    bool charging;    // kBattery: on the charger
};

// The steps, in time order, and how many.
const Step *Day(size_t *count);

// Starts the day's timer and its first day; once, under the LVGL lock. False when LVGL had no timer to give.
bool Start();
// Stops the timer; the model keeps what it holds.
void Stop();

// The request id the mock gives request n on this loop of the day: unique per loop, so a request of one day never answers another's.
uint32_t RequestId(uint32_t loop, int n);

}  // namespace agent_deck::mock
