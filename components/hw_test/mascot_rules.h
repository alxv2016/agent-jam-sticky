// The mascot page's passes, with no LVGL and no board in it, so tests/host checks it on the computer: a pass begins on
// frame 1; each refresh drawn moves the frame on; a looping move is asked to rest once the deck's stage's run has
// passed, a once-only move as it begins; the pass ends on the frame that draws rest, and a loop begins the next after
// kLoopRestMs; "n of N" never shows n past N; and the readout's words (STICKY_DESIGN_TOKENS §13).

#pragma once

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "mascot_voice_rules.h"

namespace hw_test::mascot_rules {

// The rest held between a loop's passes, so each starts clean: the deck's stage's kRestMs, with no refresh in it.
inline constexpr uint32_t kLoopRestMs = 1500;

enum class Mode : uint8_t { kRest, kOnce, kLoop };

// About how many frames a pass of `pass_ms` takes at a frame every `frame_ms`: rounded, at least one.
constexpr uint32_t EstimateFrames(uint32_t pass_ms, uint32_t frame_ms)
{
    const uint32_t n = (pass_ms + frame_ms / 2) / frame_ms;
    return n > 0 ? n : 1;
}

// What the page does after a look at the glass and the mascot.
enum class Next : uint8_t {
    kNothing,
    kAskRest,     // ask the move to rest now: its run has passed
    kPassEnded,   // the frame about to be drawn is rest: the pass is over — the page stops a once play here
    kBeginPass,   // a loop's rest has been held: the next pass from its start
};

// One pass and the loop round it. `refreshes` is every frame the glass has taken, drawn or found unchanged. Times are
// ms on a 32-bit tick; unsigned subtraction keeps a span right across the tick's wrap, as lv_tick_diff does.
struct Pass {
    Mode mode = Mode::kRest;
    uint32_t frame = 0;       // the frame the next refresh draws, of this pass, from 1
    uint32_t seen = 0;        // the refreshes counted at the last look
    uint32_t began = 0;       // when this pass began, ms
    uint32_t rest_since = 0;  // when the pass ended, ms
    uint32_t passes = 0;      // this loop's passes done
    bool asked_rest = false;
    bool resting = false;

    void Start(Mode m)
    {
        mode = m;
        passes = 0;
    }

    // A pass from its start at `now`: a once-only move (`run_ms` 0) is asked to rest as it begins, so it plays to its
    // end and rests by itself (the deck's stage).
    void Begin(uint32_t now, uint32_t refreshes, uint32_t run_ms)
    {
        frame = 1;
        seen = refreshes;
        began = now;
        asked_rest = run_ms == 0;
        resting = false;
    }

    // The refreshes the glass has taken now: true when any were drawn since the last look, the frame moved on with them
    // while the move plays.
    bool Drawn(uint32_t refreshes)
    {
        if (refreshes == seen) {
            return false;
        }
        if (!resting) {
            frame += refreshes - seen;
        }
        seen = refreshes;
        return true;
    }

    // A look at `now`, with `at_rest` whether the mascot shows rest. kAskRest at most once a pass; kPassEnded once, on
    // the frame rest is drawn; kBeginPass, looping, once rest has been held kLoopRestMs.
    Next Look(uint32_t now, uint32_t run_ms, bool at_rest)
    {
        if (mode == Mode::kRest) {
            return Next::kNothing;
        }
        if (!asked_rest && now - began >= run_ms) {
            asked_rest = true;
            return Next::kAskRest;
        }
        if (!asked_rest || !at_rest) {
            return Next::kNothing;
        }
        if (!resting) {
            resting = true;
            rest_since = now;
            ++passes;
            return Next::kPassEnded;
        }
        if (mode == Mode::kLoop && now - rest_since >= kLoopRestMs) {
            return Next::kBeginPass;
        }
        return Next::kNothing;
    }
};

// "n of N": the frame and the pass's frames, N never under n — a pass that runs long counts on.
inline void FormatCount(char *out, size_t size, uint32_t frame, uint32_t frames)
{
    std::snprintf(out, size, "%" PRIu32 " of %" PRIu32, frame, frames > frame ? frames : frame);
}

// The readout's first line at rest: Rest's reason, a move not yet played, or one played — each one 26 px line, 27 characters at most.
inline void FormatAtRest(char *out, size_t size, bool rest, uint32_t frames, uint32_t times)
{
    if (rest) {
        std::snprintf(out, size, "Rest: a still pose");
    } else if (times == 0) {
        std::snprintf(out, size, "About %" PRIu32 " frames · unplayed", frames);
    } else {
        std::snprintf(out, size, "%" PRIu32 " frames · %" PRIu32 " %s", frames, times, times == 1 ? "play" : "plays");
    }
}

// The first line while looping: the passes done.
inline void FormatLooping(char *out, size_t size, uint32_t passes)
{
    if (passes == 0) {
        std::snprintf(out, size, "Looping…");
    } else {
        std::snprintf(out, size, "Looping · %" PRIu32 " %s done", passes, passes == 1 ? "pass" : "passes");
    }
}

// --- The music: what the mascot on the stage is heard doing, besides its moments — shared with the agent deck's screens, so the
// rules are mascot_voice's (components/mascot_voice, DESIGN → Motion → Sound) and the names stay here for this page's tests.
using mascot_voice::Change;
using mascot_voice::ChangeOf;
using mascot_voice::Heard;
using mascot_voice::Music;
using mascot_voice::MusicOnView;
using mascot_voice::Seen;

}  // namespace hw_test::mascot_rules
