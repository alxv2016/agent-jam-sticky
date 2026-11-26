// What Claude's mascot is heard doing, besides its moments, with no LVGL and no board in it, so tests/host checks it on the
// computer: the deck's order on the buzzer's one voice — the spin; else the success once the finish's or the celebrate's
// movement is done; else the wave's or the thinking's beat; else the typing, looped from when it began — and what a change of
// music does to the one sounding (DESIGN → Motion → Sound; STICKY_DESIGN_TOKENS §13).

#pragma once

#include <cstdint>

namespace mascot_voice {

enum class Music : uint8_t { kNone, kTyping, kSpin, kSuccess, kWave, kThink };

// What the mascot shows now, as the widget says it: typing at the laptop; the move on view and when it began; a jaunt — the wave's
// "hi"s or the idle's thinking — and when its beat began.
struct Seen {
    bool typing = false;
    bool spin = false;     // the spin is the move on view
    bool done = false;     // the finish or the celebrate is
    uint32_t done_after = 0;  // how long into it the success waits: the done move's whole movement
    uint32_t move_since = 0;
    bool wave = false;
    bool think = false;
    uint32_t jaunt_since = 0;
};

// The music and when it began, on the move's clock: `since` 0 is none.
struct Heard {
    Music music = Music::kNone;
    uint32_t since = 0;
};

// The deck's order on one voice: the spin; else the success, once the finish's or the celebrate's movement is done; else the wave's
// or the thinking's beat; else the typing, looped from when it began (`typing_since`, the page's own note of it). A music's `since`
// may lie ahead: it sounds from then.
constexpr Heard MusicOnView(const Seen &seen, uint32_t typing_since)
{
    if (seen.spin) {
        return {Music::kSpin, seen.move_since};
    }
    if (seen.done) {
        // After the movement: the success begins as the finish's or the celebrate's movement ends, its moments heard first — the
        // developer's ask, 2026-11-24, over the deck's success from the move's start — and plays to its end however the move ends.
        return {Music::kSuccess, seen.move_since + seen.done_after};
    }
    if (seen.wave) {
        return {Music::kWave, seen.jaunt_since};
    }
    if (seen.think) {
        return {Music::kThink, seen.jaunt_since};
    }
    if (seen.typing) {
        return {Music::kTyping, typing_since};
    }
    return {};
}

// What a change of music does to the one sounding: a new one plays; none stops it, but for the success, which plays to its end as
// the deck's does.
enum class Change : uint8_t { kKeep, kPlay, kStop };
constexpr Change ChangeOf(const Heard &was, const Heard &now)
{
    if (was.music == now.music && was.since == now.since) {
        return Change::kKeep;
    }
    if (now.music != Music::kNone) {
        return Change::kPlay;
    }
    return was.music == Music::kSuccess ? Change::kKeep : Change::kStop;
}

}  // namespace mascot_voice
