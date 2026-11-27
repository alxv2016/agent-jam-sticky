// The deck screens' rules with no LVGL and no board in them, so tests/host checks them on the computer: how long, how many tokens and
// when a window resets are worded; the carousel's page order; what the overview's count says and when it is a control; which move a
// state plays; where a request leaves to (AGENT_DECK 4.7); who waits in the needs-you list and how a row says it; which change
// makes which of the deck's event sounds (STICKY_AGENT_DECK §3, §4, §6).

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "deck_model.h"
#include "widgets/mascot.h"

namespace agent_deck::rules {

// --- Words ----------------------------------------------------------------------------------------------------------------

// A span as the deck words it, counted down to the unit a glance needs: `12s`, `4m`, `2h`, `3d`. The screens count it in minutes
// once past a minute, and never faster than DESIGN's self-update (motion.self-update).
inline void FormatSpan(char *out, size_t size, uint32_t seconds)
{
    if (seconds < 60) {
        std::snprintf(out, size, "%us", static_cast<unsigned>(seconds));
    } else if (seconds < 3600) {
        std::snprintf(out, size, "%um", static_cast<unsigned>(seconds / 60));
    } else if (seconds < 86400) {
        std::snprintf(out, size, "%uh", static_cast<unsigned>(seconds / 3600));
    } else {
        std::snprintf(out, size, "%ud", static_cast<unsigned>(seconds / 86400));
    }
}

// Tokens in the deck's own short form, rounded down: `950`, `18.4k`, `66k`, `126k`, `1.2M` — a whole number when the tenth is zero.
inline void FormatTokens(char *out, size_t size, uint32_t tokens)
{
    if (tokens < 1000) {
        std::snprintf(out, size, "%u", static_cast<unsigned>(tokens));
        return;
    }
    const bool millions = tokens >= 1000000;
    const uint32_t unit = millions ? 1000000 : 1000;
    const char suffix = millions ? 'M' : 'k';
    if (!millions && tokens >= 100000) {
        std::snprintf(out, size, "%uk", static_cast<unsigned>(tokens / unit));
        return;
    }
    const uint32_t tenths = (tokens % unit) * 10 / unit;
    if (tenths == 0) {
        std::snprintf(out, size, "%u%c", static_cast<unsigned>(tokens / unit), suffix);
    } else {
        std::snprintf(out, size, "%u.%u%c", static_cast<unsigned>(tokens / unit), static_cast<unsigned>(tenths), suffix);
    }
}

// When a window resets, the line under its bar: `Resets 4d 2h`, `Resets 1h 12m`, `Resets 12m`. Empty for an unknown window (-1).
inline void FormatReset(char *out, size_t size, int32_t minutes)
{
    if (minutes < 0) {
        out[0] = '\0';
    } else if (minutes >= 1440) {
        std::snprintf(out, size, "Resets %dd %dh", static_cast<int>(minutes / 1440), static_cast<int>(minutes % 1440 / 60));
    } else if (minutes >= 60) {
        std::snprintf(out, size, "Resets %dh %dm", static_cast<int>(minutes / 60), static_cast<int>(minutes % 60));
    } else {
        std::snprintf(out, size, "Resets %dm", static_cast<int>(minutes));
    }
}

// The clock, top-right: `Fri 7 Nov  2:32 PM`. False for a time never set (a year before the firmware existed), which the screens leave blank.
inline bool FormatClock(char *out, size_t size, const std::tm &when)
{
    static const char *const kDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char *const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (when.tm_year + 1900 < 2024 || when.tm_wday < 0 || when.tm_wday > 6 || when.tm_mon < 0 || when.tm_mon > 11) {
        out[0] = '\0';
        return false;
    }
    const int hour = when.tm_hour % 12 == 0 ? 12 : when.tm_hour % 12;
    std::snprintf(out, size, "%s %d %s  %d:%02d %s", kDays[when.tm_wday], when.tm_mday, kMonths[when.tm_mon], hour, when.tm_min,
                  when.tm_hour < 12 ? "AM" : "PM");
    return true;
}

// The state chip's words: `Working for 4m`, `Needs you for 12s`, `Done for 2h`, `Stopped for 11s` for a turn the plan's limit ended.
using State = model::AgentState;
inline void FormatState(char *out, size_t size, State state, bool limit, uint32_t seconds)
{
    char span[12];
    FormatSpan(span, sizeof span, seconds);
    const char *word = limit ? "Stopped" : state == State::kWorking ? "Working" : state == State::kNeedsYou ? "Needs you"
                       : state == State::kDone ? "Done" : "Idle";
    std::snprintf(out, size, "%s for %s", word, span);
}

// "Usage 42%": the window's word and its share, `--%` when the status line carries none.
inline void FormatShare(char *out, size_t size, const char *word, int pct)
{
    if (pct < 0) {
        std::snprintf(out, size, "%s --%%", word);
    } else {
        std::snprintf(out, size, "%s %d%%", word, pct > 100 ? 100 : pct);
    }
}

// --- The carousel ---------------------------------------------------------------------------------------------------------

// The agent pages in the deck's order: the sessions as they arrived, each followed by its sub-agents; a sub-agent whose session is
// not on the deck comes last. `ids` and `parents` are the agents in arrival order; `out` takes the ids, n at most.
inline size_t PageOrder(const uint16_t *ids, const uint16_t *parents, size_t n, uint16_t *out)
{
    size_t count = 0;
    auto has = [&](uint16_t id) {
        for (size_t i = 0; i < n; ++i) {
            if (ids[i] == id) {
                return true;
            }
        }
        return false;
    };
    for (size_t i = 0; i < n; ++i) {
        if (parents[i] != 0) {
            continue;
        }
        out[count++] = ids[i];
        for (size_t j = 0; j < n; ++j) {
            if (parents[j] == ids[i]) {
                out[count++] = ids[j];
            }
        }
    }
    for (size_t i = 0; i < n; ++i) {
        if (parents[i] != 0 && !has(parents[i])) {
            out[count++] = ids[i];
        }
    }
    return count;
}

// The page after a turn of `dir` (+1 next, -1 before) among `pages`, wrapping round — past the last the overview, back from the
// overview the last.
constexpr int TurnTo(int page, int dir, int pages)
{
    return ((page + dir) % pages + pages) % pages;
}

// --- The overview's count -------------------------------------------------------------------------------------------------

struct Chip {
    bool shown = false;
    bool control = false;  // "1 needs you" while a request or question is queued: it opens what waits
    char text[24] = {};
};

// The count the overview says: the most urgent state that is not zero — `1 needs you` (`2 need you` for more), else `2 working`,
// else `3 done` — or `Connecting…` while the link is down with no agent; nothing for no agent with the link up. A control only
// while something is queued and the count says Needs you.
inline Chip ChipFor(int needs, int working, int done, bool link_up, bool queued)
{
    Chip chip;
    chip.shown = true;
    if (needs > 0) {
        std::snprintf(chip.text, sizeof chip.text, needs == 1 ? "%d needs you" : "%d need you", needs);
        chip.control = queued;
    } else if (working > 0) {
        std::snprintf(chip.text, sizeof chip.text, "%d working", working);
    } else if (done > 0) {
        std::snprintf(chip.text, sizeof chip.text, "%d done", done);
    } else if (!link_up) {
        std::snprintf(chip.text, sizeof chip.text, "Connecting…");
    } else {
        chip.shown = false;
    }
    return chip;
}

// --- Moves ----------------------------------------------------------------------------------------------------------------

// The overview's mascot stands for every agent: the jump while anyone needs you, else the work while anyone works, else the idle.
constexpr widgets::MascotMove OverviewMove(int needs, int working)
{
    return needs > 0 ? widgets::MascotMove::kJump : working > 0 ? widgets::MascotMove::kWork : widgets::MascotMove::kIdle;
}

// A page's mascot by its agent's state (AGENT_DECK 9.4): hopping or waving for attention, typing, the idle; a done agent's finish or
// celebrate is picked by `celebrate`, a coin the page keeps for the agent; a turn the plan's limit ended is not finished: it idles. A
// previous chat is idle once it has nothing to do.
constexpr widgets::MascotMove AgentMove(State state, bool background, bool limit, bool celebrate)
{
    if (background && state != State::kNeedsYou && state != State::kWorking) {
        return widgets::MascotMove::kIdle;
    }
    switch (state) {
    case State::kWorking: return widgets::MascotMove::kWork;
    case State::kNeedsYou: return widgets::MascotMove::kNeedsYou;
    case State::kDone:
        return limit ? widgets::MascotMove::kIdle : celebrate ? widgets::MascotMove::kCelebrate : widgets::MascotMove::kFinish;
    case State::kIdle: break;
    }
    return widgets::MascotMove::kIdle;
}

// --- Where a request leaves to (AGENT_DECK 4.7) --------------------------------------------------------------------------

enum class LeaveTo : uint8_t {
    kAgentPage,   // the carousel, on the asking agent's page
    kList,        // the needs-you list opens, in place of the request
    kBackToList,  // the list the request was opened from
};
// `others` is the requests and questions still waiting besides the one that ended — for an answer or a close the queue's count, for
// Dismiss the queue's less the dismissed one, which waits too. With none, the agent's page; with some, the list — the one the request
// came from, else opened in its place.
constexpr LeaveTo LeaveFor(bool opened_from_list, size_t others)
{
    return others == 0 ? LeaveTo::kAgentPage : opened_from_list ? LeaveTo::kBackToList : LeaveTo::kList;
}

// --- The needs-you list ---------------------------------------------------------------------------------------------------

// Each agent whose requests wait, once, the one with the oldest request first: `queue_agents` are the queued items' agents in arrival
// order. Returns how many; `out` takes at most n.
inline size_t WaitingAgents(const uint16_t *queue_agents, size_t n, uint16_t *out)
{
    size_t count = 0;
    for (size_t i = 0; i < n; ++i) {
        bool seen = false;
        for (size_t j = 0; j < count; ++j) {
            seen = seen || out[j] == queue_agents[i];
        }
        if (!seen) {
            out[count++] = queue_agents[i];
        }
    }
    return count;
}

// THE AVATARS' IDLE CYCLE, the deck's own (AGENT_DECK 4.7, the developer's ask, 2026-11-03; the owner's, 2026-11-17): the avatars rest, and every
// `kAvatarIdleEveryMs` one agent's, picked at random and never the one before while there is another, plays one cycle and comes home — the idle's
// lean (widgets::kMascotIdleCycleMs) or the look-around (widgets::kMascotLookCycleMs), at even odds from the pick's own hash. A cycle is longer than
// the gap, so two may move at once; one alone on the list is not picked again until its cycle is home. The screen looks every
// `kAvatarLookMs`.
inline constexpr uint32_t kAvatarIdleEveryMs = 3000;
inline constexpr uint32_t kAvatarLookMs = 100;

// A hash of the clock for the pick, as the mascot's own randomness is: no state, the same run after run.
constexpr uint32_t AvatarHash(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    return x ^ (x >> 16);
}

struct AvatarPick {
    size_t row;
    bool look;  // the look-around; else the idle's lean
};

// Which of `count` (1 or more) avatars begins a cycle at `now` (an lv_tick_get() time), given the row picked last (`count` or more for none).
constexpr AvatarPick PickAvatar(uint32_t now, size_t count, size_t last)
{
    const uint32_t h = AvatarHash(now);
    size_t pick = h % count;
    if (count > 1 && pick == last) {
        pick = (pick + 1 + (h >> 8) % (count - 1)) % count;
    }
    return {pick, ((h >> 20) & 1u) != 0};
}

// How long the cycle takes, from its start to the avatar coming home.
constexpr uint32_t AvatarCycleMs(bool look)
{
    return look ? widgets::kMascotLookCycleMs : widgets::kMascotIdleCycleMs;
}

// A row's line under the name: what the agent's oldest asks and how long it has waited, `Bash • 24s`, `Question • 1m`; a sub-agent's
// type first, `Explore • Bash • 6s`, and a previous chat's `previous chat • ` before it.
inline void FormatWaiting(char *out, size_t size, const char *kind, bool previous, const char *asks, uint32_t waited_s)
{
    char span[12];
    FormatSpan(span, sizeof span, waited_s);
    char line[96] = {};
    if (previous) {
        std::snprintf(line, sizeof line, "previous chat \xE2\x80\xA2 ");
    }
    size_t o = std::strlen(line);
    if (kind != nullptr && kind[0] != '\0') {
        o += static_cast<size_t>(std::snprintf(line + o, sizeof line - o, "%s \xE2\x80\xA2 ", kind));
    }
    std::snprintf(line + o, sizeof line - o, "%s \xE2\x80\xA2 %s", asks, span);
    std::snprintf(out, size, "%s", line);
}

// --- Settings, battery and standby (STICKY_AGENT_DECK §4.7–§4.9) ----------------------------------------------------------

// The fifteen time zones of the deck's Time zone list (AGENT_DECK 4.11), North America's first, west to east.
inline constexpr const char *kTimeZones[model::kTimeZones] = {
    "Pacific", "Mountain", "Central", "Eastern", "Alaska", "Hawaii", "UTC", "UK", "Central Europe", "Eastern Europe", "Japan", "China",
    "Australia East", "Australia Central", "Australia West"};

// The Battery row's value: the word and the percent, `Charging \xE2\x80\xA2 62%` on the charger, `Charged \xE2\x80\xA2 84%` off it from 80 %
// (the board cannot tell a full cell), then the deck's own `On battery` and `Low battery` (under 20 %); `--%` before the first reading.
inline void FormatBattery(char *out, size_t size, int percent, bool charging)
{
    if (percent < 0) {
        std::snprintf(out, size, "--%%");
        return;
    }
    const char *word = charging ? "Charging" : percent >= 80 ? "Charged" : percent >= 20 ? "On battery" : "Low battery";
    std::snprintf(out, size, "%s \xE2\x80\xA2 %d%%", word, percent > 100 ? 100 : percent);
}

// The standby page's chip and its line under the mascot (AGENT_DECK 4.13).
inline const char *StandbyChip(model::StandbyPage page)
{
    return page == model::StandbyPage::kConnecting ? "Connecting" : "On standby";
}
inline const char *StandbyWords(model::StandbyPage page)
{
    switch (page) {
    case model::StandbyPage::kConnecting: return "Asking your Mac to make this deck active.";
    case model::StandbyPage::kTimedOut: return "Your Mac did not answer. Try again.";
    case model::StandbyPage::kRefused: return "Another deck is on the cable.";
    default: return "Another deck is active.";
    }
}

// --- The deck's event sounds (DESIGN → Motion → Sound; STICKY_AGENT_DECK §6) ------------------------------------------------------------

// The four sounds the deck makes with no press, in rank: a higher one is never cut by a lower one while it sounds.
enum class EventSound : uint8_t { kNone, kAgentStart, kLinkUp, kUsageLimit, kAttention };

// How long after the link comes up, or a `reset` makes the deck active, the snapshot's agents start in silence: the bridge sends
// everything again, and none of it is news (STICKY_AGENT_DECK §10).
inline constexpr uint32_t kSnapshotQuietMs = 1000;

// What the deck last heard of the plan's limit and its requests, so each sounds as it comes, not on every record that repeats it.
struct EventHeard {
    bool limited = false;
    uint32_t request = 0;  // the last request or question that sounded, until it closes: a question's later parts share its id
};

// The sound a change makes: a request or question queued, `sound.attention`; the link announced — up, and this deck the active one —
// `sound.link-up`; the plan's limit hit, `sound.usage-limit`; an agent working that was new, idle or done, `sound.agent-start`, but
// not while `quiet`. Anything else is silent, and so is a question's later part; one closed elsewhere or timed out and sent again,
// as a snapshot after a Reset does, sounds again. `id` is the change's; `now` the agent's state after a kAgent change; `quiet`
// whether the link is down, or came up or was reset within kSnapshotQuietMs; `limited` whether the usage names a limit after a
// kUsage.
constexpr EventSound EventSoundFor(model::Event event, uint32_t id, bool added, model::AgentState was, model::AgentState now,
                                   bool quiet, bool limited, EventHeard &heard)
{
    switch (event) {
    case model::Event::kRequestOpened: {
        const bool fresh = id != heard.request;
        heard.request = id;
        return fresh ? EventSound::kAttention : EventSound::kNone;
    }
    case model::Event::kAgent:
        return !quiet && now == model::AgentState::kWorking &&
                       (added || was == model::AgentState::kIdle || was == model::AgentState::kDone)
                   ? EventSound::kAgentStart
                   : EventSound::kNone;
    case model::Event::kRequestClosed:
        heard.request = id == heard.request ? 0 : heard.request;
        return EventSound::kNone;
    case model::Event::kLinkAnnounced: return EventSound::kLinkUp;
    case model::Event::kUsage: {
        const bool hit = limited && !heard.limited;
        heard.limited = limited;
        return hit ? EventSound::kUsageLimit : EventSound::kNone;
    }
    default: return EventSound::kNone;
    }
}

// Whether `next` plays over `playing`, which began `elapsed_ms` ago and lasts `playing_ms`: once it has ended, or when `next`
// ranks at least as high.
constexpr bool EventSoundCuts(EventSound next, EventSound playing, uint32_t elapsed_ms, uint32_t playing_ms)
{
    return next != EventSound::kNone && (playing == EventSound::kNone || elapsed_ms >= playing_ms || next >= playing);
}

}  // namespace agent_deck::rules
