#pragma once

/*
 * Ported from the agent deck (STICKY_UI_KIT_PORT §4 item 8); "AGENT_DECK n" below is the deck's own doc, section n, in its repo.
 *
 * THE MASCOT — Claude's, drawn as rectangles on a theme::kMascotCols x kMascotRows cell grid (AGENT_DECK 9): the body,
 * two hands, two eyes, four legs — each an upper half and a foot — every part a whole number of cells at rest, in
 * theme::mascot_body() with the eyes in theme::background(); a move resizes them by the pixel.
 *
 * make_mascot() draws its resting pose; play_mascot() moves these same parts (AGENT_DECK 9.3, 9.5).
 * A move runs from when it began, which the screen keeps for its page in a MascotPlay and hands back to a mascot it
 * rebuilds, so one rebuilt part way through, as a page is on each record, carries on where it was. Every move starts
 * and ends on the rest pose, and one asked for while another plays waits for it to finish — the laptop put away, the
 * hop landed — so a move hands over to the next without a jump (AGENT_DECK 9.4).
 *
 * NOT A CONTROL: no input, never in a group. The hands and eyes are children of the body, so a lean of the body
 * carries them; the legs are the root's, under it.
 */

#include <array>
#include <cstdint>

#include "lvgl.h"

namespace widgets {

struct Mascot {
    lv_obj_t *root; // position THIS: kMascotCols x kMascotRows cells, `cell` px each
    lv_obj_t *body;
    std::array<lv_obj_t *, 2> hands; // left, right
    std::array<lv_obj_t *, 2> eyes;  // left, right
    std::array<lv_obj_t *, 4> legs;  // left to right: each leg's upper half
    std::array<lv_obj_t *, 4> feet;  // its lower half, which swings out to bend the leg in the air
    lv_obj_t *pixels;                // the pixel style's frames, drawn in place of the rig while one plays
    lv_obj_t *pixel_legs;            // the finish's legs, planted while the body tilts above them on `pixels`
    lv_obj_t *flag;                  // the finish's checkered flag, carried by the raised hand
    lv_obj_t *confetti;              // the celebrate's confetti, upright over everything
    lv_obj_t *dust;                  // the dust kicked up at his feet landing a jump, upright
    int32_t cell;
};

// Builds the mascot at rest, `cell` px a cell, in its body's grey (design_tokens.h kColorMascotBody); the cell is
// kSpacingMascotCell, 24 px, and the mascot kMascotCols x kMascotRows (10 x 8) cells of it. The root carries NO POSITION.
// At rest it is still: no blink, so a page at rest sends the glass nothing (MASCOT §6).
Mascot make_mascot(lv_obj_t *parent, int32_t cell);

// WHERE A PAGE STANDS IT, in portrait glass px: the moves that travel — the finish's route, the skip and its trip —
// reckon from this centre, so a page puts the root's centre here. The hardware test's mascot page: the stage band under
// the status bar and its 16 px, 360 px high, the root standing on its bottom edge, centred across the glass — a move may
// go off it, the finish's flag 10 px past the right edge in its widest frames: the person's choice, 2026-11-24
// (STICKY_UI_KIT_PORT §4 item 8).
inline constexpr int32_t kMascotPageCentreX = 240;
inline constexpr int32_t kMascotPageCentreY = 324;

// Paints every part but the eyes `body`, a pixel frame's shaded side `shade` and its lit hand `tint` — fills, so a
// grey draws as its dither. A previous chat's mascot takes kColorMascotFadedBody and its fellows (ui::Fill of each); a
// sub-agent has no grey of its own on this glass and is told by its words (DESIGN → Colors → The mascot's greys).
void set_mascot_colour(const Mascot &m, lv_color_t body, lv_color_t shade, lv_color_t tint);

enum class MascotMove : uint8_t {
    kRest, // still, the rest pose
    kJump, // a hop on the spot, theme::kMascotJumpRise high, every theme::kMascotJumpMs — "needs you"
    kWork, // working_v2 in the pixel style: a laptop taken out, typed on furiously in a loop until stopped, now and then
           // at random a sweat and its wipe, or, rarer, a faceplant, put away
    kFinish, // the checkered flag popped up, waved and put away, theme::kMascotFinishMs; then the rest pose — "done"
    kCelebrate, // a cheer: arms punched up, stomps, confetti, a flapping jump, theme::kMascotCelebrateMs — "done", and
                // a session finishing on the overview
    kIdle, // leaning to look one way, then the other, every theme::kMascotIdleMs; now and then, at random, a walk, a
           // look-around or a skip
    kWave, // waving one hand, then the other, then flapping both, impatient, every theme::kMascotWaveMs
    kNeedsYou, // in turns of theme::kMascotWaveMs, two hops or the wave, at random — "needs you"
    kHello, // the wave's first hand, once, then on to the next — the deck's entrance
    kSkip,  // skipping out to the left, across to the right and home, once — a reaction: a new agent arrived
    kSpin,  // a jump with a whole turn in the air, landing arms up in a puff of dust, once — a reaction: a request
            // allowed, the 5-hour share at 50 %
    kDust,  // two bounces, arms up, a puff of dust at each landing, once — a reaction: a sub-agent finished
    kFaceplant, // from the typing: nodding off, face down on the laptop, flapping and kicking, up, a head shake, then
                // the laptop put away, once, theme::kMascotFaceplantMs — the plan's usage limit stopped his turn
    kSkipTrip,  // the idle's skip tripping, alone, once, theme::kMascotSkipTripMs — for the stage and the specimen: the
                // deck's idle trips now and then by itself (kIdle); a reaction, played to its end
    kIdleInPlace, // the idle kept in its column — the standby page's: no walk, no skip; each stretch after the first
                  // lean a lean or the look-around at even odds, from a hash of when it began (AGENT_DECK 9.3)
};

// WHAT A MASCOT IS DOING, as a screen keeps it for its page: `move` begun at `since` (an lv_tick_get() time) and, once
// another is asked for, `stopped` then and `next` to follow it. kJump, kIdle and kIdleInPlace loop; kWork takes the
// laptop out once and then loops only the typing; kFinish and kCelebrate play once and then idle, still themselves, so
// asking for one again is no replay; kCelebrate plays to its end even when the next move is asked for.
inline constexpr uint32_t kStillPlaying = UINT32_MAX;
struct MascotPlay {
    MascotMove move;
    uint32_t since;
    uint32_t stopped; // kStillPlaying, or when it was asked to stop
    MascotMove next;  // stopped: the move once this one has finished
    MascotMove then;  // a reaction or the overview's celebrate next: the move once it has played, else kRest
};

// A move begun at `now`, from its start.
inline MascotPlay mascot_play(MascotMove move, uint32_t now)
{
    return {move, now, kStillPlaying, MascotMove::kRest, MascotMove::kRest};
}

// THE LOOK-AROUND ON ITS OWN (AGENT_DECK 9.3, 9.4): the idle's look-around from its start at `now` — a beat at rest,
// then the eyes dart from the 1:30 corner over the top to 12 and 10:30 — the same plan every time, so a page that
// shows only a glance can ask for it (the request screen's timed-out page, a needs-you list's avatar). Not a move of
// its own and drawing nothing new: it is the idle, begun part way along; ask_mascot(play, kRest, ...) stops it, the
// eyes and the body home in kMascotLookSettleMs.
inline constexpr uint32_t kMascotLookSettleMs = 200;
MascotPlay mascot_play_look(uint32_t now);

// A reaction — kSkip, kSpin, kDust, and kSkipTrip on the stage: played once, to its end, as its event comes.
inline bool mascot_reaction(MascotMove move)
{
    return move == MascotMove::kSkip || move == MascotMove::kSpin || move == MascotMove::kDust ||
           move == MascotMove::kSkipTrip;
}

// Asks for `move` at `now`: the move playing carries on; another waits for it to finish — the work puts its laptop away
// from wherever it had got to — a faceplant under way in it playing on as kFaceplant first — the jump lands and
// settles, the finish ends the swing it is in and puts the flag away, the idle skips its hold and comes home, a skip in
// it playing to its end — and then begins from its start.
void ask_mascot(MascotPlay &play, MascotMove move, uint32_t now);

// THE LIMIT HIT AT WORK (AGENT_DECK 9.3, 9.4): asks for `move` at `now` as ask_mascot() does, but work typing — past
// taking the laptop out, no sweat on — falls into kFaceplant from its start, and a faceplant already under way in the
// work carries on as kFaceplant from where it is, the laptop put away after it; then `move`. Anything else is
// ask_mascot().
void fall_mascot(MascotPlay &play, MascotMove move, uint32_t now);

// Plays `reaction` — one of those, or kCelebrate — once at `now` and then `move`: the move playing finishes as it would
// for any ask, the reaction plays to its end, then `move` begins from its start. Asking for a move meanwhile changes only
// what follows the reaction. One still waiting is replaced, but for a waiting kCelebrate, which keeps its place.
void react_mascot(MascotPlay &play, MascotMove reaction, MascotMove move, uint32_t now);

// THE STAGE (AGENT_DECK 9.8): how a stage plays `move` once, as the deck's does — `run_ms` on its clock before it is
// asked to rest, 0 for a move that plays once and rests by itself (asked to rest as it begins); `pass_ms`, that run or
// the once move's own length, which a page divides by its frame to say about how many frames a pass takes. A pass
// lasts a little past `run_ms` while the move finishes what it is doing. kRest: both 0.
struct MascotStageRun {
    uint32_t run_ms;
    uint32_t pass_ms;
};
MascotStageRun mascot_stage_run(MascotMove move);

// Plays the mascot as `play` stands now, and on from there. The mascot's state is its root's user data — the kit's.
void play_mascot(const Mascot &m, const MascotPlay &play);

// Plays `move` from its start now; kRest stops.
void play_mascot(const Mascot &m, MascotMove move);

// A MOMENT OF A MOVE, told as the frame that shows it is drawn — what a sound keeps exact time with (AGENT_DECK 17):
// EVERY MOVEMENT ITS SOUND, the developer's rule, 2026-11-02 — every move, read off its pose as drawn, so a move made
// later sounds too. The eyes: a blink, the clock's or one a move asks for; gone happy, ^ ^; squeezed > <; the work's
// wink; a glance aside. The body: down into a squat, off the ground and back on it (past half a cell up: a walk's bob
// is a step), a leg
// lifted — a step — a lean, sway or tilt setting in, a stretch of relief, a hand going up or slammed down, a turn to a
// new view, a wipe. Props: the laptop set down, its lid flipped up, snapped shut; the flag popped up; the confetti
// bursting; a puff of dust; the sweat's beads appearing and popping. Never on a mascot's first frame, so one rebuilt
// part way through a move does not tell twice.
enum class MascotSfx : uint8_t {
    kBlink, kHappy, kChevron, kWink, kLook,               // the eyes
    kHop, kLand, kSway, kStretch, kHand, kWipe,           // the body
    kLaptopDown, kLidOpen, kLidShut, kFlag, kBurst, kPuff, // props
    kBead, kPop,                                          // the sweat
    kStep, kTurn, kHandDown,                              // the body: a step, a turn, a hand slammed down
    kCrouch,                                              // the body: down into a squat, a wind-up
    kTopple, kFlop,                                       // the faceplant: tipping over forward, the face landing
};
using MascotSfxFn = void (*)(MascotSfx sfx, void *user);
// Where this mascot's moments go; nullptr for none.
void set_mascot_sfx(const Mascot &m, MascotSfxFn fn, void *user);

// Whether the mascot is typing at this moment: kWork past taking the laptop out and not yet stopped, and not in the
// sweat's wipe or a faceplant, when the typing pauses — what the agent deck's working sound keeps time with
// (AGENT_DECK 17). Only the root is read.
bool mascot_typing(const Mascot &m);

// The move on view now and when it began (an lv_tick_get() time) — a reaction once its turn has come, not while it
// waits; false for a mascot with no state. For a sound kept in time with a move.
bool mascot_move_now(const Mascot &m, MascotMove *move, uint32_t *since);

// A move with a sound kept in time with it, under way, and when its sound's beat began (an lv_tick_get() time): a skip
// — a reaction, or now and then in the idle, the idle after a finish or a celebrate included — a walk, the idle's lean to
// look, a loop of it (kThink); a hop of the jump, a needs-you turn of two hops (kHops) or of the wave, the wave's loop or
// the entrance's hello (kWave); the dust jump. False for none, and for a move cut short. For the sounds kept in time
// with them (AGENT_DECK 17).
enum class MascotJaunt : uint8_t { kSkip, kWalk, kThink, kHop, kHops, kWave, kDust };
bool mascot_jaunt(const Mascot &m, MascotJaunt *jaunt, uint32_t *since);

}  // namespace widgets
