// Host tests of the mascot page's passes (components/hw_test/mascot_rules.h), on the computer with no board: a pass
// counts the glass's frames from 1; a looping move is asked to rest once its run has passed, a once-only move never asked
// again; the pass ends once, on the frame that draws rest; a loop begins its next pass only after kLoopRestMs, and a
// once play never does; "n of N" never shows n past N; the readout's words; and the music the mascot is heard playing and what a
// change of it does, mascot_voice's rules (components/mascot_voice/include/mascot_voice_rules.h) (STICKY_DESIGN_TOKENS §13).
// Build and run: bash tests/host/run.sh

#include <cstdio>
#include <cstring>

#include "../../components/hw_test/mascot_rules.h"
#include "mascot_voice_rules.h"

using namespace hw_test::mascot_rules;

namespace {

int s_failures = 0;
int s_checks = 0;

void Check(bool ok, const char *what)
{
    ++s_checks;
    if (!ok) {
        ++s_failures;
        std::printf("FAIL  %s\n", what);
    }
}

bool Says(void (*format)(char *, size_t), const char *want)
{
    char out[64];
    format(out, sizeof out);
    if (std::strcmp(out, want) != 0) {
        std::printf("      got \"%s\", want \"%s\"\n", out, want);
        return false;
    }
    return true;
}

constexpr uint32_t kFrame = 460;

void TestEstimate()
{
    Check(EstimateFrames(5602, kFrame) == 12, "the finish, 5602 ms: 12 frames");
    Check(EstimateFrames(30000, kFrame) == 65, "the work's 30 s: 65 frames");
    Check(EstimateFrames(78500, kFrame) == 171, "idle in place: 171 frames");
    Check(EstimateFrames(1800, kFrame) == 4, "hello, 1.8 s: 4 frames");
    Check(EstimateFrames(0, kFrame) == 1, "a pass is never under one frame");
}

// A looping move: run 1000 ms, and the mascot at rest from `rests_at` ms after it is asked.
void TestLoopingPass()
{
    Pass p;
    p.Start(Mode::kOnce);
    p.Begin(0, 100, 1000);
    Check(p.frame == 1 && !p.asked_rest, "a pass begins on frame 1, a looping move not yet asked to rest");
    Check(!p.Drawn(100), "no refresh, no frame");
    Check(p.Drawn(101) && p.frame == 2, "a refresh drawn: frame 2");
    Check(p.Drawn(103) && p.frame == 4, "two refreshes since the last look: two frames on");
    Check(p.Look(999, 1000, false) == Next::kNothing, "before its run, nothing");
    Check(p.Look(1000, 1000, false) == Next::kAskRest, "its run passed: asked to rest");
    Check(p.Look(1005, 1000, false) == Next::kNothing, "asked once a pass");
    Check(p.Look(1400, 1000, true) == Next::kPassEnded && p.passes == 1, "at rest: the pass ends, once");
    p.Drawn(104);
    Check(p.frame == 4, "the rest frame does not move the count on");
    Check(p.Look(5000, 1000, true) == Next::kNothing, "a once play begins no next pass");
}

void TestOnceOnly()
{
    Pass p;
    p.Start(Mode::kOnce);
    p.Begin(0, 0, 0);
    Check(p.asked_rest, "a once-only move (run 0) is asked to rest as it begins");
    Check(p.Look(0, 0, false) == Next::kNothing, "and never asked again");
    Check(p.Look(1800, 0, true) == Next::kPassEnded, "it ends when it rests by itself");
}

void TestLoop()
{
    Pass p;
    p.Start(Mode::kLoop);
    p.Begin(0, 0, 0);
    Check(p.Look(2000, 0, true) == Next::kPassEnded && p.passes == 1, "a loop's first pass ends");
    Check(p.Look(2000 + kLoopRestMs - 1, 0, true) == Next::kNothing, "rest held until kLoopRestMs");
    Check(p.Look(2000 + kLoopRestMs, 0, true) == Next::kBeginPass, "then the next pass");
    p.Begin(3500, 9, 0);
    Check(p.frame == 1 && !p.resting, "the next pass counts from frame 1");
    Check(p.Look(5500, 0, true) == Next::kPassEnded && p.passes == 2, "its end counts a second pass");
    p.mode = Mode::kRest;
    Check(p.Look(9000, 0, true) == Next::kNothing, "stopped: nothing more");
}

void TestWords()
{
    Check(Says([](char *o, size_t n) { FormatCount(o, n, 7, 12); }, "7 of 12"), "n of N");
    Check(Says([](char *o, size_t n) { FormatCount(o, n, 170, 171); }, "170 of 171"), "three digits");
    Check(Says([](char *o, size_t n) { FormatCount(o, n, 14, 12); }, "14 of 14"), "a pass running long counts on");
    Check(Says([](char *o, size_t n) { FormatAtRest(o, n, true, 0, 0); }, "Rest: a still pose"),
          "Rest's reason");
    Check(Says([](char *o, size_t n) { FormatAtRest(o, n, false, 14, 0); }, "About 14 frames · unplayed"),
          "a move not played");
    Check(Says([](char *o, size_t n) { FormatAtRest(o, n, false, 12, 1); }, "12 frames · 1 play"), "one play");
    Check(Says([](char *o, size_t n) { FormatAtRest(o, n, false, 171, 12); }, "171 frames · 12 plays"),
          "plays, the longest line");
    Check(Says([](char *o, size_t n) { FormatLooping(o, n, 0); }, "Looping…"), "looping, the first pass");
    Check(Says([](char *o, size_t n) { FormatLooping(o, n, 1); }, "Looping · 1 pass done"), "one pass");
    Check(Says([](char *o, size_t n) { FormatLooping(o, n, 3); }, "Looping · 3 passes done"), "passes");
}

void TestMusic()
{
    Seen seen;
    Check(MusicOnView(seen, 0).music == Music::kNone, "nothing on view: no music");
    seen.typing = true;
    Heard h = MusicOnView(seen, 4200);
    Check(h.music == Music::kTyping && h.since == 4200, "typing: the loop, from when it began");
    seen.think = true;
    seen.jaunt_since = 4800;
    Check(MusicOnView(seen, 4200).music == Music::kThink, "a jaunt's beat over the typing");
    seen.wave = true;
    Check(MusicOnView(seen, 4200).music == Music::kWave, "the wave before the thinking");
    seen.done = true;
    seen.done_after = 5602;
    seen.move_since = 4000;
    h = MusicOnView(seen, 0);
    Check(h.music == Music::kSuccess && h.since == 9602, "the finish's success: after its movement, over the wave");
    Seen celebrate;
    celebrate.done = true;
    celebrate.done_after = 2143;
    celebrate.move_since = 4000;
    h = MusicOnView(celebrate, 0);
    Check(h.music == Music::kSuccess && h.since == 6143, "the celebrate's success: after its movement, its moments first");
    Check(MusicOnView(celebrate, 0).since == 6143, "asked for the same all through the move");
    seen.spin = true;
    h = MusicOnView(seen, 0);
    Check(h.music == Music::kSpin && h.since == 4000, "the spin above all, from its start");

    const Heard none{};
    const Heard typing{Music::kTyping, 100};
    const Heard success{Music::kSuccess, 200};
    Check(ChangeOf(typing, typing) == Change::kKeep, "the same music: kept");
    Check(ChangeOf(none, typing) == Change::kPlay, "a new music: played");
    Check(ChangeOf(typing, Heard{Music::kTyping, 900}) == Change::kPlay, "the same music, a new beat: played again");
    Check(ChangeOf(typing, none) == Change::kStop, "no music now: the typing stops");
    Check(ChangeOf(success, none) == Change::kKeep, "the success plays to its end");
}

}  // namespace

int main()
{
    TestEstimate();
    TestLoopingPass();
    TestOnceOnly();
    TestLoop();
    TestWords();
    TestMusic();
    std::printf("mascot page rules: %d checks, %d failed\n", s_checks, s_failures);
    return s_failures == 0 ? 0 : 1;
}
