#include "widgets/mascot.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <utility>
#include <new>

#include "internal/mascot_frames.h"
#include "internal/mascot_theme.h"

#ifdef ESP_PLATFORM
#include "board_config.h"
static_assert(widgets::theme::kScreenWidth == board_config::kScreenWidth &&
                  widgets::theme::kScreenHeight == board_config::kScreenHeight,
              "the mascot's glass is the board's portrait screen");
#endif

/*
 * CLAUDE'S MASCOT, PORTED FROM THE AGENT DECK with its numbers unchanged (STICKY_DESIGN_TOKENS §13): its moves are
 * played at the deck's pace and seen at the e-paper glass's, whatever pose is current as each partial refresh starts —
 * about two a second — and its moments sound as that refresh starts (MASCOT §6; the person's choice, 2026-11-24). Its
 * colours are DESIGN → Colors → The mascot's greys, fills a partial refresh draws as dithers; every part is a solid box
 * with no radius, and a tilted canvas is drawn by LVGL's transform with the display's antialiasing off (LVGL's default at
 * 8-bit colour), so no blended grey reaches the glass. An "AGENT_DECK n" in the comments below is the deck's own doc,
 * section n, in its repo — as are its DESIGN_TOKENS, HARDWARE_SPECS and HOST_SIMULATOR; this repo's are cited by §.
 */

namespace widgets {

namespace {

/*
 * THE REST POSE, in cells (x, y, width, height) on the 10 x 8 grid — the source rig's rectangles at a tenth of
 * its 107-unit width, each landing on whole cells (DESIGN_TOKENS 8): the body over the top six rows,
 * a hand either side of it two rows down, the eyes a row down and a cell in from each side, the legs on the two
 * rows below the body.
 */
struct Cells {
    int32_t x, y, w, h;
};

constexpr Cells kBody = {1, 0, 8, 6};
constexpr Cells kHands[2] = {{0, 2, 2, 2}, {8, 2, 2, 2}};
constexpr Cells kEyes[2] = {{2, 1, 1, 1}, {7, 1, 1, 1}};
constexpr Cells kLegs[4] = {{1, 6, 1, 2}, {3, 6, 1, 2}, {6, 6, 1, 2}, {8, 6, 1, 2}};

static_assert(kBody.x + kBody.w <= theme::kMascotCols && kLegs[0].y + kLegs[0].h == theme::kMascotRows,
              "the rest pose fills the grid");

/* THE HANDS REACH A CELL PAST THE BODY ON EACH SIDE, and a child draws outside its parent only as far as the
 * parent's extra draw size reaches — overflow_visible alone widens the clip by that and no more (LVGL 9.6,
 * lv_refr.c). So the body declares a cell of it: its hands stay its children, and a lean carries them. */
void ext_draw_cb(lv_event_t *event)
{
    const auto cell = static_cast<int32_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
    lv_event_set_ext_draw_size(event, cell);
}

/* One part: a plain box in `colour`, no radius, taking no input. */
lv_obj_t *make_part(lv_obj_t *parent, Cells c, int32_t cell, lv_color_t colour)
{
    lv_obj_t *part = lv_obj_create(parent);
    lv_obj_remove_style_all(part);
    lv_obj_set_pos(part, c.x * cell, c.y * cell);
    lv_obj_set_size(part, c.w * cell, c.h * cell);
    lv_obj_set_style_bg_color(part, colour, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(part, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollable(part, false);
    lv_obj_set_clickable(part, false);
    return part;
}

/*
 * THE JUMP (AGENT_DECK 9.3): the sheet's hop (the jump's contact sheet) given the principles of animation
 * the developer asked for, 2026-10-28 — anticipation, squash and stretch, follow-through, a cubic ease on every
 * segment. A key a pose, in px at the deck's cell (kRefCell; scaled to any other): `lift` the body's top above rest,
 * `legs` their length, `tall` the body's height past rest (squash < 0 < stretch), `wide` how far each side it widens,
 * `hands` the hands' top below the body's, `bend` how far the feet swing out at the knee — the outer legs that far,
 * the inner half as far — so the legs curve while it is in the air; `ease` takes the segment into the key. Every pose
 * on the ground has its feet on it — lift = body height + legs − 48 — so the squash lowers the body, never the feet.
 *
 *   rest; the wind-up: sinking into a crouch, squashed, the hands sagging with the weight, a held beat of tension;
 *   the launch: an explosive stretch, the hands dragging; easing up to the hang, the hands catching up; the fall
 *   accelerating, stretched, the legs reaching, the hands floating; the impact: a deep squash, the hands slammed down
 *   and splayed; then a wobble — the body springing up tall 2 px, then squat 1 px, a swing every 100 ms so the eye
 *   can follow it, the hands jiggling a beat behind — and the settle.
 */
enum class Ease : uint8_t { kLinear, kIn, kOut, kInOut };

struct JumpKey {
    int16_t t; // ms into the loop
    int8_t lift;
    int8_t legs;
    int8_t tall;
    int8_t wide;
    int8_t hands;
    int8_t bend;
    Ease ease;
};

constexpr int32_t kRefCell = 6; // the cell the keys are drawn at, scaled to the mascot's
constexpr int32_t kRefRise = 12; // the highest lift, which kMascotJumpRise stands for
constexpr JumpKey kJump[] = {
    {0, 0, 12, 0, 0, 12, 0, Ease::kLinear},     // rest
    {300, 0, 12, 0, 0, 12, 0, Ease::kLinear},   //
    {500, -5, 9, -2, 1, 15, 0, Ease::kInOut},   // the crouch
    {600, -6, 8, -2, 1, 16, 0, Ease::kInOut},   // tension
    {680, 7, 15, 2, -1, 17, 1, Ease::kOut},     // the launch: stretched, the hands dragging, the legs starting to bend
    {880, 12, 12, 0, 0, 10, 2, Ease::kOut},     // the top, the hands caught up, the legs curved
    {980, 11, 12, 0, 0, 9, 2, Ease::kInOut},    // the hang
    {1180, 3, 14, 1, 0, 8, 0, Ease::kIn},       // touchdown: stretched, the legs straight and reaching, hands floating
    {1250, -6, 8, -2, 2, 17, 0, Ease::kOut},    // the impact: squashed, the hands slammed and splayed
    {1350, 2, 12, 2, -1, 9, 0, Ease::kOut},     // the wobble: sprung up tall and narrow, 2 px,
    {1450, -1, 12, -1, 1, 14, 0, Ease::kInOut}, //   then squat and wide, 1 px, the hands jiggling behind,
    {1550, 0, 12, 0, 0, 12, 0, Ease::kInOut},   // settled
    {static_cast<int16_t>(theme::kMascotJumpMs), 0, 12, 0, 0, 12, 0, Ease::kLinear},
};
static_assert(kRefRise * theme::kMascotCell == theme::kMascotJumpRise * kRefCell,
              "the keys' rise scales to the deck's: they are drawn at kRefCell and scaled to any cell");

constexpr int32_t highest_lift()
{
    int32_t top = 0;
    for (const JumpKey &k : kJump) {
        top = k.lift > top ? k.lift : top;
    }
    return top;
}
static_assert(highest_lift() <= kRefRise, "a key lifts past kMascotJumpRise, into the lines above the mascot");

/* HOW FAR A POSE DRAWS PAST THE GRID — below by its feet, out at the sides by the splay or a swung foot — kept within
 * the cell of extra draw room the root and the body give their parts (make_mascot()). */
constexpr bool overhang_within_a_cell()
{
    for (const JumpKey &k : kJump) {
        if (k.legs + k.tall - 2 * kRefCell > kRefCell || k.wide > kRefCell || k.bend + k.wide > kRefCell) {
            return false;
        }
    }
    return true;
}
static_assert(overhang_within_a_cell(), "a key draws past the cell of room around the mascot; it would be clipped");

/* On the ground: the feet on it — the body's height plus the legs, less the lift, is the rest's 48 px. */
constexpr bool grounded(const JumpKey &k) { return k.lift == k.legs + k.tall - 2 * kRefCell; }

/* A cubic ease on 0..1000 per mille. */
int32_t ease(Ease e, int32_t x)
{
    const auto cube = [](int64_t v) { return v * v / 1000 * v / 1000; };
    switch (e) {
    case Ease::kIn: return static_cast<int32_t>(cube(x));
    case Ease::kOut: return static_cast<int32_t>(1000 - cube(1000 - x));
    case Ease::kInOut:
        return x < 500 ? static_cast<int32_t>(4 * cube(x)) : static_cast<int32_t>(1000 - 4 * cube(1000 - x));
    case Ease::kLinear: break;
    }
    return x;
}

/* a × b / c rounded to the nearest, for either sign. */
int32_t scale(int32_t a, int32_t b, int32_t c)
{
    const int64_t n = int64_t{a} * b;
    return static_cast<int32_t>(n >= 0 ? (n + c / 2) / c : (n - c / 2) / c);
}

/*
 * THE WORKING LOOP'S PHYSICS (AGENT_DECK 9.3): its frames step from picture to picture, so a motion
 * track runs over them as the jump's keys do, a cubic ease each. The mascot is squishy — the developer's rule,
 * 2026-10-29: every move shows its momentum, its impact and a bounce. So the tilt as it winks overshoots and wobbles
 * before it holds; it squashes to wind up and springs up stretched to hop right as it turns, and lands squished hard,
 * settling tall, squat, tall; the laptop set down shut lands with a squashed impact and bounces twice, the body
 * wobbling as the lid flips up; the crouch to the keys lands with a squash; each keystroke is an impact
 * (kStrike); the typing done, a stretch of relief; the lid snapped shut, a squash and a bounce; and the hop home lands
 * as the hop out did. Per key: `lift` px up, `wide` and `tall` the body's stretch across and up in per mille about its
 * feet, `laptop` px the laptop on the floor is bounced up — at the deck's cell (kRefCell) — and `lean` the body's tilt
 * to the right about its right foot, in 0.1° (LVGL's rotation unit).
 */
struct WorkKey {
    int16_t t; // ms into the loop
    int8_t lift;
    int16_t wide;
    int16_t tall;
    int8_t laptop;
    int16_t lean;
    Ease ease;
};

constexpr WorkKey kWorkMotion[] = {
    {0, 0, 1000, 1000, 0, 0, Ease::kLinear},       // rest
    {200, 0, 1000, 1000, 0, 0, Ease::kLinear},     // the wink…
    {260, 0, 1040, 960, 0, 55, Ease::kOut},        // …tilted right past where it holds, a little squashed,
    {310, 0, 1010, 990, 0, 38, Ease::kInOut},      // rocking back,
    {360, 0, 1020, 980, 0, 46, Ease::kInOut},      // and over again,
    {420, 0, 1010, 990, 0, 42, Ease::kInOut},      // held, the arms dropping
    {500, 0, 1080, 890, 0, 15, Ease::kInOut},      // the wind-up, squashed, straightening
    {575, 5, 940, 1100, 0, 0, Ease::kOut},         // turned for the laptop, hopping right: sprung up, stretched
    {668, 0, 980, 1040, 0, 0, Ease::kIn},          // down, still stretched as it falls
    {690, 0, 1150, 830, 0, 0, Ease::kOut},         // landing: squished hard…
    {730, 0, 950, 1070, 0, 0, Ease::kInOut},       // …and an elastic settle: springing up tall,
    {765, 0, 1040, 965, 0, 0, Ease::kInOut},       // squat,
    {800, 0, 985, 1015, 0, 0, Ease::kInOut},       // tall,
    {835, 0, 1000, 1000, 0, 0, Ease::kInOut},      // settled
    {869, 0, 1000, 1000, 0, 0, Ease::kLinear},     // the laptop set down shut…
    {900, 0, 1100, 880, 6, 0, Ease::kOut},         // …with a squashed impact, the laptop bouncing up
    {935, 0, 960, 1050, 0, 0, Ease::kIn},          // the wobble, tall, as the lid flips up; the laptop down again
    {975, 0, 1030, 975, 2, 0, Ease::kOut},         // squat; a second, smaller bounce
    {1010, 0, 990, 1010, 0, 0, Ease::kIn},         //
    {1069, 0, 1000, 1000, 0, 0, Ease::kInOut},     // crouching to the keys…
    {1095, 0, 1070, 940, 0, 0, Ease::kOut},        // …landing in it with a squash,
    {1125, 0, 990, 1010, 0, 0, Ease::kInOut},      // and up again as the typing starts
    {1135, 0, 1000, 1000, 0, 0, Ease::kInOut},     //
    {2404, 0, 1000, 1000, 0, 0, Ease::kLinear},    // done typing…
    {2445, 2, 960, 1050, 0, 0, Ease::kOut},        // …a stretch of relief
    {2505, 0, 1000, 1000, 0, 0, Ease::kIn},        //
    {2672, 0, 1000, 1000, 0, 0, Ease::kLinear},    // the lid snapped shut…
    {2695, 0, 1070, 940, 4, 0, Ease::kOut},        // …a squash, the laptop bouncing
    {2720, 0, 985, 1015, 0, 0, Ease::kInOut},      //
    {2739, 0, 1000, 1000, 0, 0, Ease::kInOut},     // picked up
    {2873, 0, 1000, 1000, 0, 0, Ease::kLinear},    // the laptop put away, turning back…
    {2905, 5, 940, 1100, 0, 0, Ease::kOut},        // …hopping home: sprung up, stretched
    {2939, 0, 980, 1040, 0, 0, Ease::kIn},         // down, still stretched
    {2960, 0, 1150, 830, 0, 0, Ease::kOut},        // landing: squished hard…
    {3000, 0, 950, 1070, 0, 0, Ease::kInOut},      // …springing up tall,
    {3035, 0, 1040, 965, 0, 0, Ease::kInOut},      // squat,
    {3070, 0, 985, 1015, 0, 0, Ease::kInOut},      // tall,
    {3105, 0, 1000, 1000, 0, 0, Ease::kInOut},     // settled
    {static_cast<int16_t>(theme::kMascotWorkMs), 0, 1000, 1000, 0, 0, Ease::kLinear},
};

/* EACH KEYSTROKE IS AN IMPACT while it types furiously: through each GIF frame of the typing the body squashes and
 * wobbles back and the laptop on the floor bounces — at the board's 33 ms step a strike and its rebound. */
constexpr WorkKey kStrike[] = {
    {0, 0, 1000, 1000, 0, 0, Ease::kLinear},
    {12, 0, 1050, 930, 3, 0, Ease::kOut},
    {36, 0, 975, 1030, 1, 0, Ease::kInOut},
    {66, 0, 1000, 1000, 0, 0, Ease::kInOut},
};
constexpr uint32_t kTypingFirstFrame = 17; // the GIF frame the typing starts on (f18)
constexpr uint32_t kTypingEndFrame = 36;   // and the one after it ends (f37, the hands up)

/*
 * THE WIPE (AGENT_DECK 9.3): the sweat's second typing loop, its keys and hand the draft's, approved by the developer,
 * 2026-10-30, but for the hand's step at 355 ms, a row higher to clear his eye. The typing stops and the right hand
 * comes up off the keys; he ducks his head into it, held; it drags across his brow, pulling him up tall, the beads
 * bursting as it reaches them; it flicks off his side at the top of the stretch; the sigh, all the air out of him, and
 * the elastic settle; the hand back on the keys; a wind-up, and the typing again. No lean: the tilt turns the whole
 * canvas, and the laptop is on the floor.
 */
constexpr WorkKey kWipe[] = {
    {0, 0, 1000, 1000, 0, 0, Ease::kLinear},
    {70, 0, 980, 1035, 0, 0, Ease::kOut},     // up off the keys
    {130, 0, 985, 1020, 0, 0, Ease::kInOut},  //
    {190, 0, 1075, 925, 0, 0, Ease::kOut},    // the head ducks into the hand: the wind-up
    {290, 0, 1085, 915, 0, 0, Ease::kInOut},  // held
    {385, 0, 975, 1060, 0, 0, Ease::kIn},     // the wipe drags him up…
    {445, 0, 965, 1085, 0, 0, Ease::kOut},    // …to the flick, stretched
    {530, 0, 1000, 1000, 0, 0, Ease::kInOut}, //
    {640, 0, 1130, 855, 0, 0, Ease::kOut},    // the sigh: squashed
    {700, 0, 950, 1065, 0, 0, Ease::kInOut},  // the elastic settle: tall,
    {745, 0, 1040, 965, 0, 0, Ease::kInOut},  // squat,
    {790, 0, 985, 1015, 0, 0, Ease::kInOut},  // tall,
    {835, 0, 1000, 1000, 0, 0, Ease::kInOut}, // settled
    {1100, 0, 1000, 1000, 0, 0, Ease::kLinear},
    {1190, 0, 1060, 945, 0, 0, Ease::kOut},   // back to it: a wind-up
    {static_cast<int16_t>(theme::kMascotSweatMs / 2), 0, 1000, 1000, 0, 0, Ease::kInOut},
};

/* The hand through the wipe, stepping from pixel to pixel as a frame does: its top-left, in kWorkPoses's columns and
 * rows, from `t` ms into the wipe. */
struct WipeHand {
    int16_t t;
    int8_t col, row;
};

constexpr WipeHand kWipeHand[] = {
    {0, 17, 13}, {70, 17, 10}, {130, 15, 6}, {190, 14, 6},  // up to the far end of his brow, held
    {290, 16, 6}, {325, 19, 6}, {355, 22, 6}, {385, 25, 7}, // across it, clear of his eye, to the beads
    {415, 27, 7}, {445, 28, 8}, {480, 29, 9}, {540, 28, 9}, // the flick off his side
    {640, 26, 11}, {690, 23, 12}, {740, 20, 13}, {790, 17, 13}, // and down to the keys
};
constexpr int32_t kSweatPopAtMs = 355; // into the wipe: the hand reaches the beads, and they burst
constexpr int32_t kSweatPopMs = 190;
static_assert(kWipeHand[6].t == kSweatPopAtMs, "the beads burst as the hand reaches them");

/* THE HOP ACROSS: from the turn on, the frames are drawn detail::kTurnedShift pixels right, so the mascot and the open
 * laptop are centred on the screen together. The hop as it turns carries it there, and the hop as it turns back
 * carries it home — the developer's ask, 2026-10-28. Per key, `back` the per mille of that shift it is drawn back to
 * the left: all of it the instant a turned frame first shows (f9) and up to the instant the rest frame returns
 * (f45), so it never jumps. */
struct GlideKey {
    int16_t t; // ms into the loop
    int16_t back;
    Ease ease;
};

constexpr GlideKey kGlide[] = {
    {0, 0, Ease::kLinear},
    {533, 0, Ease::kLinear},     // the rest frames, home
    {534, 1000, Ease::kLinear},  // the turned frame shows (f9): drawn back home…
    {668, -60, Ease::kInOut},    // …hopped across as the laptop comes out (f11), sliding a little past on landing,
    {705, 25, Ease::kInOut},     // springing back,
    {740, 0, Ease::kInOut},      // and settled
    {2872, 0, Ease::kLinear},    // turning back (f44)…
    {2938, 1000, Ease::kInOut},  // …hopped home…
    {2939, 0, Ease::kLinear},    // …as the rest frame returns (f45),
    {2965, 40, Ease::kOut},      // sliding a little past home on landing, to the left,
    {2995, -15, Ease::kInOut},   // springing back,
    {3025, 0, Ease::kInOut},     // and settled
    {static_cast<int16_t>(theme::kMascotWorkMs), 0, Ease::kLinear},
};
static_assert(8 * theme::kMascotWorkMs / detail::kWorkGifFrames == 534 &&
                  44 * theme::kMascotWorkMs / detail::kWorkGifFrames == 2939,
              "the hop across must let go on the frames the turn shows and the rest returns");

/* A pose in px, as pose() takes it. */
struct Pose {
    int32_t lift, legs, tall, wide, hands, bend;
    bool operator==(const Pose &) const = default;
};

/* The working loop's physics at an instant, in px and per mille. */
struct WorkPose {
    int32_t lift, wide, tall, laptop, lean, x;
    bool operator==(const WorkPose &) const = default;
};

/* The keys either side of `t`, eased. */
template <size_t N> WorkPose motion_at(const WorkKey (&keys)[N], int32_t t, int32_t cell)
{
    size_t i = 0;
    while (i + 2 < N && keys[i + 1].t <= t) {
        i++;
    }
    const WorkKey &a = keys[i];
    const WorkKey &b = keys[i + 1];
    const int32_t span = b.t - a.t;
    const int32_t x = ease(b.ease, std::clamp<int32_t>(t - a.t, 0, span) * 1000 / span);
    const auto at = [&](int32_t from, int32_t to) { return from + scale(to - from, x, 1000); };
    return {scale(at(a.lift, b.lift), cell, kRefCell), at(a.wide, b.wide), at(a.tall, b.tall),
            scale(at(a.laptop, b.laptop), cell, kRefCell), at(a.lean, b.lean), 0};
}

/* The hop across at `t`: px the frame is drawn left of where it stands. */
int32_t glide_at(int32_t t, int32_t cell)
{
    size_t i = 0;
    while (i + 2 < std::size(kGlide) && kGlide[i + 1].t <= t) {
        i++;
    }
    const GlideKey &a = kGlide[i];
    const GlideKey &b = kGlide[i + 1];
    const int32_t span = b.t - a.t;
    const int32_t x = ease(b.ease, std::clamp<int32_t>(t - a.t, 0, span) * 1000 / span);
    const int32_t back = a.back + scale(b.back - a.back, x, 1000);
    return scale(detail::kTurnedShift * (cell / 2), back, 1000);
}

/* The track, and through the typing each GIF frame's strike on top of it. */
WorkPose work_motion_at(uint32_t t, int32_t cell)
{
    WorkPose w = motion_at(kWorkMotion, static_cast<int32_t>(t), cell);
    w.x = -glide_at(static_cast<int32_t>(t), cell);
    const uint32_t gif = t * detail::kWorkGifFrames / theme::kMascotWorkMs;
    if (gif >= kTypingFirstFrame && gif < kTypingEndFrame) {
        const auto into = static_cast<int32_t>(t - gif * theme::kMascotWorkMs / detail::kWorkGifFrames);
        const WorkPose k = motion_at(kStrike, into, cell);
        w.wide = w.wide * k.wide / 1000;
        w.tall = w.tall * k.tall / 1000;
        w.laptop += k.laptop;
    }
    return w;
}

/*
 * THE FINISH (AGENT_DECK 9.3): the checkered flag of the finish's contact sheet, waved overhead, given the
 * squishy physics the developer asks of every move, 2026-10-29 — enthusiastic, energetic, wobbling with it. A wind-up,
 * squashed, leaning away, the hand cocked and the eyes gone happy (^ ^); the pop, sprung up stretched as the arm shoots
 * up and the flag comes up from behind him, edge-on; a squashed landing, the cloth flopping and spreading, the elastic
 * settle. Then five swings of the flag sheet's frames 4–12: each throw swings him across onto the legs he had raised,
 * stomping down with the body squished and the lean overshooting, rocking back and over again, as the legs he leans
 * away from rise — a pixel shorter, the knees bent; the hand thrusts up as it throws the flag over and the free hand
 * swings against it. Last, the flag yanked down behind him, a dip, a little hop of satisfaction, a squashed landing
 * and the wobble; his eyes open, and he rests. The body and the flag tilt about his hips (the canvas's transform); the
 * legs stay planted beneath (Mascot::pixel_legs). Per key, in px at the deck's cell (kFinishRefCell): `lift`, `wide`
 * and `tall` the body's stretch about its feet in per mille, `lean` its tilt to the right in 0.1°, `x` its sway.
 */
struct FinishKey {
    int16_t t; // ms into the track
    int8_t lift;
    int16_t wide;
    int16_t tall;
    int16_t lean;
    int8_t x;
    Ease ease;
};

constexpr int32_t kFinishRefCell = 8;
constexpr int32_t kFlagStepMs = 67;                  // a flag frame: two of the sheet's 30 fps frames
constexpr int32_t kWaveFromMs = 531;                 // the first swing, from the sheet's frame 4
constexpr int32_t kWaveCycleMs = 9 * kFlagStepMs;    // one swing across and back: frames 4–12
constexpr int32_t kWaveCycles = 5;
constexpr int32_t kWaveEndMs = kWaveFromMs + kWaveCycles * kWaveCycleMs;
constexpr int32_t kFinishCloseMs = 520;
constexpr int32_t kFinishTrackMs = kWaveEndMs + kFinishCloseMs;

constexpr FinishKey kFinishOpen[] = {
    {0, 0, 1000, 1000, 0, 0, Ease::kLinear},
    {60, 0, 1000, 1000, 0, 0, Ease::kLinear},
    {150, 0, 1090, 880, -25, 0, Ease::kOut},   // the wind-up: squashed, leaning away from the raise
    {210, 0, 1100, 870, -30, 0, Ease::kInOut}, // tension
    {275, 5, 920, 1130, 0, 0, Ease::kOut},     // the pop: sprung up stretched, the arm and the flag shooting up
    {330, 0, 970, 1060, 0, 0, Ease::kIn},      // falling, still stretched
    {365, 0, 1140, 850, 0, 0, Ease::kOut},     // the impact: squashed
    {410, 0, 950, 1070, 0, 0, Ease::kInOut},   // the elastic settle: tall,
    {455, 0, 1040, 965, 0, 0, Ease::kInOut},   // squat,
    {500, 0, 985, 1015, 0, 0, Ease::kInOut},   // tall,
    {kWaveFromMs, 0, 1000, 1000, 40, 2, Ease::kInOut},
};
/* One swing, from the sheet's frame 4: the cloth across to the glass's left (frames 4–8), then back (9–12). */
constexpr FinishKey kFinishWave[] = {
    {0, 0, 1000, 1000, 40, 2, Ease::kLinear},    // leaning on his left legs (the glass's right), the others raised
    {80, 0, 1000, 1000, -10, 0, Ease::kIn},      // swung across, faster and faster, onto the raised legs…
    {115, 0, 1130, 860, -55, -3, Ease::kOut},    // …the stomp: the body squished, the lean overshooting
    {175, 0, 965, 1045, -30, -2, Ease::kOut},    // rocking back, springing tall,
    {235, 0, 1020, 985, -42, -2, Ease::kInOut},  // and over again
    {300, 0, 1000, 1000, -40, -2, Ease::kInOut}, //
    {380, 0, 1000, 1000, 10, 0, Ease::kIn},      // swung back…
    {415, 0, 1130, 860, 55, 3, Ease::kOut},      // …the other stomp, squished
    {475, 0, 965, 1045, 30, 2, Ease::kOut},      //
    {535, 0, 1020, 985, 42, 2, Ease::kInOut},    //
    {kWaveCycleMs, 0, 1000, 1000, 40, 2, Ease::kInOut},
};
constexpr FinishKey kFinishClose[] = {
    {0, 0, 1000, 1000, 40, 2, Ease::kLinear},
    {90, 0, 1070, 920, -10, 0, Ease::kOut},  // the flag yanked down, the raised legs landing: a dip
    {170, 6, 950, 1090, 0, 0, Ease::kOut},   // the flag gone behind him: a little hop of satisfaction
    {230, 0, 980, 1040, 0, 0, Ease::kIn},    //
    {260, 0, 1130, 860, 0, 0, Ease::kOut},   // the landing: squashed
    {305, 0, 955, 1065, 0, 0, Ease::kInOut}, // tall,
    {350, 0, 1035, 970, 0, 0, Ease::kInOut}, // squat,
    {395, 0, 990, 1010, 0, 0, Ease::kInOut}, // tall,
    {440, 0, 1000, 1000, 0, 0, Ease::kInOut}, // settled
    {kFinishCloseMs, 0, 1000, 1000, 0, 0, Ease::kLinear},
};

/* HALF SPEED, THEN UP TO SPEED — the developer's ask, 2026-10-29: the opening and the first swing play at half speed,
 * then the pace climbs evenly over the next two swings' worth of real time to full speed, which the rest keeps. From
 * real ms since it began to ms into the track. */
constexpr int32_t kFinishSlowMs = 2 * (kWaveFromMs + kWaveCycleMs);
constexpr int32_t kFinishRampMs = 8 * kWaveCycleMs / 3; // the ramp's track covers two swings: rate ½ to 1, ¾ on average
static_assert(kFinishRampMs * 3 == 8 * kWaveCycleMs, "the ramp covers whole swings");

int32_t finish_track_at(uint32_t real)
{
    const auto r = static_cast<int32_t>(std::min<uint32_t>(real, theme::kMascotFinishMs));
    if (r < kFinishSlowMs) {
        return r / 2;
    }
    const int32_t d = r - kFinishSlowMs;
    if (d < kFinishRampMs) {
        return kFinishSlowMs / 2 + d / 2 + static_cast<int32_t>(int64_t{d} * d / (4 * kFinishRampMs));
    }
    return kFinishSlowMs / 2 + 3 * kFinishRampMs / 4 + (d - kFinishRampMs);
}
static_assert(theme::kMascotFinishMs ==
                  kFinishSlowMs + kFinishRampMs + (kFinishTrackMs - kFinishSlowMs / 2 - 3 * kFinishRampMs / 4),
              "theme::kMascotFinishMs is the finish's track played at its pace");

/* Stopped: where it stopped on the track, and from there on at full speed — the swing it was in played out, or the
 * opening, then straight to the close — so the flag is never cut off mid-throw. Its track ms `d` real ms after the
 * stop, and how long that takes. */
int32_t finish_stopped_at(const MascotPlay &play) { return finish_track_at(play.stopped - play.since); }

int32_t finish_close_from(int32_t s)
{
    if (s < kWaveFromMs) {
        return kWaveFromMs;
    }
    return s < kWaveEndMs ? kWaveFromMs + (s - kWaveFromMs + kWaveCycleMs - 1) / kWaveCycleMs * kWaveCycleMs : s;
}

int32_t finish_outro_at(int32_t s, uint32_t d)
{
    const int32_t until = finish_close_from(s);
    const auto dd = static_cast<int32_t>(std::min<uint32_t>(d, theme::kMascotFinishMs));
    const int32_t t = s >= kWaveEndMs ? s + dd : (dd < until - s ? s + dd : kWaveEndMs + dd - (until - s));
    return std::min(t, kFinishTrackMs);
}

uint32_t finish_outro_ms(int32_t s)
{
    return static_cast<uint32_t>(s >= kWaveEndMs ? kFinishTrackMs - s : finish_close_from(s) - s + kFinishCloseMs);
}

enum class Hand : uint8_t { kRest, kLow, kMid, kUp };
enum class Raised : uint8_t { kNone, kLeft, kRight, kBoth };

/* A front-view pixel move at an instant — the finish's or the celebrate's: the track's physics in px, per mille and
 * 0.1°; the hands' top-left GIF cells; the flag frame (into detail::kFlagFrames, or -1 for none), the GIF cell its
 * pole's foot is in on the raised hand's top row, and whether that foot shows over the hand; the confetti bursts' ms
 * in, or -1. */
struct PixelPose {
    int32_t lift, wide, tall, lean, x;
    int32_t crouch;  // px the body drops into a squat, the feet planted; negative, rises on stretching legs
    int32_t sway;    // px the body leans over its planted feet, the legs slanting to follow
    int32_t swing;   // px the feet swing out at the knee in the air — the outer legs that far, the inner half
    int32_t travel;  // px the whole mascot has walked across the glass (every part's translation, set_travel())
    int8_t knee_dir; // the raised knees all bend this way, -1 left, 1 right; 0 each out from the body's centre
    int8_t look;     // pixels the eyes are moved, -1 left, 1 right
    int8_t look_y;   // pixels the eyes are moved, -1 up, 1 down
    int8_t view;     // 0 the front; 1–7 turned to its left round to the front again (kTurnViews), legs and all
    int16_t dust;    // a puff of dust kicked up this many ms ago, counted from 1; 0 for none
    int8_t left_shear, right_shear; // pixels a raised hand's top two rows lead its swing
    bool chevron;    // the eyes > <
    bool shut;       // the eyes shut to their lower row, a blink the move asks for
    bool happy;      // the eyes ^ ^
    int8_t left_col, left_row, right_col, right_row;
    Raised raised;   // the legs off the ground
    int8_t flag;
    int8_t grip_col, grip_row;
    bool grip_over;
    std::array<int16_t, 2> bursts;
    /* THE SKIP TRIP (skip_pose_at()): `trip`, `trip_u` ms since he caught his foot, through the faceplant, the turn
     * back and the wind-up — its moments are its table's (tell_trip()), the front view's own hushed from the catch to
     * the turn back; stumbling: the front view's stumble, no blink over it; down: the faceplant's frame `down_ms` into
     * it, on the trip's canvas, mirrored going right. All false and 0 in every other pose. */
    bool trip;
    int16_t trip_u;
    bool stumble;
    bool down;
    bool down_mirror;
    int16_t down_ms;
    bool operator==(const PixelPose &) const = default;
};

template <size_t N> void finish_track(PixelPose &f, const FinishKey (&keys)[N], int32_t t, int32_t cell)
{
    size_t i = 0;
    while (i + 2 < N && keys[i + 1].t <= t) {
        i++;
    }
    const FinishKey &a = keys[i];
    const FinishKey &b = keys[i + 1];
    const int32_t span = b.t - a.t;
    const int32_t x = ease(b.ease, std::clamp<int32_t>(t - a.t, 0, span) * 1000 / span);
    const auto at = [&](int32_t from, int32_t to) { return from + scale(to - from, x, 1000); };
    f.lift = scale(at(a.lift, b.lift), cell, kFinishRefCell);
    f.wide = at(a.wide, b.wide);
    f.tall = at(a.tall, b.tall);
    f.lean = at(a.lean, b.lean);
    f.x = scale(at(a.x, b.x), cell, kFinishRefCell);
}

constexpr int8_t kGripCol = 33; // the raised hand's third column, the pole through it
constexpr int8_t kGripRow = 7;  // its top row

/* Every flag frame the finish names is one there is. */
constexpr bool flags_in_range()
{
    for (const uint8_t f : detail::kFlagSequence) {
        if (f >= std::size(detail::kFlagFrames)) {
            return false;
        }
    }
    return std::size(detail::kFlagSequence) == 12 && 9 < std::size(detail::kFlagFrames);
}
static_assert(flags_in_range(), "the finish names a flag frame there is not: the sheet's 12, the edge-on 9 and 10");

/* The finish's raised hand at an instant: its height, and the pixels it and the free hand are moved. */
struct FinishHand {
    Hand hand = Hand::kRest;
    int8_t dx = 0;      // pixels, raised: to the left as it throws the flag across
    int8_t dy = 0;      // pixels, raised: up as it thrusts
    int8_t free_dy = 0; // pixels, the left hand swinging against the flag
};

/* The finish's hands `t` ms in, as the flag waves: the flag's frame, the pole's grip and the raised legs into `f`. */
FinishHand finish_wave_hand(PixelPose &f, int32_t t)
{
    FinishHand h;
    const int32_t u = (t - kWaveFromMs) % kWaveCycleMs;
    const int32_t frame = 3 + u / kFlagStepMs; // the sheet's frames 4–12
    const bool across = frame >= 4 && frame <= 7;
    h.hand = Hand::kUp;
    h.dx = across ? -1 : 0;
    h.dy = frame == 4 || frame == 9 ? -1 : 0; // thrusting as it throws the flag over
    f.flag = static_cast<int8_t>(detail::kFlagSequence[frame]);
    f.grip_col = static_cast<int8_t>(kGripCol + h.dx);
    f.grip_row = static_cast<int8_t>(kGripRow + h.dy);
    h.free_dy = across ? -1 : 1;
    f.raised = u >= 80 && u < 380 ? Raised::kRight : Raised::kLeft; // the side he leans away from
    return h;
}

/* The finish's hands `t` ms in: the flag's frame, the pole's grip and the raised legs into `f`. */
FinishHand finish_hand_at(PixelPose &f, int32_t t)
{
    FinishHand h;
    if (t < 60) {
        h.hand = Hand::kRest;
    } else if (t < 225) {
        h.hand = Hand::kLow; // cocked
    } else if (t < 245) {
        h.hand = Hand::kMid;
        f.flag = 8; // rising from behind him, edge-on
        f.grip_row = 13;
    } else if (t < kWaveFromMs) {
        h.hand = Hand::kUp;
        f.flag = t < 320 ? 8 : t < 365 ? 9 : static_cast<int8_t>(std::min<int32_t>((t - 365) / kFlagStepMs, 2));
        h.free_dy = t < 365 ? 1 : 0; // up with the pop edge-on, the cloth flopping on the impact, then spreading
    } else if (t < kWaveEndMs) {
        h = finish_wave_hand(f, t);
    } else {
        const int32_t u = t - kWaveEndMs;
        if (u < 60) {
            h.hand = Hand::kUp;
            f.flag = 0;
            f.raised = Raised::kLeft; // landing with the dip
        } else if (u < 110) {
            h.hand = Hand::kMid;
            f.flag = 8; // yanked down behind him
            f.grip_row = 11;
        } else {
            h.hand = u < 260 ? Hand::kLow : Hand::kRest;
        }
    }
    return h;
}

PixelPose finish_pose_at(int32_t t, int32_t cell)
{
    PixelPose f = {};
    f.flag = -1;
    f.bursts = {-1, -1};
    f.grip_col = kGripCol;
    f.grip_row = kGripRow;
    if (t < kWaveFromMs) {
        finish_track(f, kFinishOpen, t, cell);
    } else if (t < kWaveEndMs) {
        finish_track(f, kFinishWave, (t - kWaveFromMs) % kWaveCycleMs, cell);
    } else {
        finish_track(f, kFinishClose, t - kWaveEndMs, cell);
    }
    f.happy = t >= 90 && t < kWaveEndMs + 350;
    const FinishHand h = finish_hand_at(f, t);
    f.left_col = 15;
    f.left_row = static_cast<int8_t>(14 + h.free_dy);
    f.right_col = static_cast<int8_t>(31 + h.dx);
    f.right_row = static_cast<int8_t>(h.hand == Hand::kRest  ? 14
                                      : h.hand == Hand::kLow ? 15
                                      : h.hand == Hand::kMid ? 9 + h.dy
                                                             : 7 + h.dy);
    f.grip_over = h.hand == Hand::kUp; /* rising or yanked down behind him, the pole's foot is behind the hand too */
    return f;
}

/* Where the finish is at `now`, in ms into its track: at its pace while it plays, on at full speed once stopped. */
int32_t finish_time(const MascotPlay &play, uint32_t now)
{
    if (play.stopped == kStillPlaying) {
        return std::min(finish_track_at(now - play.since), kFinishTrackMs);
    }
    return finish_outro_at(finish_stopped_at(play), now - play.stopped);
}

/*
 * THE CELEBRATE (AGENT_DECK 9.3): the celebrate's contact sheet's stomp and confetti, given the squishy
 * physics — the developer's choices, 2026-10-29. A wind-up crouch, the eyes happy; the right arm punched up, sprung up
 * stretched and leaning into it, the first burst of confetti on its side; a squashed landing down into the wide squat,
 * the arms out, the legs short; the left arm punched up and the second burst, mirrored; the squat again; both arms
 * flung up, sinking deeper, and the big jump — the legs tucked, the hands flapping three times, each downstroke lifting
 * him a little — then the fall, a hard squashed landing with the arms splayed, the elastic settle and rest. Its track
 * is the approved draft's, played at seven tenths of its speed (theme::kMascotCelebrateMs). Per key: `lift`, `wide`,
 * `tall`, `lean` as the finish's, `crouch` px the body drops into the squat.
 */
struct CelebrateKey {
    int16_t t; // ms into the track
    int8_t lift;
    int16_t wide;
    int16_t tall;
    int16_t lean;
    int8_t crouch;
    Ease ease;
};

constexpr CelebrateKey kCelebrate[] = {
    {0, 0, 1000, 1000, 0, 0, Ease::kLinear},
    {60, 0, 1000, 1000, 0, 0, Ease::kLinear},
    {150, 0, 1090, 880, 0, 4, Ease::kOut},     // the wind-up: a squashed crouch, the eyes happy
    {190, 0, 1100, 870, 0, 5, Ease::kInOut},   // tension
    {260, 7, 920, 1120, 25, 0, Ease::kOut},    // the right arm punched up: sprung up stretched, leaning into it
    {320, 0, 980, 1050, 15, 0, Ease::kIn},     // falling
    {350, 0, 1120, 870, 0, 3, Ease::kOut},     // landing: squashed…
    {420, 0, 1100, 900, 0, 8, Ease::kInOut},   // …down into the wide squat, the arms out: the stomp
    {450, 0, 1060, 940, 0, 7, Ease::kOut},     //
    {520, 7, 920, 1120, -25, 0, Ease::kOut},   // the left arm punched up, leaning into it
    {580, 0, 980, 1050, -15, 0, Ease::kIn},    //
    {610, 0, 1120, 870, 0, 3, Ease::kOut},     // landing
    {680, 0, 1100, 900, 0, 8, Ease::kInOut},   // the squat again
    {710, 0, 1060, 940, 0, 7, Ease::kOut},     //
    {750, 0, 1110, 870, 0, 9, Ease::kInOut},   // both arms flung up, sinking deeper: the wind-up for the jump
    {830, 14, 900, 1150, 0, 0, Ease::kOut},    // the launch: sprung up stretched, the legs tucking
    {910, 16, 960, 1060, 0, 0, Ease::kInOut},  // up in the air, flapping
    {990, 14, 975, 1035, 0, 0, Ease::kInOut},  // still flapping, hanging
    {1065, 0, 985, 1050, 0, 0, Ease::kIn},     // the fall, faster and faster
    {1095, 0, 1150, 840, 0, 4, Ease::kOut},    // the impact: squashed hard, the arms splayed
    {1145, 0, 950, 1070, 0, 0, Ease::kInOut},  // the elastic settle: tall,
    {1190, 0, 1040, 965, 0, 0, Ease::kInOut},  // squat,
    {1235, 0, 985, 1015, 0, 0, Ease::kInOut},  // tall,
    {1280, 0, 1000, 1000, 0, 0, Ease::kInOut}, // settled
    {1500, 0, 1000, 1000, 0, 0, Ease::kLinear},
};
constexpr int32_t kCelebrateTrackMs = 1500;
static_assert(kCelebrate[std::size(kCelebrate) - 1].t == kCelebrateTrackMs, "the celebrate's track ends at rest");
/* SEVEN TENTHS OF THE DRAFT'S SPEED — the developer's pick, 2026-10-29, between half speed and full. */
static_assert(theme::kMascotCelebrateMs * 7 / 10 == kCelebrateTrackMs &&
                  (theme::kMascotCelebrateMs - 1) * 7 / 10 < kCelebrateTrackMs,
              "theme::kMascotCelebrateMs is the celebrate's track at seven tenths of its speed");

constexpr int32_t kFlapFromMs = 840;  // the flaps in the air: up, half, down and out, half — 140 ms a flap
constexpr int32_t kFlapUntilMs = 1050;
constexpr int32_t kFlapStepMs = 35;
constexpr int8_t kFlap[4][4] = {{15, 6, 31, 6}, {14, 9, 32, 9}, {13, 12, 33, 12}, {14, 9, 32, 9}};
constexpr int32_t kBurstFromMs[2] = {230, 490}; // the first as the right arm goes up, the second as the left does
constexpr int32_t kBurstMs = 8 * 125;           // the article's 8 frames, 125 ms each

PixelPose celebrate_pose_at(int32_t t, int32_t cell)
{
    PixelPose f = {};
    f.flag = -1;
    size_t i = 0;
    while (i + 2 < std::size(kCelebrate) && kCelebrate[i + 1].t <= t) {
        i++;
    }
    const CelebrateKey &a = kCelebrate[i];
    const CelebrateKey &b = kCelebrate[i + 1];
    const int32_t span = b.t - a.t;
    const int32_t x = ease(b.ease, std::clamp<int32_t>(t - a.t, 0, span) * 1000 / span);
    const auto at = [&](int32_t from, int32_t to) { return from + scale(to - from, x, 1000); };
    f.lift = scale(at(a.lift, b.lift), cell, kFinishRefCell);
    f.wide = at(a.wide, b.wide);
    f.tall = at(a.tall, b.tall);
    f.lean = at(a.lean, b.lean);
    f.crouch = scale(at(a.crouch, b.crouch), cell, kFinishRefCell);
    f.happy = t >= 60 && t < 1330;
    int8_t hands[4] = {15, 14, 31, 14};
    const auto set = [&](int8_t lc, int8_t lr, int8_t rc, int8_t rr) {
        hands[0] = lc;
        hands[1] = lr;
        hands[2] = rc;
        hands[3] = rr;
    };
    if (t >= 190 && t < 350) {
        set(15, 14, 31, 6); /* the right arm up */
    } else if (t >= 450 && t < 610) {
        set(15, 6, 31, 14); /* the left arm up */
    } else if ((t >= 350 && t < 450) || (t >= 610 && t < 710) || (t >= 1095 && t < 1145)) {
        set(13, 16, 33, 16); /* the squat's arms out wide, or splayed on the impact */
    } else if ((t >= 710 && t < kFlapFromMs) || (t >= kFlapUntilMs && t < 1095)) {
        set(15, 6, 31, 6); /* both arms up */
    } else if (t >= kFlapFromMs && t < kFlapUntilMs) {
        const int32_t phase = (t - kFlapFromMs) / kFlapStepMs % 4;
        set(kFlap[phase][0], kFlap[phase][1], kFlap[phase][2], kFlap[phase][3]);
        if (phase == 2) {
            f.lift += scale(3, cell, 2 * kFinishRefCell); /* each downstroke lifts him a little */
        }
    }
    f.left_col = hands[0];
    f.left_row = hands[1];
    f.right_col = hands[2];
    f.right_row = hands[3];
    f.raised = t >= 820 && t < 1040 ? Raised::kBoth : Raised::kNone; /* the legs tucked in the air */
    for (size_t k = 0; k < f.bursts.size(); k++) {
        const int32_t into = t - kBurstFromMs[k];
        f.bursts[k] = static_cast<int16_t>(into >= 0 && into < kBurstMs ? into : -1);
    }
    return f;
}

/* Where the celebrate is at `now`, in ms into its track: it plays to its end even when the next move is asked for. */
int32_t celebrate_time(const MascotPlay &play, uint32_t now)
{
    return static_cast<int32_t>(std::min<uint32_t>(now - play.since, theme::kMascotCelebrateMs) * 7 / 10);
}

/*
 * THE IDLE (AGENT_DECK 9.3): the overview with nobody working — the idle's contact sheet's lean to look,
 * with the article's numbers given the squishy physics, the developer's choice of the draft, 2026-10-29. A beat at
 * rest; a little wind-up into the feet; leaning to look to the glass's left — the body swayed over its planted feet,
 * the legs slanting to follow, tilted and risen on the stretching legs, the eyes a pixel into the look — past where it
 * holds, rocking back, settled; held; back through the middle a touch past and a wobble; the same to the right; home. A
 * loop of theme::kMascotIdleMs. Per key: `sway`, `rise` px at kFinishRefCell, `lean` 0.1°, `wide`, `tall` per mille,
 * `look` pixels. As he leans back home from the last look, a double blink.
 */
struct IdleKey {
    int16_t t; // ms into the loop
    int8_t sway;
    int8_t rise;
    int16_t lean;
    int16_t wide;
    int16_t tall;
    int8_t look;
    Ease ease;
};

constexpr IdleKey kIdle[] = {
    {0, 0, 0, 0, 1000, 1000, 0, Ease::kLinear},
    {600, 0, 0, 0, 1000, 1000, 0, Ease::kLinear},
    {680, 0, 0, 0, 1030, 975, 0, Ease::kOut},       // a little wind-up: settling into the feet
    {980, -4, 5, -38, 985, 1020, -1, Ease::kOut},   // leaning left to look, risen on the stretching legs, past the hold
    {1120, -3, 4, -28, 1005, 995, -1, Ease::kInOut}, // rocking back
    {1240, -3, 4, -32, 1000, 1000, -1, Ease::kInOut}, // settled, looking
    {2300, -3, 4, -32, 1000, 1000, -1, Ease::kLinear}, // held
    {2560, 1, 0, 6, 1030, 975, 0, Ease::kInOut},    // back through the middle, a touch past, landing in the feet
    {2680, 0, 0, 0, 990, 1010, 0, Ease::kInOut},    // a little wobble
    {2780, 0, 0, 0, 1000, 1000, 0, Ease::kInOut},   //
    {2860, 0, 0, 0, 1030, 975, 0, Ease::kOut},      // the wind-up the other way
    {3160, 4, 5, 38, 985, 1020, 1, Ease::kOut},     // leaning right to look, past
    {3300, 3, 4, 28, 1005, 995, 1, Ease::kInOut},   //
    {3420, 3, 4, 32, 1000, 1000, 1, Ease::kInOut},  //
    {4400, 3, 4, 32, 1000, 1000, 1, Ease::kLinear}, // held
    {4660, -1, 0, -6, 1030, 975, 0, Ease::kInOut},  // home, a touch past
    {4780, 0, 0, 0, 990, 1010, 0, Ease::kInOut},    //
    {4880, 0, 0, 0, 1000, 1000, 0, Ease::kInOut},   //
    {static_cast<int16_t>(theme::kMascotIdleMs), 0, 0, 0, 1000, 1000, 0, Ease::kLinear},
};
static_assert(kIdle[std::size(kIdle) - 1].t == theme::kMascotIdleMs, "the idle's keys run the length of its loop");
/* The looks: settled at `held`, held until `back`, home again at `home`. Stopped part way, the idle skips its hold and
 * comes home at full speed, so the next move starts from rest. */
struct IdleLook {
    int32_t from, held, back, home;
};
constexpr IdleLook kIdleLooks[2] = {{600, 1240, 2300, 2780}, {2780, 1240 + 2180, 2300 + 2100, 4880}};
static_assert(kIdle[5].t == kIdleLooks[0].held && kIdle[6].t == kIdleLooks[0].back && kIdle[9].t == kIdleLooks[0].home &&
                  kIdle[13].t == kIdleLooks[1].held && kIdle[14].t == kIdleLooks[1].back &&
                  kIdle[17].t == kIdleLooks[1].home,
              "the idle's looks are its keys'");

PixelPose idle_pose_at(int32_t t, int32_t cell)
{
    PixelPose f = {};
    f.flag = -1;
    f.bursts = {-1, -1};
    size_t i = 0;
    while (i + 2 < std::size(kIdle) && kIdle[i + 1].t <= t) {
        i++;
    }
    const IdleKey &a = kIdle[i];
    const IdleKey &b = kIdle[i + 1];
    const int32_t span = b.t - a.t;
    const int32_t x = ease(b.ease, std::clamp<int32_t>(t - a.t, 0, span) * 1000 / span);
    const auto at = [&](int32_t from, int32_t to) { return from + scale(to - from, x, 1000); };
    f.sway = scale(at(a.sway, b.sway), cell, kFinishRefCell);
    f.crouch = -scale(at(a.rise, b.rise), cell, kFinishRefCell);
    f.lean = at(a.lean, b.lean);
    f.wide = at(a.wide, b.wide);
    f.tall = at(a.tall, b.tall);
    f.look = static_cast<int8_t>(scale(at(a.look * 1000, b.look * 1000), 1, 1000));
    /* A double blink as he leans back home from the last look — the developer's ask, 2026-10-29. */
    f.shut = (t >= 4600 && t < 4710) || (t >= 4830 && t < 4940);
    f.left_col = 15;
    f.left_row = 14;
    f.right_col = 31;
    f.right_row = 14;
    return f;
}

/* Stopped: ms into the loop `d` ms after the stop, and how long it takes to be home. */
int32_t idle_outro_at(int32_t s, uint32_t d, uint32_t *finishes)
{
    for (const IdleLook &l : kIdleLooks) {
        if (s >= l.from + 1 && s < l.home) {
            const int32_t to_held = std::max<int32_t>(0, l.held - s);
            const int32_t from_back = std::max<int32_t>(s, l.back);
            const auto dd = static_cast<int32_t>(std::min<uint32_t>(d, theme::kMascotIdleMs));
            if (finishes != nullptr) {
                *finishes = static_cast<uint32_t>(to_held + (l.home - from_back));
            }
            if (s < l.back && dd < to_held) {
                return s + dd;
            }
            return std::min(from_back + dd - to_held, l.home);
        }
    }
    if (finishes != nullptr) {
        *finishes = 0;
    }
    return s;
}

/*
 * THE ROUTE OFF THE GLASS — the owner's rule (DESIGN → The mascot → On the glass), as they corrected it watching the
 * glass, 2026-11-07: a move that travels sideways — the walk, the skip — goes on from home off one side of the glass
 * until he is wholly gone, comes back from wholly past the other side and travels on, the same way, across the data
 * column's lines to home. Never on both sides at once: the travel jumps the moment he is gone, by the whole route, so
 * he is wholly past the far side. Every frame, gait and step keeps its timing and pace; only the route is longer. In px
 * at kFinishRefCell, from home (the page's mascot: theme::kMascotCentreX on the theme::kScreenWidth glass) to his right
 * edge wholly past the left side, and from wholly past the right side to home, a cell of room each for a lean or a
 * squash.
 */
constexpr int32_t to_ref(int32_t px) { return (px * kFinishRefCell + theme::kMascotCell - 1) / theme::kMascotCell; }
constexpr int32_t kMascotHalfWidth = theme::kMascotCols * theme::kMascotCell / 2;
constexpr int32_t kRouteOff = to_ref(theme::kMascotCentreX + kMascotHalfWidth + theme::kMascotCell);
constexpr int32_t kRouteBack = to_ref(theme::kScreenWidth - theme::kMascotCentreX + kMascotHalfWidth + theme::kMascotCell);
/* Whole strides of `per` px covering the route; its length. */
constexpr int32_t route_strides(int32_t per) { return (kRouteOff + kRouteBack + per - 1) / per; }

/*
 * Where travel `raw` (px at kFinishRefCell, from home, ≤ 0 as the keys write it: going left) shows on the glass along a
 * route `span` long, `dir` the way he goes. Left (-1): itself until he is wholly past the left side, then the whole
 * route on — wholly past the right side, coming home. RIGHT (1) — the owner's ask, 2026-11-07, each move's way picked
 * at random (travel_dir()): the mirror, `raw` turned round, off past the right side and back in from wholly past the
 * left, the route's jump where he is wholly gone (kRouteBack that way, kRouteOff this), so never on both sides at once.
 * The same keys, so the same gait, pace and timing either way.
 */
constexpr int32_t on_route(int32_t raw, int32_t span, int dir = -1)
{
    if (dir < 0) {
        return raw < -kRouteOff ? raw + span : raw;
    }
    return -raw > kRouteBack ? -raw - span : -raw;
}

/* HE FACES THE WAY HE GOES: a pose keyed for the left-going route, turned round for the right-going one — the lean, the
 * sway over the feet, the eyes' look and the raised knees' bend. The front-view frames are symmetric but for these. */
void face(PixelPose &f, int dir)
{
    if (dir > 0) {
        f.lean = -f.lean;
        f.sway = -f.sway;
        f.look = static_cast<int8_t>(-f.look);
        f.knee_dir = static_cast<int8_t>(-f.knee_dir);
    }
}

/*
 * THE WALK (AGENT_DECK 9.3): now and then in the overview's idle — the walk's contact sheet's look, given the squishy
 * physics; the developer's choices of the draft at its real speed, 2026-10-29, on the owner's route, 2026-11-07.
 * Looking left and leaning to it; off he walks to the left, kWalkStride px a step as in the draft, off the glass and
 * back in from past its right side, on past home by kWalkPast px (kWalkSteps + kWalkPastSteps steps); stopping with the
 * body carried past the feet, rocking back, settled, looking back home; then the hop home, its keys and timing kept as
 * they were, the owner's ask, 2026-11-07: the crouch, the hop home across, the feet swung out, a squashed landing, a
 * slide past home and the wobble, settled, looking at you. Each step lifts the other pair a pixel, every raised knee
 * bent toward where he goes, the body bobbing up and squashing as the foot lands, the hands swinging against the legs.
 * Per key: `x` the walk's raw travel and `lift`, `sway`, `crouch`, `swing` px at kFinishRefCell, `lean` 0.1°, `wide`,
 * `tall` per mille, `look`. The hop's keys are the JC deck's turned round, as he hops back the other way.
 */
struct WalkKey {
    int16_t t; // ms into the walk
    int16_t x;
    int8_t lift;
    int8_t sway;
    int8_t crouch;
    int16_t lean;
    int16_t wide;
    int16_t tall;
    int8_t look;
    int8_t swing;
    Ease ease;
};

constexpr int32_t kWalkStepMs = 300;
constexpr int32_t kWalkRaisedMs = 240; // the pair up, then the foot down
constexpr int32_t kWalkStride = 10;    // the draft's four steps out, 40 px at kFinishRefCell
constexpr int32_t kWalkFrom = 700;     // off he goes
constexpr int32_t kWalkSteps = route_strides(kWalkStride);
constexpr int32_t kWalkSpan = kWalkSteps * kWalkStride;
constexpr int32_t kWalkPastSteps = 4; // on past home: the JC deck's kWalkFar, the hop home's length
constexpr int32_t kWalkPast = kWalkPastSteps * kWalkStride;
constexpr int32_t kWalkPastMs = kWalkFrom + (kWalkSteps + kWalkPastSteps) * kWalkStepMs; // past home, the last foot down
constexpr auto kWalkHome = static_cast<int16_t>(-kWalkSpan);
/* Raw travel `px` short of home on the way back, past it (the JC deck's hop keys' x, from home toward where he stops).
 * */
constexpr int16_t past_home(int32_t px) { return static_cast<int16_t>(-kWalkSpan - px); }
static_assert(kWalkSpan - kRouteOff >= kRouteBack, "the walk comes back from wholly past the far side");
static_assert(on_route(-(kRouteOff + 1), kWalkSpan) >= kRouteBack && on_route(-(kRouteBack + 1), kWalkSpan, 1) <= -kRouteOff,
              "either way, the jump lands him wholly past the far side");
static_assert(on_route(past_home(kWalkPast + 1), kWalkSpan) == -(kWalkPast + 1) &&
                  on_route(past_home(kWalkPast + 1), kWalkSpan, 1) == kWalkPast + 1,
              "past home, either way, he is back on the glass's near side of the jump");
constexpr WalkKey kWalk[] = {
    {0, 0, 0, 0, 0, 0, 1000, 1000, 0, 0, Ease::kLinear},
    {300, 0, 0, 0, 0, 0, 1000, 1000, 0, 0, Ease::kLinear},
    {360, 0, 0, 0, 0, 0, 1030, 975, 0, 0, Ease::kOut},             // a little wind-up
    {560, 0, 0, -3, -3, -25, 990, 1015, -1, 0, Ease::kOut},        // looking left, leaning to it: going there
    {kWalkFrom, 0, 0, -2, -1, -20, 1000, 1000, -1, 0, Ease::kInOut}, // off he goes, leaning into the walk
    {kWalkPastMs, past_home(kWalkPast), 0, -2, 0, -20, 1000, 1000, -1, 0, Ease::kLinear},  // off, back in, past home
    {kWalkPastMs + 80, past_home(kWalkPast + 1), 0, -4, 0, -32, 1000, 1000, -1, 0, Ease::kOut}, // stopping: the body
                                                                                                // carried past
    {kWalkPastMs + 160, past_home(kWalkPast), 0, 1, 0, 6, 1000, 1000, 0, 0, Ease::kInOut},  // rocking back
    {kWalkPastMs + 220, past_home(kWalkPast), 0, 0, 0, 0, 1000, 1000, 1, 0, Ease::kInOut},  // settled, looking back home
    {kWalkPastMs + 340, past_home(kWalkPast), 0, 0, 6, 8, 1100, 880, 1, 0, Ease::kInOut},   // the crouch: the wind-up
    {kWalkPastMs + 400, past_home(kWalkPast), 0, 0, 7, 10, 1110, 870, 1, 0, Ease::kInOut},  //   for the hop
    {kWalkPastMs + 460, past_home(kWalkPast - 4), 7, 0, 0, 20, 920, 1120, 1, 2, Ease::kOut}, // the launch: sprung up
                                                                                             // stretched, leaning in
    {kWalkPastMs + 700, past_home(kWalkPast / 2 - 4), 18, 0, 0, 15, 970, 1050, 1, 3, Ease::kOut}, // the top: the feet
                                                                                                  // swung out, across
    {kWalkPastMs + 940, past_home(2), 3, 0, 0, 5, 980, 1060, 1, 1, Ease::kIn}, // falling, faster and faster, stretched
    {kWalkPastMs + 980, kWalkHome, 0, 0, 4, 0, 1150, 840, 0, 0, Ease::kOut},   // the impact: squashed hard
    {kWalkPastMs + 1020, past_home(-2), 0, 0, 0, 0, 1080, 920, 0, 0, Ease::kOut}, // sliding a little past home
    {kWalkPastMs + 1070, kWalkHome, 0, 0, 0, 0, 950, 1070, 0, 0, Ease::kInOut},   // the elastic settle: tall,
    {kWalkPastMs + 1120, kWalkHome, 0, 0, 0, 0, 1040, 965, 0, 0, Ease::kInOut},   // squat,
    {kWalkPastMs + 1170, kWalkHome, 0, 0, 0, 0, 985, 1015, 0, 0, Ease::kInOut},   // tall,
    {kWalkPastMs + 1220, kWalkHome, 0, 0, 0, 0, 1000, 1000, 0, 0, Ease::kInOut},  // settled, looking at you
    {static_cast<int16_t>(theme::kMascotWalkMs), kWalkHome, 0, 0, 0, 0, 1000, 1000, 0, 0, Ease::kLinear},
};
static_assert(kWalk[std::size(kWalk) - 1].t == theme::kMascotWalkMs && kWalkPastMs + 1220 + 380 == theme::kMascotWalkMs,
              "the walk's keys run its length: past home, the JC deck's hop home and settle, and its 380 ms at rest");
struct WalkLeg {
    int32_t from, steps;
    int8_t dir;
};
constexpr WalkLeg kWalkLegs[1] = {{kWalkFrom, kWalkSteps + kWalkPastSteps, -1}};
/* One step: the body bobbing up, then the foot down with a squash. (t, bob px at kFinishRefCell, wide, tall ‰) */
constexpr int16_t kWalkStep[5][4] = {{0, 0, 1000, 1000}, {120, 2, 985, 1020}, {240, 0, 1000, 1000},
                                     {270, 0, 1050, 950}, {kWalkStepMs, 0, 1000, 1000}};
constexpr Ease kWalkStepEase[5] = {Ease::kLinear, Ease::kOut, Ease::kIn, Ease::kOut, Ease::kInOut};

PixelPose walk_pose_at(int32_t t, int32_t cell, int dir)
{
    PixelPose f = {};
    f.flag = -1;
    f.bursts = {-1, -1};
    size_t i = 0;
    while (i + 2 < std::size(kWalk) && kWalk[i + 1].t <= t) {
        i++;
    }
    const WalkKey &a = kWalk[i];
    const WalkKey &b = kWalk[i + 1];
    const int32_t span = b.t - a.t;
    const int32_t x = ease(b.ease, std::clamp<int32_t>(t - a.t, 0, span) * 1000 / span);
    const auto at = [&](int32_t from, int32_t to) { return from + scale(to - from, x, 1000); };
    f.travel = scale(on_route(at(a.x, b.x), kWalkSpan, dir), cell, kFinishRefCell);
    f.lift = scale(at(a.lift, b.lift), cell, kFinishRefCell);
    f.sway = scale(at(a.sway, b.sway), cell, kFinishRefCell);
    f.crouch = scale(at(a.crouch, b.crouch), cell, kFinishRefCell);
    f.swing = scale(at(a.swing, b.swing), cell, kFinishRefCell);
    f.lean = at(a.lean, b.lean);
    f.wide = at(a.wide, b.wide);
    f.tall = at(a.tall, b.tall);
    f.look = static_cast<int8_t>(scale(at(a.look * 1000, b.look * 1000), 1, 1000));
    f.left_col = 15;
    f.left_row = 14;
    f.right_col = 31;
    f.right_row = 14;
    for (const WalkLeg &leg : kWalkLegs) {
        if (t < leg.from || t >= leg.from + leg.steps * kWalkStepMs) {
            continue;
        }
        const int32_t k = (t - leg.from) / kWalkStepMs;
        const int32_t u = (t - leg.from) % kWalkStepMs;
        size_t s = 0;
        while (s + 2 < 5 && kWalkStep[s + 1][0] <= u) {
            s++;
        }
        const int32_t sp = kWalkStep[s + 1][0] - kWalkStep[s][0];
        const int32_t e = ease(kWalkStepEase[s + 1], std::clamp<int32_t>(u - kWalkStep[s][0], 0, sp) * 1000 / sp);
        const auto st = [&](int c) { return kWalkStep[s][c] + scale(kWalkStep[s + 1][c] - kWalkStep[s][c], e, 1000); };
        f.lift += scale(st(1), cell, kFinishRefCell);
        f.wide = f.wide * st(2) / 1000;
        f.tall = f.tall * st(3) / 1000;
        if (u < kWalkRaisedMs) { /* each step lifts the other pair, the hands swinging against it */
            f.raised = k % 2 == 0 ? Raised::kRight : Raised::kLeft;
            f.knee_dir = leg.dir;
            f.left_row = static_cast<int8_t>(k % 2 == 0 ? 13 : 15);
            f.right_row = static_cast<int8_t>(k % 2 == 0 ? 15 : 13);
        }
    }
    face(f, dir);
    return f;
}

/* A slot's number, mixed: lowbias32 (Chris Wellons's integer hash). constexpr, so a plan the app names by its seed can
 * be held to what it shows (kLookStretch). */
constexpr uint32_t mix(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

/* WHICH WAY A WALK OR A SKIP GOES — the owner's ask, 2026-11-07: left or right at even odds, from a hash of when the
 * move (or the idle's stretch) began — at random to the eye, the same through the move, and the same frames twice in
 * the simulator (HOST_SIMULATOR 3.1). */
int travel_dir(uint32_t seed) { return (mix(seed ^ 0x68E31DA5u) & 1u) != 0 ? 1 : -1; }

/*
 * THE LOOK-AROUND (AGENT_DECK 9.3): now and then in the overview's idle — no sheet covers it, drawn from
 * the character and the motion principles; the developer's choices of the draft at its real speed, 2026-10-29. A beat
 * at rest; the eyes dart round the glass like a clock hand, from a corner, one way or the other, all the way round and
 * back to that corner — each corner held as if something caught his eye, with a little squash as the look lands, one
 * of them the long, curious look with a blink in it; the clock's in-betweens only passed through, each a little
 * different, so it is never robotic. The body follows the eyes a beat behind, leaning and swaying toward the look;
 * looking down he bends forward, lower and squashed, the eyes two pixels down; looking up he stretches up a little.
 * Home: the eyes back to the middle, a shake-off wobble and a blink. Where it starts, which way round, which corner is
 * the curious one and the in-betweens' lengths come from a hash of the stretch's number. Per value: `sway`, `crouch` px
 * at kFinishRefCell, `lean` 0.1°, `wide`, `tall` per mille, at a look straight to the side, down or up.
 */
constexpr int32_t kLookFromMs = 400;     // the beat at rest
constexpr int32_t kLookDartMs = 120;     // the eyes darting from one look to the next
constexpr int32_t kLookStops = 9;        // a corner, round the clock, back to it
constexpr int16_t kLookCornerMs[4] = {450, 520, 600, 520}; // the corners' holds
constexpr int32_t kLookCuriousMs = 750;  // the curious corner's
constexpr int32_t kLookPassMs = 130;     // an in-between's, and up to kLookJitterMs more
constexpr int32_t kLookJitterMs = 50;
constexpr int32_t kLookLastMs = 300;     // back at the corner it started from, briefly
constexpr int32_t kLookFollowMs = 110;   // the body following the eyes
constexpr int32_t kLookReturnMs = 200;   // the eyes back to the middle
constexpr int32_t kLookBlinkMs = 110;
constexpr int32_t kLookCuriousBlinkMs = 380; // into the curious look
constexpr int32_t kLookHomeMs = 710;     // past its last look: back to the middle, the wobble, the blink
constexpr int32_t kLookSettleMs = static_cast<int32_t>(kMascotLookSettleMs); // a stopped look-around coming home
/* The clock's eight looks, the eyes' pixels from 12 o'clock round: the GIF grid's y is down. */
constexpr int8_t kLookClock[8][2] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
/* A held look landing, and home: (t, wide, tall ‰). */
struct SquashKey {
    int16_t t;
    int16_t wide;
    int16_t tall;
    Ease ease;
};
constexpr SquashKey kLookLand[] = {{0, 1000, 1000, Ease::kLinear},
                                   {50, 1025, 975, Ease::kOut},
                                   {100, 990, 1010, Ease::kInOut},
                                   {140, 1000, 1000, Ease::kInOut}};
constexpr int32_t kLookLandHeldMs = 400; // a look held this long lands with the squash
constexpr SquashKey kLookShake[] = {{0, 1000, 1000, Ease::kLinear},
                                    {90, 1040, 965, Ease::kOut},
                                    {180, 975, 1025, Ease::kInOut},
                                    {270, 1010, 990, Ease::kInOut},
                                    {350, 1000, 1000, Ease::kInOut}};
constexpr int32_t kLookLongestMs = kLookFromMs + kLookStops * kLookDartMs + kLookCornerMs[1] + kLookCornerMs[2] +
                                   kLookCornerMs[3] + kLookCuriousMs + 4 * (kLookPassMs + kLookJitterMs - 1) +
                                   kLookLastMs + kLookHomeMs;
static_assert(kLookLongestMs <= static_cast<int32_t>(theme::kMascotLookMs),
              "theme::kMascotLookMs holds the longest look-around");
static_assert(kLookReturnMs + kLookShake[std::size(kLookShake) - 1].t < kLookHomeMs - kLookBlinkMs + 1,
              "home's blink comes after the wobble");

struct LookPlan {
    int8_t start;   // the first look, on kLookClock
    int8_t way;     // 1 clockwise, -1 not
    int8_t curious; // the stop held longest
    std::array<int16_t, kLookStops> at; // ms each stop's dart begins
    int16_t end;    // ms the last look's hold ends
};

constexpr LookPlan look_plan(uint32_t seed)
{
    const uint32_t h = mix(seed ^ 0x68E31DA4u);
    LookPlan p = {};
    p.start = static_cast<int8_t>(1 + 2 * (h % 4)); /* a corner: 1:30, 4:30, 7:30 or 10:30 */
    p.way = (h >> 4) & 1 ? 1 : -1;
    p.curious = static_cast<int8_t>(2 * ((h >> 8) % 4));
    int32_t t = kLookFromMs;
    for (int32_t i = 0; i < kLookStops; i++) {
        p.at[i] = static_cast<int16_t>(t);
        const int32_t hold = i == kLookStops - 1 ? kLookLastMs
                             : i == p.curious     ? kLookCuriousMs
                             : i % 2 == 0         ? kLookCornerMs[i / 2]
                                                  : kLookPassMs + static_cast<int32_t>((h >> (12 + i)) % kLookJitterMs);
        t += kLookDartMs + hold;
    }
    p.end = static_cast<int16_t>(t);
    return p;
}

int32_t look_dir(const LookPlan &p, int32_t stop) { return ((p.start + p.way * stop) % 8 + 8) % 8; }

/* The look at `t`: the eyes' pixels, and the way the body leans, per mille across and down — round the clock smoothly
 * from one look to the next. */
struct Gaze {
    int8_t ex, ey;
    int32_t lx, ly;
};

Gaze look_gaze(const LookPlan &p, int32_t t)
{
    Gaze g = {};
    const auto toward = [&](int32_t degrees, int32_t amount) {
        g.lx = scale(lv_trigo_cos(static_cast<int16_t>(degrees)), amount, LV_TRIGO_SIN_MAX);
        g.ly = scale(lv_trigo_sin(static_cast<int16_t>(degrees)), amount, LV_TRIGO_SIN_MAX);
    };
    const auto degrees = [](int32_t dir) { return dir * 45 - 90 + 360; };
    if (t < kLookFromMs) {
        return g;
    }
    if (t >= p.end) { /* back to the middle from the last look */
        const int32_t last = look_dir(p, kLookStops - 1);
        const int32_t f = std::min<int32_t>((t - p.end) * 1000 / kLookReturnMs, 1000);
        if (f < 400) {
            g.ex = kLookClock[last][0];
            g.ey = kLookClock[last][1];
        }
        toward(degrees(last), 1000 - ease(Ease::kInOut, f));
        return g;
    }
    int32_t k = 0;
    while (k + 1 < kLookStops && p.at[k + 1] <= t) {
        k++;
    }
    const int32_t cur = look_dir(p, k);
    const int32_t u = (t - p.at[k]) * 1000 / kLookDartMs;
    const int32_t e = ease(Ease::kInOut, std::min<int32_t>(u, 1000));
    const int32_t eyes = u > 450 ? cur : k == 0 ? -1 : look_dir(p, k - 1);
    if (eyes >= 0) {
        g.ex = kLookClock[eyes][0];
        g.ey = kLookClock[eyes][1];
    }
    if (k == 0) { /* out from the middle to the first look */
        toward(degrees(cur), e);
    } else { /* round the clock from the look before, an eighth of a turn */
        toward(degrees(look_dir(p, k - 1)) + scale(45 * p.way, e, 1000), 1000);
    }
    return g;
}

template <size_t N> void squash_track(const SquashKey (&keys)[N], int32_t t, int32_t &wide, int32_t &tall)
{
    size_t i = 0;
    while (i + 2 < N && keys[i + 1].t <= t) {
        i++;
    }
    const SquashKey &a = keys[i];
    const SquashKey &b = keys[i + 1];
    const int32_t span = b.t - a.t;
    const int32_t x = ease(b.ease, std::clamp<int32_t>(t - a.t, 0, span) * 1000 / span);
    wide = wide * (a.wide + scale(b.wide - a.wide, x, 1000)) / 1000;
    tall = tall * (a.tall + scale(b.tall - a.tall, x, 1000)) / 1000;
}

PixelPose look_pose_at(const LookPlan &p, int32_t t, int32_t cell)
{
    PixelPose f = {};
    f.flag = -1;
    f.bursts = {-1, -1};
    const Gaze eyes = look_gaze(p, t);
    const Gaze body = look_gaze(p, std::max<int32_t>(t - kLookFollowMs, 0)); /* the body a beat behind */
    f.look = eyes.ex;
    f.look_y = static_cast<int8_t>(eyes.ey > 0 ? 2 : eyes.ey); /* bending forward, the eyes lower still */
    f.lean = scale(30, body.lx, 1000);
    f.sway = scale(2 * body.lx, cell, 1000 * kFinishRefCell);
    if (body.ly > 0) { /* looking down: bending forward, lower and squashed — the developer's ask, 2026-10-29 */
        f.crouch = scale(4 * body.ly, cell, 1000 * kFinishRefCell);
        f.wide = 1000 + scale(40, body.ly, 1000);
        f.tall = 1000 - scale(70, body.ly, 1000);
    } else { /* looking up: stretching up a little */
        f.crouch = scale(2 * body.ly, cell, 1000 * kFinishRefCell);
        f.wide = 1000 + scale(25, body.ly, 1000);
        f.tall = 1000 - scale(25, body.ly, 1000);
    }
    if (t >= kLookFromMs && t < p.end) { /* a held look landing: a little squash */
        int32_t k = 0;
        while (k + 1 < kLookStops && p.at[k + 1] <= t) {
            k++;
        }
        const int32_t held = (k + 1 < kLookStops ? p.at[k + 1] : p.end) - p.at[k] - kLookDartMs;
        const int32_t u = t - p.at[k] - kLookDartMs;
        if (held >= kLookLandHeldMs && u >= 0 && u < kLookLand[std::size(kLookLand) - 1].t) {
            squash_track(kLookLand, u, f.wide, f.tall);
        }
    }
    const int32_t shake = t - p.end - kLookReturnMs; /* home: the shake-off */
    if (shake >= 0 && shake < kLookShake[std::size(kLookShake) - 1].t) {
        squash_track(kLookShake, shake, f.wide, f.tall);
    }
    const int32_t curious = p.at[p.curious] + kLookDartMs + kLookCuriousBlinkMs;
    const int32_t home = p.end + kLookHomeMs - kLookBlinkMs;
    f.shut = (t >= curious && t < curious + kLookBlinkMs) || (t >= home && t < home + kLookBlinkMs);
    f.left_col = 15;
    f.left_row = 14;
    f.right_col = 31;
    f.right_row = 14;
    return f;
}

/* Stopped part way: the eyes back to the middle and the body home, eased out over kLookSettleMs — the pose `d` ms after
 * the stop, and how long it takes to be home. */
PixelPose look_outro_at(const LookPlan &p, int32_t s, uint32_t d, int32_t cell, uint32_t *finishes)
{
    const bool done = s >= p.end + kLookHomeMs;
    if (finishes != nullptr) {
        *finishes = done ? 0 : kLookSettleMs;
    }
    const PixelPose from = look_pose_at(p, s, cell);
    if (done) {
        return from;
    }
    const auto dd = static_cast<int32_t>(std::min<uint32_t>(d, kLookSettleMs));
    PixelPose f = from;
    const int32_t x = ease(Ease::kOut, dd * 1000 / kLookSettleMs);
    const auto to = [&](int32_t a, int32_t b) { return a + scale(b - a, x, 1000); };
    f.lean = to(from.lean, 0);
    f.sway = to(from.sway, 0);
    f.crouch = to(from.crouch, 0);
    f.wide = to(from.wide, 1000);
    f.tall = to(from.tall, 1000);
    f.shut = false;
    if (dd >= kLookSettleMs * 2 / 5) {
        f.look = 0;
        f.look_y = 0;
    }
    return f;
}

/* THE OVERVIEW'S IDLING, AT RANDOM — the developer's asks, 2026-10-29 and 2026-10-30: its time in stretches of three
 * idle loops — two until the walk took the owner's route off the glass and back, 2026-11-07, and outgrew them — each
 * stretch the loops, the walk and a rest, or the look-around and a rest, at even odds, picked by a hash
 * of the stretch's number so the simulator draws the same frames twice (HOST_SIMULATOR 3.1); half the stretches that
 * are the two loops, by a hash of when the idle began too, so each run of it differs, are the skip and a rest instead.
 * The first is always the idle's lean. */
constexpr uint32_t kIdleStretchMs = 3 * theme::kMascotIdleMs;
static_assert(theme::kMascotWalkMs <= kIdleStretchMs, "the walk fits a stretch of the idle");
static_assert(theme::kMascotLookMs <= kIdleStretchMs, "the look-around fits a stretch of the idle");
static_assert(theme::kMascotSkipMs <= kIdleStretchMs, "the skip fits a stretch of the idle");

enum class Stretch : uint8_t { kLean, kWalk, kLook, kSkip };

PixelPose reaction_pose_at(MascotMove move, int32_t t, int32_t cell, int dir = -1);
PixelPose skip_pose_at(int32_t t, int32_t cell, int dir, int32_t trip);
constexpr uint32_t skip_ms(int32_t trip);
int32_t idle_skip_trip(uint32_t since, uint32_t stretch, int dir, int32_t stopped);

/* THE IDLE IN PLACE (kIdleInPlace) — the owner's choice of the draft standby_idle_remap, at random, 2026-11-17: the
 * standby page's idle kept in its column, the walk and the skip left out — the first stretch always the lean; each
 * after it the lean or the look-around at even odds, from a hash of when the idle began too, so each visit differs. */
constexpr Stretch stretch_kind(uint32_t stretch, uint32_t since, bool in_place = false)
{
    if (stretch == 0) {
        return Stretch::kLean;
    }
    if (in_place) {
        return (mix((since + stretch * 0x9E3779B9u) ^ 0x4F1BBCDCu) & 1u) != 0 ? Stretch::kLook : Stretch::kLean;
    }
    switch (mix(stretch ^ 0x5BD1E995u) % 3) {
    case 0: return Stretch::kWalk;
    case 1: return Stretch::kLook;
    default: return (mix((since + stretch * 0x9E3779B9u) ^ 0x2F6B1C3Du) & 1u) != 0 ? Stretch::kSkip : Stretch::kLean;
    }
}

/* THE LOOK-AROUND ON ITS OWN (mascot_play_look()): a stretch of the idle that is a look-around, begun at its start,
 * its plan fixed by the stretch's number — from the 1:30 corner counter-clockwise, over the top: 12, then 10:30 —
 * so a page that wants it gets the same glance every time and a scenario can see it. A change to either hash fails
 * here, not on the glass. */
constexpr uint32_t kLookStretch = 4;
static_assert(stretch_kind(kLookStretch, 0) == Stretch::kLook && stretch_kind(kLookStretch, 1) == Stretch::kLook,
              "the look-around's stretch is one the hash makes a look-around, whenever the idle began");
static_assert(look_plan(kLookStretch).start == 1 && look_plan(kLookStretch).way == -1,
              "the look-around's plan goes from 1:30 counter-clockwise: 12, then 10:30");

/* The idle in place over its first `stretches` from `since`: never the walk or the skip, the first the lean, and both
 * the lean and the look-around among the rest — a hash stuck on one kind, or one that let the route in, fails it. */
constexpr bool stays_in_place(uint32_t since, uint32_t stretches)
{
    bool looks = false;
    bool leans = false;
    for (uint32_t s = 0; s < stretches; s++) {
        const Stretch k = stretch_kind(s, since, true);
        if (k == Stretch::kWalk || k == Stretch::kSkip || (s == 0 && k != Stretch::kLean)) {
            return false;
        }
        looks = looks || k == Stretch::kLook;
        leans = leans || (s > 0 && k == Stretch::kLean);
    }
    return looks && leans;
}
/* Two visits a moment apart take different stretches: the hash reads when the idle began. */
constexpr bool visits_differ(uint32_t a, uint32_t b, uint32_t stretches)
{
    for (uint32_t s = 1; s < stretches; s++) {
        if (stretch_kind(s, a, true) != stretch_kind(s, b, true)) {
            return true;
        }
    }
    return false;
}
static_assert(stays_in_place(0, 16) && stays_in_place(1, 16) && stays_in_place(0x80000000u, 16),
              "the idle in place keeps to the lean and the look-around, the first stretch the lean");
static_assert(visits_differ(0, 777, 8), "the idle in place is the same at every visit: its hash ignores `since`");

bool turn_waves(uint32_t turn) { return turn != 0 && mix(turn ^ 0x1B873593u) % 3 == 0; }


/* The idle at `t` ms into stretch number `stretch`: an idle loop, or the walk, the look-around or the skip — now and
 * then tripping — and then the rest pose; in place, an idle loop or the look-around; `stopped` ms into it the idle was
 * asked to stop, or INT32_MAX. */
PixelPose stretch_pose(uint32_t stretch, uint32_t since, bool in_place, int32_t t, int32_t cell,
                       int32_t stopped = INT32_MAX)
{
    const int dir = travel_dir(since + stretch * kIdleStretchMs);
    switch (stretch_kind(stretch, since, in_place)) {
    case Stretch::kWalk:
        return walk_pose_at(std::min<int32_t>(t, static_cast<int32_t>(theme::kMascotWalkMs)), cell, dir);
    case Stretch::kLook: return look_pose_at(look_plan(stretch), t, cell);
    case Stretch::kSkip: {
        const int32_t trip = idle_skip_trip(since, stretch, dir, stopped);
        return skip_pose_at(std::min<int32_t>(t, static_cast<int32_t>(skip_ms(trip))), cell, dir, trip);
    }
    case Stretch::kLean: break;
    }
    return idle_pose_at(t % static_cast<int32_t>(theme::kMascotIdleMs), cell);
}

PixelPose idle_cycle_pose(const MascotPlay &play, uint32_t now, int32_t cell)
{
    const bool in_place = play.move == MascotMove::kIdleInPlace;
    if (play.stopped == kStillPlaying) {
        const uint32_t e = now - play.since;
        return stretch_pose(e / kIdleStretchMs, play.since, in_place, static_cast<int32_t>(e % kIdleStretchMs), cell);
    }
    const uint32_t e = play.stopped - play.since;
    const uint32_t stretch = e / kIdleStretchMs;
    const auto s = static_cast<int32_t>(e % kIdleStretchMs);
    switch (stretch_kind(stretch, play.since, in_place)) {
    case Stretch::kLook: return look_outro_at(look_plan(stretch), s, now - play.stopped, cell, nullptr);
    case Stretch::kWalk: /* each plays on to its end: part way along the route, home is nearest on ahead */
    case Stretch::kSkip: /* a skip stopped before it trips skips home plainly; tripped, it plays on (skip_trip()) */
        return stretch_pose(stretch, play.since, in_place, s + static_cast<int32_t>(now - play.stopped), cell, s);
    case Stretch::kLean: break;
    }
    return idle_pose_at(idle_outro_at(s % static_cast<int32_t>(theme::kMascotIdleMs), now - play.stopped, nullptr),
                        cell);
}

/* DONE, HE IDLES — the developer's ask, 2026-10-29: the finish and the celebrate, played out to their end without being
 * stopped, carry on into the idle (and its walks), still themselves, so a record asking for them again replays nothing.
 * The idle's play, begun as the move of `length` ended; and whether `at` is past that end. */
MascotPlay idle_after(const MascotPlay &play, uint32_t length)
{
    MascotPlay idle = play;
    idle.move = MascotMove::kIdle;
    idle.since = play.since + length;
    return idle;
}

bool past_end(const MascotPlay &play, uint32_t length, uint32_t at)
{
    return static_cast<int32_t>(at - (play.since + length)) >= 0;
}

/* When a stopped idle is home. */
uint32_t idle_cycle_finishes(const MascotPlay &play)
{
    const uint32_t e = play.stopped - play.since;
    const auto s = static_cast<int32_t>(e % kIdleStretchMs);
    uint32_t home = 0;
    switch (stretch_kind(e / kIdleStretchMs, play.since, play.move == MascotMove::kIdleInPlace)) {
    case Stretch::kWalk:
        home = static_cast<uint32_t>(std::max<int32_t>(static_cast<int32_t>(theme::kMascotWalkMs) - s, 0));
        break;
    case Stretch::kLook: look_outro_at(look_plan(e / kIdleStretchMs), s, 0, kFinishRefCell, &home); break;
    case Stretch::kSkip: {
        const uint32_t stretch = e / kIdleStretchMs;
        const int32_t trip = idle_skip_trip(play.since, stretch, travel_dir(play.since + stretch * kIdleStretchMs), s);
        home = static_cast<uint32_t>(std::max<int32_t>(static_cast<int32_t>(skip_ms(trip)) - s, 0));
        break;
    }
    case Stretch::kLean: idle_outro_at(s % static_cast<int32_t>(theme::kMascotIdleMs), 0, &home); break;
    }
    return play.stopped + home;
}

/*
 * THE REACTIONS (AGENT_DECK 9.3): Claude Code's terminal mascot's skip, spin and dust-puff jump, redrawn
 * on the deck's own mascot with the squishy physics — no sheet covers them; the developer's choices of the drafts at
 * their real speed, 2026-10-29. Each plays once as its event comes, to its end, then the page's own move. Per key:
 * `x` the skip's travel and `lift`, `crouch` px at kFinishRefCell, `lean` 0.1°, `wide`, `tall` per mille.
 */
struct ReactKey {
    int16_t t;
    int16_t x;
    int8_t lift;
    int8_t crouch;
    int16_t lean;
    int16_t wide;
    int16_t tall;
    Ease ease;
};

/* `route` the skip's whole route (on_route()), 0 for a move in place; `dir` the way along it. */
void react_track(PixelPose &f, const ReactKey *keys, size_t N, int32_t t, int32_t cell, int32_t route, int dir = -1)
{
    size_t i = 0;
    while (i + 2 < N && keys[i + 1].t <= t) {
        i++;
    }
    const ReactKey &a = keys[i];
    const ReactKey &b = keys[i + 1];
    const int32_t span = b.t - a.t;
    const int32_t x = ease(b.ease, std::clamp<int32_t>(t - a.t, 0, span) * 1000 / span);
    const auto at = [&](int32_t from, int32_t to) { return from + scale(to - from, x, 1000); };
    f.travel = scale(route > 0 ? on_route(at(a.x, b.x), route, dir) : at(a.x, b.x), cell, kFinishRefCell);
    f.lift = scale(at(a.lift, b.lift), cell, kFinishRefCell);
    f.crouch = scale(at(a.crouch, b.crouch), cell, kFinishRefCell);
    f.lean = at(a.lean, b.lean);
    f.wide = at(a.wide, b.wide);
    f.tall = at(a.tall, b.tall);
}

template <size_t N> void react_track(PixelPose &f, const ReactKey (&keys)[N], int32_t t, int32_t cell)
{
    react_track(f, keys, N, t, cell, 0);
}

PixelPose react_rest()
{
    PixelPose f = {};
    f.flag = -1;
    f.bursts = {-1, -1};
    f.left_col = 15;
    f.left_row = 14;
    f.right_col = 31;
    f.right_row = 14;
    return f;
}

/* Both hands flung up over his top corners. */
void arms_up(PixelPose &f)
{
    f.left_row = 7;
    f.right_row = 7;
}

/*
 * THE SKIP — a new agent arrived, or now and then in the idle: from home, a squashed wind-up looking left; then skips
 * to the left, each pushed off one pair stretched and leaning in, that pair's knees up toward the skip, the hands
 * swinging against the legs, landing on the other pair squashed, kSkipHopMs and kSkipStride px apart as in the draft —
 * off the glass and back in from past its right side on the owner's route, 2026-11-07, kSkipCount of them — the last a
 * little bigger, a squashed landing home, a slide past home and the settle; then turned to you, ^ ^, hello. Every
 * landing rings on in the body, tall, squat, tall, dying away (kSkipJiggle) — the developer's ask.
 * theme::kMascotSkipMs.
 */
constexpr int32_t kSkipWindUp = 320; // the wind-up ends: the first push-off
constexpr int32_t kSkipHopMs = 380;  // one skip's wind-up end to the next's
constexpr int32_t kSkipStride = 16;  // px at kFinishRefCell a skip
constexpr int32_t kSkipCount = route_strides(kSkipStride);
constexpr int32_t kSkipSpan = kSkipCount * kSkipStride;
constexpr int32_t kSkipLandAt = 350; // into a skip: landed
constexpr int32_t kSkipHomeMs = kSkipWindUp + (kSkipCount - 1) * kSkipHopMs + kSkipLandAt;
static_assert(kSkipSpan - kRouteOff >= kRouteBack, "the skip comes back from wholly past the far side");

/*
 * THE SKIP TRIP (AGENT_DECK 9.3) — the owner's ask and their approved draft 4, 2026-11-10: now and then an idle skip
 * trips at the landing of skip `trip` (skip_trip()). The trailing foot catches; flung forward over it, stretched, the
 * arms out at the body's middle; two staggers, each a bob and a squashed contact, pitching further; caught low and
 * squashed (kTripKeys) — then he turns to the three-quarter view facing the way he skips and THE FACEPLANT plays, its
 * frames as baked (mascot_frames.h), from its rock back (kTripFallFrom, held kTripHoldMs) to its head shake's end, on
 * the open ground: the laptop left out, his face on the ground row, mirrored going right, 5 columns left of the work's
 * so his body stands over his feet (kTripCanvas). Then the turn back to the front looking the way he goes, the skip's
 * own wind-up, and skips trip + 2 on along the route as built — the stumble covered stride trip + 1 — home, the settle
 * and hello. theme::kMascotSkipTripMs. Per key as the skip's.
 */
constexpr int32_t kTripStumbleMs = 330; // the catch to the turn into the three-quarter view
constexpr int32_t kTripFallFrom = 867;  // into the faceplant: its rock back, the eyes open (its baked step 26)
constexpr int32_t kTripHoldMs = 67;     // that frame held, so the rock back reads
constexpr int32_t kTripUpMs = 3850;     // into the faceplant: the head shake's end (kFaceplantUpMs, asserted there)
constexpr int32_t kTripDownMs = kTripHoldMs + kTripUpMs - kTripFallFrom;
constexpr int32_t kTripBackU = kTripStumbleMs + kTripDownMs; // since the catch: the turn back to the front
constexpr ReactKey kTripKeys[] = { // x from the catch's landing
    {0, 0, 0, 3, 0, 1100, 900, Ease::kLinear},       // skip k's landing: the trailing foot caught
    {50, -4, 0, 1, -110, 950, 1070, Ease::kOut},     // flung forward over the caught foot, stretched, arms out
    {120, -8, 3, 0, -140, 980, 1040, Ease::kOut},    // stagger 1: a bob forward, pitching on
    {150, -10, 0, 2, -130, 1060, 940, Ease::kIn},    //   its contact, squashed
    {230, -13, 3, 0, -150, 980, 1040, Ease::kOut},   // stagger 2, further over
    {260, -15, 0, 2, -120, 1070, 930, Ease::kIn},    //   its contact
    {kTripStumbleMs, -kSkipStride, 0, 6, -60, 1080, 900, Ease::kInOut}, // caught low, squashed, arms in: the turn
};
static_assert(kTripKeys[std::size(kTripKeys) - 1].x == -kSkipStride, "the stumble covers a stride");

/* Since the skip's start, `trip` the skip that trips or -1: when skip `i` pushes off — those after the trip from the
 * resume, its own opening again — when he catches his foot, turns, turns back, and lands home. */
constexpr int32_t trip_catch(int32_t trip) { return kSkipWindUp + trip * kSkipHopMs + kSkipLandAt; }
constexpr int32_t trip_resume(int32_t trip) { return trip_catch(trip) + kTripBackU; }
constexpr int32_t skip_from(int32_t i, int32_t trip)
{
    return trip < 0 || i <= trip ? kSkipWindUp + i * kSkipHopMs
                                 : trip_resume(trip) + kSkipWindUp + (i - trip - 2) * kSkipHopMs;
}
constexpr int32_t skip_home(int32_t trip) { return skip_from(kSkipCount - 1, trip) + kSkipLandAt; }
constexpr uint32_t skip_ms(int32_t trip) { return static_cast<uint32_t>(skip_home(trip) + 1000); }

/* The keys before the skips, each skip's four from its wind-up's end — tripping, the skips to the trip, the resume's
 * opening and the skips after the stride the stumble covered — and the settle home after the last. */
constexpr size_t kSkipKeys = 3 + 4 * kSkipCount + 6;
struct SkipKeys {
    std::array<ReactKey, kSkipKeys> k;
    size_t n;
};
constexpr SkipKeys skip_keys(int32_t trip = -1)
{
    SkipKeys s = {};
    auto &k = s.k;
    size_t &n = s.n;
    k[n++] = {0, 0, 0, 0, 0, 1000, 1000, Ease::kLinear};
    k[n++] = {200, 0, 0, 0, 0, 1000, 1000, Ease::kLinear};
    k[n++] = {kSkipWindUp, 0, 0, 5, 10, 1090, 900, Ease::kOut}; // the wind-up: squashed, leaning away a touch
    for (int32_t i = 0; i < kSkipCount; i++) {
        if (trip >= 0 && i == trip + 1) {
            continue; /* the stride the stumble covered */
        }
        const auto x = static_cast<int16_t>(-i * kSkipStride);
        if (trip >= 0 && i == trip + 2) { /* up again: the skip's own opening, from where he got up */
            const auto at = static_cast<int16_t>(trip_resume(trip));
            k[n++] = {at, x, 0, 0, 0, 1000, 1000, Ease::kLinear};
            k[n++] = {static_cast<int16_t>(at + 200), x, 0, 0, 0, 1000, 1000, Ease::kLinear};
            k[n++] = {static_cast<int16_t>(at + kSkipWindUp), x, 0, 5, 10, 1090, 900, Ease::kOut};
        }
        const auto from = static_cast<int16_t>(skip_from(i, trip));
        const bool last = i == kSkipCount - 1;
        k[n++] = {static_cast<int16_t>(from + 60), static_cast<int16_t>(x - 2), 5, 0, -25, 930, 1100, Ease::kOut};
        k[n++] = {static_cast<int16_t>(from + 200), static_cast<int16_t>(x - 8), static_cast<int8_t>(last ? 14 : 12), 0,
                  -20, 980, 1030, Ease::kOut}; // the top of the skip; the last a little bigger
        k[n++] = {static_cast<int16_t>(from + 320), static_cast<int16_t>(x - 15), 2, 0, -10, 970, 1050, Ease::kIn};
        k[n++] = last ? ReactKey{static_cast<int16_t>(from + kSkipLandAt), static_cast<int16_t>(x - 16), 0, 4, 0, 1150,
                                 840, Ease::kOut} // home: the landing, squashed hard
                      : ReactKey{static_cast<int16_t>(from + kSkipLandAt), static_cast<int16_t>(x - 16), 0, 3, 0, 1100,
                                 900, Ease::kOut}; // landing on the other pair, squashed
    }
    constexpr auto home = static_cast<int16_t>(-kSkipSpan);
    const auto h = static_cast<int16_t>(skip_home(trip));
    const auto at = [&](int32_t ms) { return static_cast<int16_t>(h + ms); };
    k[n++] = {at(50), static_cast<int16_t>(home - 2), 0, 0, 6, 1070, 930, Ease::kOut}; // sliding past, rocking
    k[n++] = {at(100), home, 0, 0, 0, 950, 1070, Ease::kInOut};                        // the settle: tall,
    k[n++] = {at(150), home, 0, 0, 0, 1040, 965, Ease::kInOut};                        // squat,
    k[n++] = {at(200), home, 0, 0, 0, 985, 1015, Ease::kInOut};                        // tall,
    k[n++] = {at(250), home, 0, 0, 0, 1000, 1000, Ease::kInOut};                       // settled
    k[n++] = {static_cast<int16_t>(skip_ms(trip)), home, 0, 0, 0, 1000, 1000, Ease::kLinear};
    return s;
}
constexpr SkipKeys kSkipTrack = skip_keys();
static_assert(kSkipTrack.n == kSkipKeys && kSkipTrack.k[kSkipKeys - 1].t == theme::kMascotSkipMs &&
                  kSkipHomeMs + 1000 == theme::kMascotSkipMs && skip_ms(-1) == theme::kMascotSkipMs,
              "the skip's keys run its length: home, and the draft's second of settle and hello");
static_assert(skip_ms(12) == theme::kMascotSkipTripMs &&
                  skip_keys(12).k[skip_keys(12).n - 1].t == theme::kMascotSkipTripMs &&
                  skip_ms(12) - skip_ms(-1) == 3290,
              "the tripped skip runs its approved draft 4's length, the skip's and 3290 ms");
constexpr int32_t kSkipAirFrom = 40;  // into a skip: off the ground, a pair's knees up
constexpr int32_t kSkipAirTo = 330;
constexpr int32_t kSkipLookFrom = 140; // looking left, where he goes, from the wind-up to home
/* A landing ringing on in the body: (ms after it, squat per mille — wider and shorter, or taller and narrower). */
constexpr int16_t kSkipJiggle[7][2] = {{0, 0}, {50, -55}, {100, 42}, {150, -30}, {200, 20}, {250, -10}, {300, 0}};
constexpr Ease kSkipJiggleEase[7] = {Ease::kLinear, Ease::kOut, Ease::kInOut, Ease::kInOut,
                                     Ease::kInOut,  Ease::kInOut, Ease::kInOut};

/*
 * WHERE A TRIP MAY HAPPEN — the owner's rules, 2026-11-10: wholly on the glass while he stumbles, lies and gets up, a
 * cell clear of each side. From the catch, the trip reaches kTripAhead px the way he goes and kTripBehind back (at
 * kMascotCell, measured on the simulator's frames, every 10 ms, the larger of the two ways: 144 ahead going right in
 * the second stagger, 91 behind going left in the head shake's sway); a skip whose catch puts that inside the margin
 * may trip, so going left it trips coming home across the data column (skips 12–18), going right on its way out (0–5) —
 * never with the route's jump inside the stumble, and always with a skip after it to carry on.
 */
constexpr int32_t kTripAhead = 144;
constexpr int32_t kTripBehind = 91;
constexpr bool trip_fits_band(int32_t trip, int dir)
{
    if (trip < 0 || trip > kSkipCount - 3) {
        return false;
    }
    const int32_t raw = -(trip + 1) * kSkipStride;
    const int32_t a = on_route(raw, kSkipSpan, dir);
    if (on_route(raw - kSkipStride, kSkipSpan, dir) - a != dir * kSkipStride) {
        return false;
    }
    const int32_t x = theme::kMascotCentreX + a * theme::kMascotCell / kFinishRefCell;
    const int32_t left = x - (dir < 0 ? kTripAhead : kTripBehind);
    const int32_t right = x + (dir < 0 ? kTripBehind : kTripAhead);
    return left >= theme::kMascotCell && right <= theme::kScreenWidth - 1 - theme::kMascotCell;
}

/*
 * WHERE A TRIP MAY HAPPEN ON THE ROUND GLASS (theme::kMascotTripsHomeward) — the owner, 2026-11-18: "The mascot can
 * continue to skip, but he should trip only when he's on screen so when he's able to return back to his position he
 * would trip there." He skips off and back in as ever, and trips only on the way home — past the route's jump — at a
 * skip whose whole trip, from the catch through the stumble, the faceplant and the head shake to the wind-up after it,
 * lies inside the glass's circle a cell clear; then skips on home. The trip's reach from the catch's centre in px at
 * kFinishRefCell, the larger of the two ways, measured on the simulator's frames at cell 8 every 10 ms `[H]`
 * 2026-11-18, in three bands of rows from the mascot's top (kTripReach); each band's box inside the circle kMascotCell
 * in. At cell 8 on the 240 px glass: skips 16-18 either way, the catch 64, 48 or 32 px from home.
 */
struct TripReach {
    int16_t top, bottom, ahead, behind; // rows from the mascot's top; px the way he goes and back
};
constexpr TripReach kTripReach[] = {
    {-20, -1, 56, 30}, // above the mascot: the spring up off the ground, the head shake's hop
    {0, 23, 66, 35},   // the body's upper rows
    {24, 63, 72, 44},  // the lower rows to the foot: the stagger ahead, the caught foot's hand behind
};
/* Glass pixel (x, y)'s centre within the circle `in` px inside the glass's edge — in half pixels, as chord_width(). */
constexpr bool within_circle(int32_t x, int32_t y, int32_t in)
{
    const int32_t dx = 2 * x + 1 - theme::kScreenWidth;
    const int32_t dy = 2 * y + 1 - theme::kScreenHeight;
    const int32_t r = theme::kScreenWidth - 2 * in;
    return dx * dx + dy * dy <= r * r;
}
constexpr bool trip_fits_homeward(int32_t trip, int dir)
{
    if (trip < 0 || trip > kSkipCount - 3) {
        return false;
    }
    const int32_t raw = -(trip + 1) * kSkipStride;
    const int32_t a = on_route(raw, kSkipSpan, dir);
    if (a == (dir < 0 ? raw : -raw)) {
        return false; /* still on the way out */
    }
    if (on_route(raw - kSkipStride, kSkipSpan, dir) - a != dir * kSkipStride) {
        return false;
    }
    const int32_t x = theme::kMascotCentreX + a * theme::kMascotCell / kFinishRefCell;
    const int32_t c = theme::kMascotCell;
    for (const TripReach &b : kTripReach) {
        const int32_t ahead = b.ahead * c / kFinishRefCell;
        const int32_t behind = b.behind * c / kFinishRefCell;
        const int32_t left = x - (dir < 0 ? ahead : behind);
        const int32_t right = x + (dir < 0 ? behind : ahead);
        const int32_t top = theme::kMascotTripTop + b.top * c / kFinishRefCell;
        const int32_t bottom = theme::kMascotTripTop + b.bottom * c / kFinishRefCell;
        if (!within_circle(left, top, c) || !within_circle(right, top, c) || !within_circle(left, bottom, c) ||
            !within_circle(right, bottom, c)) {
            return false;
        }
    }
    return true;
}
constexpr bool trip_fits(int32_t trip, int dir)
{
    return theme::kMascotTripsHomeward ? trip_fits_homeward(trip, dir) : trip_fits_band(trip, dir);
}
/* The band of skips that may trip going `dir`: the first, and how many. */
struct TripBand {
    int32_t first, count;
};
constexpr TripBand trip_band(int dir)
{
    TripBand b = {-1, 0};
    for (int32_t i = 0; i < kSkipCount; i++) {
        if (trip_fits(i, dir)) {
            b.first = b.first < 0 ? i : b.first;
            b.count++;
        }
    }
    return b;
}
/* THE GLASS THE TRIP WAS DRAWN ON — the family's theme::kMascotTrips: a glass whose trip the owner approved. The
 * landscape glass trips in draft 4's band (trip_fits_band(): kTripAhead and kTripBehind, px measured at kMascotCell
 * 16); the round glass on the way home only (trip_fits_homeward(), theme::kMascotTripsHomeward: its approved draft,
 * ONX2424G013_DESIGN_TOKENS 5.6). Each band and the stage's trips asserted at its own glass. */
constexpr bool kTripDrawnHere = theme::kMascotTrips;
/* ON THE STICKY'S PORTRAIT GLASS (MASCOT §6): the band is derived from draft 4's reach, its numbers unchanged, on this
 * 480 px glass with the mascot centred — the deck's landscape band (skips 12-18 going left, 0-5 going right) was its left
 * column's. Each way keeps at least one skip that may trip. */
static_assert(!kTripDrawnHere || theme::kMascotTripsHomeward || (trip_band(-1).count > 0 && trip_band(1).count > 0),
              "the trip has a band each way on this glass");
static_assert(!theme::kMascotTripsHomeward || theme::kScreenWidth != 240 || theme::kMascotCell != 8 ||
                  (trip_band(-1).first == 16 && trip_band(-1).count == 3 && trip_band(1).first == 16 &&
                   trip_band(1).count == 3),
              "the round trip's band is its draft's: skips 16-18 either way, on the way home");
static_assert(!kTripDrawnHere || skip_ms(trip_band(-1).first) == theme::kMascotSkipTripMs,
              "every trip runs the same length");

/* The front view's stumble, `u` ms since the catch, onto `f` (the skip's pose): its keys from the catch's landing, the
 * caught pair raised behind, then each stagger's pair stepping on; the arms out at the body's middle, flailing a pixel
 * in turn, and in again. Going right, mirrored whole — face() turns the lean, the look and the knees. */
void trip_stumble(PixelPose &f, int32_t u, int32_t trip, int32_t cell, int dir)
{
    ReactKey keys[std::size(kTripKeys)] = {};
    const auto from = static_cast<int16_t>(-(trip + 1) * kSkipStride);
    for (size_t i = 0; i < std::size(kTripKeys); i++) {
        keys[i] = kTripKeys[i];
        keys[i].x = static_cast<int16_t>(keys[i].x + from);
    }
    react_track(f, keys, std::size(keys), u, cell, kSkipSpan, dir);
    f.raised = Raised::kNone;
    f.knee_dir = 0;
    if (u >= 10 && u < 105) { /* the trailing pair caught behind, the knee bent back */
        f.raised = Raised::kRight;
        f.knee_dir = 1;
    } else if (u >= 105 && u < 145) { /* stagger 1: the other pair stepping on */
        f.raised = Raised::kLeft;
        f.knee_dir = -1;
    } else if (u >= 215 && u < 255) { /* stagger 2 */
        f.raised = Raised::kRight;
        f.knee_dir = -1;
    }
    int8_t lc = 15, lr = 14, rc = 31, rr = 14; /* the arms: out, flailing, in */
    if (u >= 40 && u < 70) {
        lc = 14;
        rc = 32;
    } else if (u >= 70 && u < 265) {
        const auto a = static_cast<int8_t>((u / 60) % 2);
        lc = 13;
        lr = static_cast<int8_t>(13 + a);
        rc = 33;
        rr = static_cast<int8_t>(14 - a);
    } else if (u >= 265 && u < 300) {
        lc = 14;
        rc = 32;
    }
    if (dir > 0) { /* mirrored whole: the hands' sides and the pairs swapped */
        std::swap(lr, rr);
        f.raised = f.raised == Raised::kLeft ? Raised::kRight : f.raised == Raised::kRight ? Raised::kLeft : f.raised;
    }
    f.left_col = lc;
    f.left_row = lr;
    f.right_col = rc;
    f.right_row = rr;
    f.look = -1;
    f.stumble = true;
}

/* The skip at `t` ms into it going `dir`, tripping at skip `trip` (-1 for none). */
PixelPose skip_pose_at(int32_t t, int32_t cell, int dir, int32_t trip)
{
    PixelPose f = react_rest();
    SkipKeys tripped = {};
    const SkipKeys &keys = trip < 0 ? kSkipTrack : (tripped = skip_keys(trip));
    react_track(f, keys.k.data(), keys.n, t, cell, kSkipSpan, dir);
    for (int32_t k = 0; k < kSkipCount; k++) {
        if (trip >= 0 && k == trip + 1) {
            continue;
        }
        const int32_t u = t - skip_from(k, trip);
        if (u >= kSkipAirFrom && u < kSkipAirTo) { /* in the air: one pair's knees up toward the skip */
            f.raised = k % 2 == 0 ? Raised::kLeft : Raised::kRight;
            f.knee_dir = -1;
            f.left_row = static_cast<int8_t>(k % 2 == 0 ? 15 : 12); /* the hands swinging against the legs */
            f.right_row = static_cast<int8_t>(k % 2 == 0 ? 12 : 15);
        }
        const int32_t j = u - kSkipLandAt; /* the landing ringing on — but the one he trips on */
        if (j >= 0 && j < kSkipJiggle[6][0] && k != trip) {
            size_t s = 0;
            while (s + 2 < 7 && kSkipJiggle[s + 1][0] <= j) {
                s++;
            }
            const int32_t span = kSkipJiggle[s + 1][0] - kSkipJiggle[s][0];
            const int32_t e = ease(kSkipJiggleEase[s + 1], (j - kSkipJiggle[s][0]) * 1000 / span);
            const int32_t sq = kSkipJiggle[s][1] + scale(kSkipJiggle[s + 1][1] - kSkipJiggle[s][1], e, 1000);
            f.wide = f.wide * (1000 + sq) / 1000;
            f.tall = f.tall * (1000 - sq) / 1000;
        }
    }
    const int32_t home = skip_home(trip);
    if (t >= kSkipLookFrom && t < home + 200) { /* looking where he goes */
        f.look = -1;
    }
    f.happy = t >= home + 260 && t < home + 760; /* turned to you, ^ ^ */
    const int32_t u = trip >= 0 ? t - trip_catch(trip) : -1;
    if (u >= 0 && u < kTripBackU + kSkipWindUp) { /* tripped: its moments from its table, to the wind-up's end */
        if (u < kTripStumbleMs) {
            trip_stumble(f, u, trip, cell, dir);
        } else if (u < kTripBackU) { /* down: the faceplant's frames, standing in for every front-view part */
            f = react_rest();
            f.travel = scale(on_route(-(trip + 2) * kSkipStride, kSkipSpan, dir), cell, kFinishRefCell);
            f.look = -1;
            f.down = true;
            f.down_mirror = dir > 0;
            f.down_ms = static_cast<int16_t>(kTripFallFrom + std::max<int32_t>(u - kTripStumbleMs - kTripHoldMs, 0));
        }
        f.trip = true;
        f.trip_u = static_cast<int16_t>(u);
    }
    face(f, dir);
    return f;
}

/*
 * WHEN AN IDLE SKIP TRIPS — the owner's rule, 2026-11-10: about one idle skip in three, never two in a row, as the
 * faceplant's roll in the sweat's pool. Each stretch of the idle that skips rolls even odds of its own hash; of a run
 * of skipping stretches that all rolled it, the first trips, the next skips clean, the next trips and so on. Where
 * along the route, a hash of its own among the skips that may (trip_band()). The skip as an agent joins never trips.
 * Asked to stop `stopped` ms into the stretch, before the catch: no trip, he skips home plainly; at or after it, it
 * plays on.
 */
static_assert(theme::kMascotSkipTripMs <= kIdleStretchMs, "the tripped skip fits a stretch of the idle");
constexpr uint32_t kTripChain = 64; // the most stretches looked back over for a run of rolled skips

bool trip_rolled(uint32_t since, uint32_t stretch)
{
    return (mix((since + stretch * 0x9E3779B9u) ^ 0x1B56C4E9u) & 1u) == 0;
}

/* The skip stretch `stretch` of the idle begun at `since` going `dir`: the skip it trips at, or -1. */
int32_t idle_skip_trip(uint32_t since, uint32_t stretch, int dir, int32_t stopped)
{
    if (stretch_kind(stretch, since) != Stretch::kSkip || !trip_rolled(since, stretch)) {
        return -1;
    }
    uint32_t run = 0;
    for (uint32_t s = stretch, looked = 0; s > 1 && looked < kTripChain; looked++) {
        s--;
        if (stretch_kind(s, since) != Stretch::kSkip) {
            continue;
        }
        if (!trip_rolled(since, s)) {
            break;
        }
        run++;
    }
    if (run % 2 != 0) {
        return -1;
    }
    if (!kTripDrawnHere) {
        return -1; /* no trip off the glass it was drawn on */
    }
    const TripBand band = trip_band(dir);
    const int32_t trip =
        band.first + static_cast<int32_t>((mix((since + stretch * 0x9E3779B9u) ^ 0x2545F491u) >> 4) %
                                          static_cast<uint32_t>(band.count));
    return stopped >= trip_catch(trip) ? trip : -1;
}

/*
 * THE SPIN — a request allowed, or the 5-hour share reaching 50 %: a squat wind-up held a beat; the launch, stretched;
 * in the air a whole turn to his left, the legs tucked, kSpinViewMs a view — three-quarter, side, back three-quarter,
 * back, and mirrored round to the front (kTurnViews); falling stretched; the impact slammed flat, the arms flung up, ^
 * ^, dust kicked up at his feet; bounced back up off it, down again squashed, a long elastic settle — the developer's
 * ask; the arms held up, then down with a little squash. theme::kMascotSpinMs.
 */
constexpr ReactKey kSpin[] = {
    {0, 0, 0, 0, 0, 1000, 1000, Ease::kLinear},
    {100, 0, 0, 0, 0, 1000, 1000, Ease::kLinear},
    {180, 0, 0, 6, 0, 1100, 880, Ease::kOut},     // the wind-up: down into a squat
    {240, 0, 0, 7, 0, 1110, 870, Ease::kInOut},   // held a beat
    {290, 0, 8, 0, 0, 920, 1120, Ease::kOut},     // the launch: sprung up stretched
    {490, 0, 16, 0, 0, 990, 1020, Ease::kOut},    // the top, turning: the jump's rise
    {700, 0, 3, 0, 0, 960, 1070, Ease::kIn},      // falling, faster and faster, stretched
    {740, 0, 0, 6, 0, 1200, 800, Ease::kOut},     // the impact: slammed flat, the arms flung up, the dust
    {800, 0, 3, 0, 0, 930, 1100, Ease::kOut},     // bounced back up off it, stretched
    {850, 0, 0, 3, 0, 1100, 900, Ease::kIn},      // down again, squashed
    {900, 0, 0, 0, 0, 955, 1055, Ease::kInOut},   // the elastic settle: tall,
    {950, 0, 0, 0, 0, 1035, 970, Ease::kInOut},   // squat,
    {1000, 0, 0, 0, 0, 985, 1015, Ease::kInOut},  // tall,
    {1050, 0, 0, 0, 0, 1010, 993, Ease::kInOut},  // squat,
    {1100, 0, 0, 0, 0, 1000, 1000, Ease::kInOut}, // settled
    {1400, 0, 0, 0, 0, 1000, 1000, Ease::kLinear},
    {1440, 0, 0, 2, 0, 1060, 945, Ease::kOut},    // the arms down: a little squash
    {1490, 0, 0, 0, 0, 980, 1020, Ease::kInOut},
    {1540, 0, 0, 0, 0, 1000, 1000, Ease::kInOut},
    {static_cast<int16_t>(theme::kMascotSpinMs), 0, 0, 0, 0, 1000, 1000, Ease::kLinear},
};
static_assert(kSpin[std::size(kSpin) - 1].t == theme::kMascotSpinMs, "the spin's keys run its length");
constexpr int32_t kSpinTurnFrom = 290;
constexpr int32_t kSpinViewMs = 50;
constexpr int32_t kSpinViews = 7;
constexpr int32_t kSpinLandMs = 740;
static_assert(kSpin[7].t == kSpinLandMs && kSpinTurnFrom + kSpinViews * kSpinViewMs < kSpinLandMs,
              "the turn is done before he lands");

PixelPose spin_pose_at(int32_t t, int32_t cell)
{
    PixelPose f = react_rest();
    react_track(f, kSpin, t, cell);
    const int32_t k = t >= kSpinTurnFrom ? (t - kSpinTurnFrom) / kSpinViewMs : -1;
    if (k >= 0 && k < kSpinViews) {
        f.view = static_cast<int8_t>(1 + k);
    } else if (t >= kSpinTurnFrom - 40 && t < kSpinLandMs - 20) {
        f.raised = Raised::kBoth; /* in the air, the legs tucked */
    }
    if (t >= kSpinLandMs && t < 1420) {
        arms_up(f);
    }
    f.happy = t >= kSpinLandMs && t < 1450;
    if (t >= kSpinLandMs) {
        f.dust = static_cast<int16_t>(std::min<int32_t>(t - kSpinLandMs + 1, INT16_MAX));
    }
    return f;
}

/*
 * THE DUST JUMP — a sub-agent finished: a wind-up; the launch, the arms up, ^ ^; the top; a squashed impact kicking up
 * a puff of dust at each side of his feet; straight back up off it, a second bounce, smaller, another puff; the elastic
 * settle, the arms down. theme::kMascotDustMs.
 */
constexpr ReactKey kDust[] = {
    {0, 0, 0, 0, 0, 1000, 1000, Ease::kLinear},
    {80, 0, 0, 0, 0, 1000, 1000, Ease::kLinear},
    {180, 0, 0, 4, 0, 1080, 910, Ease::kOut},     // the wind-up
    {240, 0, 0, 6, 0, 1100, 880, Ease::kInOut},
    {290, 0, 6, 0, 0, 930, 1100, Ease::kOut},     // the launch, the arms up
    {450, 0, 16, 0, 0, 980, 1040, Ease::kOut},    // the top: the jump's rise
    {600, 0, 3, 0, 0, 960, 1060, Ease::kIn},      // falling
    {640, 0, 0, 4, 0, 1150, 840, Ease::kOut},     // the impact, a puff of dust
    {700, 0, 0, 2, 0, 1080, 920, Ease::kOut},     // straight back up off it:
    {740, 0, 5, 0, 0, 940, 1080, Ease::kOut},     // the second bounce, smaller
    {880, 0, 12, 0, 0, 985, 1030, Ease::kOut},
    {1010, 0, 2, 0, 0, 965, 1055, Ease::kIn},
    {1050, 0, 0, 3, 0, 1130, 860, Ease::kOut},    // the impact, another puff
    {1100, 0, 0, 0, 0, 950, 1070, Ease::kInOut},  // the elastic settle
    {1150, 0, 0, 0, 0, 1040, 965, Ease::kInOut},
    {1200, 0, 0, 0, 0, 985, 1015, Ease::kInOut},
    {1250, 0, 0, 0, 0, 1000, 1000, Ease::kInOut},
    {static_cast<int16_t>(theme::kMascotDustMs), 0, 0, 0, 0, 1000, 1000, Ease::kLinear},
};
static_assert(kDust[std::size(kDust) - 1].t == theme::kMascotDustMs, "the dust jump's keys run its length");
/* The spin and the dust jump never lean in their own frames: their bigger canvas (body_canvas()) draws through no
 * layer while they play. */
template <size_t N> constexpr bool upright(const ReactKey (&keys)[N])
{
    for (const ReactKey &k : keys) {
        if (k.lean != 0 || k.x != 0) {
            return false;
        }
    }
    return true;
}
static_assert(upright(kSpin) && upright(kDust), "the spin and the dust jump stand upright, in place");
constexpr int32_t kDustLands[2] = {640, 1050};
constexpr int32_t kDustPuffMs = 320; // a puff, kicked up to gone (kPuff)
static_assert(kDust[7].t == kDustLands[0] && kDust[12].t == kDustLands[1], "the puffs are the landings'");

PixelPose dust_pose_at(int32_t t, int32_t cell)
{
    PixelPose f = react_rest();
    react_track(f, kDust, t, cell);
    if ((t >= 250 && t < kDustLands[0]) || (t >= 700 && t < kDustLands[1])) {
        arms_up(f);
    }
    f.happy = t >= 250 && t < 1300;
    if (f.lift > scale(3, cell, kFinishRefCell)) {
        f.raised = Raised::kBoth; /* off the ground, the legs tucked */
    }
    for (const int32_t land : kDustLands) {
        if (t >= land && t - land < kDustPuffMs) {
            f.dust = static_cast<int16_t>(t - land + 1);
        }
    }
    return f;
}

uint32_t reaction_ms(MascotMove move)
{
    return move == MascotMove::kSkip       ? theme::kMascotSkipMs
           : move == MascotMove::kSkipTrip ? (kTripDrawnHere ? theme::kMascotSkipTripMs : theme::kMascotSkipMs)
           : move == MascotMove::kSpin     ? theme::kMascotSpinMs
                                           : theme::kMascotDustMs;
}

/* THE SKIP TRIP ALONE (kSkipTrip, the stage's and the specimen's): the drafted trips, the way picked as the skip's —
 * on the landscape glass at skip 14 going left and 3 going right, on the round glass, on the way home, at 16 going left
 * and 18 going right (theme::kMascotTripsHomeward); on a glass with no trip drawn (kTripDrawnHere false), the plain
 * skip, its length (reaction_ms()) the skip's. */
/* On the Sticky's glass at the 24 px cell, the mascot centred, the stage trips at the first skip either way, leaving
 * home — the band's own (trip_fits) `[DER]` (mock 4, 2026-11-24). */
constexpr int32_t kStageTrip[2] = {0, 0};
static_assert(!kTripDrawnHere || (trip_fits(kStageTrip[0], -1) && trip_fits(kStageTrip[1], 1)),
              "the stage's trips are inside the band");
constexpr int32_t stage_trip(int dir) { return kTripDrawnHere ? kStageTrip[dir < 0 ? 0 : 1] : -1; }

PixelPose reaction_pose_at(MascotMove move, int32_t t, int32_t cell, int dir)
{
    return move == MascotMove::kSkip       ? skip_pose_at(t, cell, dir, -1)
           : move == MascotMove::kSkipTrip ? skip_pose_at(t, cell, dir, stage_trip(dir))
           : move == MascotMove::kSpin     ? spin_pose_at(t, cell)
                                           : dust_pose_at(t, cell);
}

/*
 * THE WAVE (AGENT_DECK 9.3): no sheet covers it — drawn from the character and the motion principles, the
 * developer's choices of the draft at its real speed, 2026-10-29. The right hand: a wind-up, the hand cocked; the
 * raise, sprung up a little, stretched; six swings over his head, the hand's top leading, the body tilting against it
 * and bouncing in the knees at each turn; the hand down with a squash, the wobble. A pause. The left hand, the same
 * mirrored. Then, impatient, the eyes > <, both hands flapping like wings — slammed down from over his top corners to
 * his sides, overshooting and springing back, whipped up past the top and springing back, the body lifted off and
 * stretched as they beat down and landing squashed as they rise, rocking side to side — four flaps; the hands slapped
 * down, the wobble. theme::kMascotWaveMs. Per key, px at kFinishRefCell, per mille, 0.1°.
 */
struct WaveKey {
    int16_t t;
    int8_t lift;
    int16_t wide, tall, lean;
    Ease ease;
};

constexpr int32_t kWaveSwingMs = 200; // a swing of the hand, out or in
constexpr int32_t kWaveSwings = 6;
constexpr int32_t kWaveHandMs = 1800; // one hand's wave, wind-up to settled
static_assert(kWaveHandMs == static_cast<int32_t>(kMascotHelloMs), "mascot.h's kMascotHelloMs is the hello's length");
constexpr int32_t kWaveLeftFrom = 2200;
constexpr int32_t kWaveFlapFrom = 4000;
constexpr int32_t kWaveFlapMs = 260;
constexpr int32_t kWaveFlaps = 4;
constexpr WaveKey kWaveHand[] = {
    {0, 0, 1000, 1000, 0, Ease::kLinear},
    {60, 0, 1000, 1000, 0, Ease::kLinear},
    {160, 0, 1070, 935, -15, Ease::kOut},  // the wind-up: squashed, leaning away, the hand cocked
    {260, 3, 950, 1080, 12, Ease::kOut},   // the raise: sprung up a little, stretched, the hand shooting up
    {330, 0, 1000, 1000, 0, Ease::kInOut}, // down into the wave
    {260 + kWaveSwings * kWaveSwingMs, 0, 1000, 1000, 0, Ease::kLinear}, // the swings (theirs on top)
    {1560, 0, 1000, 1000, 0, Ease::kLinear},
    {1640, 0, 1080, 920, 0, Ease::kOut},   // the hand down: a squash as it lands
    {1690, 0, 960, 1050, 0, Ease::kInOut}, // the elastic settle: tall,
    {1740, 0, 1025, 980, 0, Ease::kInOut}, // squat,
    {1790, 0, 1000, 1000, 0, Ease::kInOut}, // settled
    {kWaveHandMs, 0, 1000, 1000, 0, Ease::kLinear},
};
/* One swing: (t, side −1000 in … 1000 out, wide, tall) — the hand passing over, a bounce in the knees at the turn. */
constexpr int16_t kWaveSwing[4][4] = {{0, -1000, 1030, 975}, {70, -300, 995, 1010}, {150, 800, 1000, 1000},
                                      {kWaveSwingMs, 1000, 1030, 975}};
constexpr Ease kWaveSwingEase[4] = {Ease::kLinear, Ease::kOut, Ease::kInOut, Ease::kOut};
/* The flapping, from kWaveFlapFrom: (t, lift, wide, tall) — a wind-up, the flaps (theirs on top), the slap down. */
constexpr int16_t kWaveFlap[8][4] = {
    {0, 0, 1000, 1000},
    {100, 0, 1060, 945},                                       // a squashed wind-up, the eyes going > <
    {100 + kWaveFlaps * kWaveFlapMs, 0, 1000, 1000},           //
    {100 + kWaveFlaps * kWaveFlapMs + 60, 0, 1080, 920},       // the hands slapped down: a squash
    {100 + kWaveFlaps * kWaveFlapMs + 110, 0, 960, 1050},      //
    {100 + kWaveFlaps * kWaveFlapMs + 160, 0, 1025, 980},      //
    {100 + kWaveFlaps * kWaveFlapMs + 210, 0, 1000, 1000},     //
    {static_cast<int16_t>(theme::kMascotWaveMs - kWaveFlapFrom), 0, 1000, 1000},
};
constexpr Ease kWaveFlapEase[8] = {Ease::kLinear, Ease::kOut,   Ease::kLinear, Ease::kOut,
                                   Ease::kInOut,  Ease::kInOut, Ease::kInOut,  Ease::kLinear};
/* One flap: (t, the hands 0 high … 1000 low, lift, wide, tall) — slammed down faster and faster, overshooting and
 * springing back, whipped up past the top and springing back. */
constexpr int16_t kWaveBeat[7][5] = {{0, -40, 0, 1040, 960},    {70, 1000, 6, 950, 1060}, {105, 1120, 7, 960, 1050},
                                     {140, 950, 6, 980, 1030},  {200, -140, 1, 1060, 940}, {235, 40, 0, 990, 1015},
                                     {kWaveFlapMs, -40, 0, 1040, 960}};
constexpr Ease kWaveBeatEase[7] = {Ease::kLinear, Ease::kIn, Ease::kOut, Ease::kInOut, Ease::kOut, Ease::kInOut,
                                   Ease::kInOut};
static_assert(kWaveFlapFrom + 100 + kWaveFlaps * kWaveFlapMs + 210 <= static_cast<int32_t>(theme::kMascotWaveMs),
              "the wave's flapping settles within theme::kMascotWaveMs");

/* A row of a key table at `t`: the keys either side, eased into the later. */
template <size_t N, size_t M> void keyed(const int16_t (&keys)[N][M], const Ease (&eases)[N], int32_t t, int32_t *out)
{
    size_t i = 0;
    while (i + 2 < N && keys[i + 1][0] <= t) {
        i++;
    }
    const int32_t span = keys[i + 1][0] - keys[i][0];
    const int32_t x = ease(eases[i + 1], std::clamp<int32_t>(t - keys[i][0], 0, span) * 1000 / span);
    for (size_t c = 1; c < M; c++) {
        out[c - 1] = keys[i][c] + scale(keys[i + 1][c] - keys[i][c], x, 1000);
    }
}

/* The right hand's wave `u` ms in: the physics, and its hand (col, row, shear) — row −1 for the hand at rest. */
void wave_hand(PixelPose &f, int32_t u, int32_t cell, int8_t *col, int8_t *row, int8_t *shear)
{
    size_t i = 0;
    while (i + 2 < std::size(kWaveHand) && kWaveHand[i + 1].t <= u) {
        i++;
    }
    const WaveKey &a = kWaveHand[i];
    const WaveKey &b = kWaveHand[i + 1];
    const int32_t span = b.t - a.t;
    const int32_t x = ease(b.ease, std::clamp<int32_t>(u - a.t, 0, span) * 1000 / span);
    const auto at = [&](int32_t from, int32_t to) { return from + scale(to - from, x, 1000); };
    f.lift = scale(at(a.lift, b.lift), cell, kFinishRefCell);
    f.wide = at(a.wide, b.wide);
    f.tall = at(a.tall, b.tall);
    f.lean = at(a.lean, b.lean);
    *col = 31;
    *row = -1;
    *shear = 0;
    constexpr int32_t kSwingsEnd = 260 + kWaveSwings * kWaveSwingMs;
    if (u >= 60 && u < 160) {
        *row = 15; /* cocked */
    } else if (u >= 160 && u < 260) {
        *row = 9; /* on the way up */
    } else if (u >= 260 && u < kSwingsEnd) {
        const int32_t k = (u - 260) / kWaveSwingMs;
        int32_t sw[3];
        keyed(kWaveSwing, kWaveSwingEase, (u - 260) % kWaveSwingMs, sw);
        const int32_t side = k % 2 != 0 ? -sw[0] : sw[0]; /* every other swing comes back in */
        f.wide = f.wide * sw[1] / 1000;
        f.tall = f.tall * sw[2] / 1000;
        f.lean -= side * 22 / 1000; /* the body tilting against the hand */
        *col = static_cast<int8_t>(31 + scale(side, 2, 1000));
        *row = 6;
        *shear = static_cast<int8_t>(side > 300 ? 1 : side < -300 ? -1 : 0);
    } else if (u >= kSwingsEnd && u < 1640) {
        *row = static_cast<int8_t>(u < 1560 ? 6 : 10); /* the last swing done, the hand coming down */
    }
}

/* The wave's flapping `u` ms in (from kWaveFlapFrom): both hands beating like wings into `f`, the body rocking. */
void wave_flap(PixelPose &f, int32_t u, int32_t cell)
{
    int32_t fl[3];
    keyed(kWaveFlap, kWaveFlapEase, u, fl);
    f.lift = scale(fl[0], cell, kFinishRefCell);
    f.wide = fl[1];
    f.tall = fl[2];
    f.chevron = u < 100 + kWaveFlaps * kWaveFlapMs + 160;
    if (u >= 100 && u < 100 + kWaveFlaps * kWaveFlapMs) {
        const int32_t k = (u - 100) / kWaveFlapMs;
        const int32_t into = (u - 100) % kWaveFlapMs;
        int32_t bt[4];
        keyed(kWaveBeat, kWaveBeatEase, into, bt);
        f.lift += scale(bt[1], cell, kFinishRefCell);
        f.wide = f.wide * bt[2] / 1000;
        f.tall = f.tall * bt[3] / 1000;
        /* like wings: up over the top corners at row 7, slammed down and out to his sides at row 16 */
        const auto r = static_cast<int8_t>(7 + scale(bt[0], 9, 1000));
        const auto out = static_cast<int8_t>(scale(std::clamp<int32_t>(bt[0], 0, 1000), 2, 1000));
        f.left_col = static_cast<int8_t>(15 - out);
        f.right_col = static_cast<int8_t>(31 + out);
        f.left_row = f.right_row = r;
        /* rocking side to side, smoothly, flap to flap */
        const int32_t half = kWaveFlapMs / 2;
        const int32_t rock = into < half ? -60 + scale(120, ease(Ease::kInOut, into * 1000 / half), 1000)
                                         : 60 - scale(120, ease(Ease::kInOut, (into - half) * 1000 / half), 1000);
        f.lean = (k % 2 != 0 ? rock : -rock) / 10;
        if (f.lift > scale(3, cell, kFinishRefCell)) {
            f.raised = Raised::kBoth; /* off the ground: the legs tucking */
        }
    }
}

PixelPose wave_pose_at(int32_t t, int32_t cell)
{
    PixelPose f = {};
    f.flag = -1;
    f.bursts = {-1, -1};
    f.wide = f.tall = 1000;
    f.left_col = 15;
    f.left_row = 14;
    f.right_col = 31;
    f.right_row = 14;
    int8_t col = 0;
    int8_t row = 0;
    int8_t shear = 0;
    if (t < kWaveHandMs) {
        wave_hand(f, t, cell, &col, &row, &shear);
        if (row >= 0) {
            f.right_col = col;
            f.right_row = row;
            f.right_shear = shear;
        }
    } else if (t >= kWaveLeftFrom && t < kWaveLeftFrom + kWaveHandMs) {
        wave_hand(f, t - kWaveLeftFrom, cell, &col, &row, &shear);
        f.lean = -f.lean; /* the left hand's: the right's, mirrored */
        if (row >= 0) {
            f.left_col = static_cast<int8_t>(46 - col);
            f.left_row = row;
            f.left_shear = static_cast<int8_t>(-shear);
        }
    } else if (t >= kWaveFlapFrom) {
        wave_flap(f, t - kWaveFlapFrom, cell);
    }
    return f;
}

/* Stopped part way: a hand mid-wave comes down, the flapping ends; the pause is home already. Where it carries on from,
 * and where it ends. */
void wave_outro(int32_t s, int32_t *from, int32_t *end)
{
    if (s < kWaveHandMs) {
        *from = std::max<int32_t>(s, 260 + kWaveSwings * kWaveSwingMs);
        *end = kWaveHandMs;
    } else if (s < kWaveLeftFrom) {
        *from = *end = s;
    } else if (s < kWaveFlapFrom) {
        *from = std::max<int32_t>(s, kWaveLeftFrom + 260 + kWaveSwings * kWaveSwingMs);
        *end = kWaveLeftFrom + kWaveHandMs;
    } else {
        *from = std::max<int32_t>(s, kWaveFlapFrom + 100 + kWaveFlaps * kWaveFlapMs);
        *end = static_cast<int32_t>(theme::kMascotWaveMs);
    }
}

int32_t wave_time(const MascotPlay &play, uint32_t now)
{
    const uint32_t e = now - play.since;
    if (play.stopped == kStillPlaying) {
        return static_cast<int32_t>(e % theme::kMascotWaveMs);
    }
    int32_t from = 0;
    int32_t end = 0;
    wave_outro(static_cast<int32_t>((play.stopped - play.since) % theme::kMascotWaveMs), &from, &end);
    return std::min<int32_t>(from + static_cast<int32_t>(std::min<uint32_t>(now - play.stopped, theme::kMascotWaveMs)),
                             end);
}

/* NEEDS YOU, AT RANDOM — the developer's ask, 2026-10-29: in turns of theme::kMascotWaveMs, each two hops and a rest or
 * the wave, about one in three a wave, never the first, picked by a hash of the turn's number. */
bool turn_waves(uint32_t turn);
constexpr uint32_t kJumpTurnHops = 2;
static_assert(kJumpTurnHops * theme::kMascotJumpMs <= theme::kMascotWaveMs, "two hops fit a turn of the wave's length");

/* Needs you at `now`: whether this turn waves, and the ms into it — stopped, a wave's outro, the hops on their clock.
 * */
struct NeedsAt {
    bool wave;
    int32_t u;
};

NeedsAt needs_at(const MascotPlay &play, uint32_t now)
{
    const uint32_t at = play.stopped == kStillPlaying ? now : play.stopped;
    const bool wave = turn_waves((at - play.since) / theme::kMascotWaveMs);
    if (play.stopped == kStillPlaying || !wave) {
        return {wave, static_cast<int32_t>((now - play.since) % theme::kMascotWaveMs)};
    }
    int32_t from = 0;
    int32_t end = 0;
    wave_outro(static_cast<int32_t>((play.stopped - play.since) % theme::kMascotWaveMs), &from, &end);
    return {true, std::min<int32_t>(from + static_cast<int32_t>(std::min<uint32_t>(now - play.stopped,
                                                                                   theme::kMascotWaveMs)),
                                    end)};
}

/* The sweat as drawn over a work frame: the wipe's hand, the big bead's size and its foot's row, the little one's size
 * (-1 for none, 0 a bit, 1 thin, 2 a teardrop), and how far into the burst the beads are, or -1. */
struct SweatView {
    int8_t hand_col = -1;
    int8_t hand_row = 0;
    int8_t big = -1;
    int8_t foot = 0;
    int8_t little = -1;
    int16_t pop = -1;
    bool operator==(const SweatView &) const = default;
};

/*
 * THE STRESS LINES (AGENT_DECK 9.3), face down in the faceplant: three pale blue lines in theme::mascot_sweat() — the
 * sweat's — that start on his body, the upper right of the big face of the face-down block, a pixel under its top edge,
 * and drip down over it one after another, wobbling side to side by half a pixel as they slide, then thin to half a
 * pixel and vanish: the approved draft 16's, a full pixel wide, 2.5 pixels long, dripping 3.5 pixels, easing out as it
 * runs dry. In px at the deck's cell (theme::kMascotCell, its 8 px pixel) from the work canvas's corner; `t` ms from
 * the faceplant's start.
 */
struct StressLine {
    int16_t t, x, y;
};
constexpr StressLine kStressLines[] = {{1210, 154, 96}, {1330, 170, 96}, {1450, 186, 96}};
constexpr int32_t kStressMs = 600;      // each line's life
constexpr int32_t kStressGrowMs = 100;  // welling out
constexpr int32_t kStressThinAt = 360;  // thinning to half a pixel, and shortening from the top
constexpr int32_t kStressWide = 8;
constexpr int32_t kStressThin = 4;
constexpr int32_t kStressLong = 20;
constexpr int32_t kStressSlide = 28;
constexpr int32_t kStressWobbleMs = 80; // half a pixel each way, a step every 80 ms as it slides
constexpr int8_t kStressWobble[] = {0, 4, 0, -4};

struct StressRect {
    int16_t x = 0, y = 0, w = 0, h = 0; // px at kMascotCell; h 0 for none
    bool operator==(const StressRect &) const = default;
};
struct StressView {
    StressRect lines[std::size(kStressLines)];
    bool operator==(const StressView &) const = default;
};

/* The stress lines `e` ms into the faceplant, as the draft drew them. */
StressView stress_at(int32_t e)
{
    StressView view;
    for (size_t i = 0; i < std::size(kStressLines); i++) {
        const StressLine &l = kStressLines[i];
        const int32_t u = e - l.t;
        if (u < 0 || u >= kStressMs) {
            continue;
        }
        /* welling out, then shortening from the top, per mille; the length rounded to half pixels */
        const int32_t grow = std::min(u, kStressGrowMs) * 1000 / kStressGrowMs;
        const int32_t keep = u < kStressThinAt ? 1000 : (kStressMs - u) * 1000 / (kStressMs - kStressThinAt);
        const int32_t whole = kStressLong / 4 * grow * keep; /* rounded half to even, as the draft's round() */
        const int32_t quarters = whole / 1000000 + (whole % 1000000 > 500000 ||
                                                     (whole % 1000000 == 500000 && (whole / 1000000) % 2 == 1));
        if (quarters <= 0) {
            continue;
        }
        const int32_t length = 4 * quarters;
        /* the slide eases out, 28 (1 - (1 - f)^2), rounded down */
        const int32_t left = kStressMs - u;
        const int32_t span = kStressMs * kStressMs;
        const int32_t slide = kStressSlide - (kStressSlide * left * left + span - 1) / span;
        const int32_t w = u < kStressThinAt ? kStressWide : kStressThin;
        const int32_t dx = kStressWobble[(u / kStressWobbleMs) % std::size(kStressWobble)] + (kStressWide - w) / 2;
        view.lines[i] = {static_cast<int16_t>(l.x + dx), static_cast<int16_t>(l.y + kStressLong + slide - length),
                         static_cast<int16_t>(w), static_cast<int16_t>(length)};
    }
    return view;
}

/* WHAT A MASCOT IS, hung off its root's user data and freed with it: its colours, its move, the rig's pose last drawn
 * and the pixel frame on view — a frame that would draw the same, as through the rest between hops, touches nothing
 * and redraws nothing. */
struct Motion {
    Mascot m;
    MascotMove move;
    Pose last;
    bool drawn;
    int frame;     // the pixel frame on view, or -1
    WorkPose work; // the physics it is drawn with
    lv_color_t body;
    lv_color_t shade;
    lv_color_t tint;
    MascotPlay play;    // what it is doing: the move on view, from when, and what follows
    int32_t outro_from; // stopped work: the ms into the loop the putting away starts from, or -1 for none
    lv_timer_t *blink_timer;
    bool blink;         // the eyes shut
    PixelPose pose;     // the front-view pixel move's pose drawn
    bool front;         // drawn in the front-view pixels now: the finish, celebrate, idle, wave, or needs-you's wave
    SweatView sweat;    // the work's sweat drawn
    MascotSfxFn sfx;    // where its moments go (set_mascot_sfx()), or nullptr
    void *sfx_user;
    int32_t work_t;     // the work's ms into its loop at the last tick, for the moments it passed; -1 before one
    bool airborne;      // off the ground past half a cell since the last landing, the rig's or a pixel move's
    bool crouched;      // down in a squat past a quarter of a cell since it last stood up
    bool hushed;        // a first frame: the moments' state is learnt from the pose, nothing told
    int8_t sway_side;   // the side it last leant to past kSwayFrom or kTiltFrom, -1 left, 1 right, 0 none since a rock
    int32_t stretch;    // the idle's stretch drawn, a front-view move's own idle or the one after it; -1 for none
    uint8_t look_still; // ticks the eyes have held where they look, a still frame's too, up to kLookHeld
    int32_t sweat_t;    // ms into the sweat on the last frame, or -1: for the wipe's moments
    lv_area_t canvas{}; // the pixels' whole canvas for the move on view, px from the root's corner (show())
    lv_point_t pivot{}; // its tilt's pivot, px from the canvas's corner
    lv_point_t crop{};  // where the pixels' object sits in the canvas now, px from its corner (crop_pixels())
    lv_point_t legs_crop{}; // where the legs' object sits in their canvas, kLegsCanvas, px from its corner (crop_legs())
    int32_t fp_e = -1;      // the faceplant on view: ms into it, or -1 for none (its frame, lift and stress lines)
    int fp_pose = -1;       // its body's frame drawn, into detail::kFaceplantPoses, or -1
    int fp_hand = -1;       // its hands' frame drawn, into detail::kFaceplantHands
    int32_t fp_hand_lift = 0; // the hands' lift, px: the draft keeps them down while he jerks awake and springs
    StressView stress{};    // its stress lines drawn
    bool fp_mirror = false; // the faceplant drawn mirrored, facing right: the skip trip going right
    bool fp_laptop = true;  // the faceplant's laptop drawn: none in the skip trip, he falls on the open ground
};

/* A moment told, to whoever listens. */
void tell(const Motion *motion, MascotSfx sfx)
{
    if (motion->sfx != nullptr && !motion->hushed) {
        motion->sfx(sfx, motion->sfx_user);
    }
}

/* THE WORK'S MOMENTS, at their keys' times in kWorkMotion: the hop out and its landing, the laptop set down, its lid
 * flipped up, the crouch to the keys landing, the relief, the lid snapped shut, the hop home and its landing. The
 * typing between is the deck's working loop, the wink an eye's. */
struct WorkMoment {
    uint32_t t;
    MascotSfx sfx;
};
constexpr WorkMoment kWorkMoments[] = {
    {575, MascotSfx::kHop},  {690, MascotSfx::kLand},      {900, MascotSfx::kLaptopDown}, {935, MascotSfx::kLidOpen},
    {1095, MascotSfx::kLand}, {2445, MascotSfx::kStretch}, {2695, MascotSfx::kLidShut},   {2905, MascotSfx::kHop},
    {2960, MascotSfx::kLand},
};

/* The wipe's beats no field of its frame shows, in ms into the wipe (kWipe's keys): the flick off his side and the
 * sigh, all the air out of him. */
constexpr int32_t kWipeFlickMs = 445;
constexpr int32_t kWipeSighMs = 640;

/* The moments the work passed from `from` to `to` ms into its loop; none across the typing loop's wrap. */
void tell_work(const Motion &motion, uint32_t from, uint32_t to)
{
    for (const WorkMoment &w : kWorkMoments) {
        if (from < w.t && w.t <= to) {
            tell(&motion, w.sfx);
        }
    }
}

/* THE FACEPLANT'S MOMENTS (AGENT_DECK 9.3, 17), at the approved draft 16's times in ms from its start — its event log
 * as drafted, each movement its sound: the nods' blinks, the jerk awake's stretch, the sag's crouch, the lurch back's
 * sway, the topple and the flop; the bounce's second contact, a soft tick; each stress line's bead; each flap's hand up
 * (a chirp) and slap on the ground (a fwap); each kick's landing, a diagonal pair at a time (a step's tick, 70 ms after
 * a slap, so no two share an effect voice past the bead's); the give-up's crouch; the push-up's hands on the floor and
 * its legs coming down; the spring's hop and its > < eyes, the landing; the head shake's turns, a whoosh as each
 * three-quarter view is reached; the settle back to typing's squash and tick (kFaceplantUpMs on: the work's only — the
 * limit's puts the laptop away instead). */
constexpr WorkMoment kFaceplantMoments[] = {
    {230, MascotSfx::kBlink}, {330, MascotSfx::kStretch}, {520, MascotSfx::kBlink}, {540, MascotSfx::kCrouch},
    {830, MascotSfx::kSway}, {890, MascotSfx::kTopple}, {960, MascotSfx::kFlop}, {1095, MascotSfx::kLand},
    {1210, MascotSfx::kBead}, {1230, MascotSfx::kHand}, {1285, MascotSfx::kHandDown}, {1325, MascotSfx::kHand},
    {1330, MascotSfx::kBead}, {1355, MascotSfx::kStep}, {1380, MascotSfx::kHandDown}, {1420, MascotSfx::kHand},
    {1450, MascotSfx::kBead}, {1450, MascotSfx::kStep}, {1475, MascotSfx::kHandDown}, {1515, MascotSfx::kHand},
    {1545, MascotSfx::kStep}, {1570, MascotSfx::kHandDown}, {1610, MascotSfx::kHand}, {1640, MascotSfx::kStep},
    {1665, MascotSfx::kHandDown}, {1705, MascotSfx::kHand}, {1735, MascotSfx::kStep}, {1760, MascotSfx::kHandDown},
    {1800, MascotSfx::kHand}, {1830, MascotSfx::kStep}, {1855, MascotSfx::kHandDown}, {1895, MascotSfx::kHand},
    {1925, MascotSfx::kStep}, {1950, MascotSfx::kHandDown}, {1990, MascotSfx::kHand}, {2020, MascotSfx::kStep},
    {2045, MascotSfx::kHandDown}, {2100, MascotSfx::kCrouch}, {2710, MascotSfx::kHandDown}, {2760, MascotSfx::kStep},
    {2880, MascotSfx::kHop}, {2885, MascotSfx::kChevron}, {2965, MascotSfx::kLand}, {3280, MascotSfx::kTurn},
    {3430, MascotSfx::kTurn}, {3590, MascotSfx::kTurn}, {3760, MascotSfx::kTurn}, {3900, MascotSfx::kCrouch},
    {4010, MascotSfx::kLand},
};

/* The faceplant's moments from `from` to `to` ms into it, before `end` ms. */
void tell_faceplant(const Motion &motion, int32_t from, int32_t to, uint32_t end)
{
    for (const WorkMoment &w : kFaceplantMoments) {
        if (from < static_cast<int32_t>(w.t) && static_cast<int32_t>(w.t) <= to && w.t < end) {
            tell(&motion, w.sfx);
        }
    }
}

/* THE SKIP TRIP'S MOMENTS (AGENT_DECK 17), ms since he caught his foot, its approved draft 4's: a lean, the arms out,
 * the two staggers, the arms in, the turn into the three-quarter view; the faceplant's own from its rock back to the
 * head shake's end, each at its time there less the rock back's (kFaceplantMoments: the topple, the flop, the bounce's
 * second contact, the beads, the flaps and kicks, the give-up, the push-up, the spring and its > <, the landing, the
 * shake's turns); the turn back to the front. The catch's landing is the skip's own, told by the front view. */
constexpr WorkMoment kTripMoments[] = {
    {40, MascotSfx::kSway},
    {70, MascotSfx::kHand},
    {120, MascotSfx::kStep},
    {230, MascotSfx::kStep},
    {280, MascotSfx::kHandDown},
    {static_cast<uint32_t>(kTripStumbleMs), MascotSfx::kTurn},
    {static_cast<uint32_t>(kTripBackU), MascotSfx::kTurn},
};
/*
 * THE TRIP'S MOMENTS ACROSS A NEW MASCOT, as the faceplant's (FaceplantTold): a record that rebuilds the page builds a
 * new mascot part way through a trip, so the moments a trip has told are kept here by when he caught his foot, not by
 * the mascot — the one drawing it next tells on from there, never a moment twice and never one lost between the two.
 */
struct TripTold {
    uint32_t caught; // when he caught his foot (an lv_tick_get() time)
    int32_t u;       // ms since it its moments are told up to, or -1
};
TripTold g_trip_told = {0, -1};
constexpr int32_t kTripNotDrawn = INT32_MAX; // a mascot's first frame: nothing of its own told before

void tell_trip(const Motion &motion, int32_t from, int32_t to);

/* Tells the trip caught at `caught` up to `u` ms since it: from `was`, where this mascot's last frame had it (-1 just
 * before the catch, kTripNotDrawn on its first frame), or from where the trip's moments were told, whichever is
 * later. */
void trip_moments(const Motion &motion, uint32_t caught, int32_t u, int32_t was)
{
    const bool same = g_trip_told.caught == caught && g_trip_told.u >= 0;
    const int32_t from = same ? std::max(was == kTripNotDrawn ? -1 : was, g_trip_told.u) : was;
    tell_trip(motion, from, u);
    if (!same || u > g_trip_told.u) {
        g_trip_told = {caught, u};
    }
}

/* Since the catch, the faceplant's time `t` there. */
constexpr int32_t trip_u_of(uint32_t t)
{
    return static_cast<int32_t>(t) - kTripFallFrom + kTripStumbleMs + kTripHoldMs;
}
static_assert(trip_u_of(890) == 420 && trip_u_of(3760) == 3290, "the trip's moments are the approved draft 4's times");

/* The trip's moments from `from` to `to` ms since the catch. */
void tell_trip(const Motion &motion, int32_t from, int32_t to)
{
    for (const WorkMoment &w : kTripMoments) {
        if (from < static_cast<int32_t>(w.t) && static_cast<int32_t>(w.t) <= to) {
            tell(&motion, w.sfx);
        }
    }
    for (const WorkMoment &w : kFaceplantMoments) {
        const int32_t u = trip_u_of(w.t);
        if (static_cast<int32_t>(w.t) > kTripFallFrom && static_cast<int32_t>(w.t) < kTripUpMs && from < u && u <= to) {
            tell(&motion, w.sfx);
        }
    }
}

/*
 * A FACEPLANT'S MOMENTS ACROSS A NEW MASCOT: a record that rebuilds the page builds a new mascot, and the work's
 * faceplant carried on as kFaceplant begins anew (faceplant_on()) — so the moments a faceplant has told are kept here
 * by when it began, not by the mascot: the one drawing it next tells on from there, never a moment twice and never one
 * lost between the two. A mascot come to a faceplant some other way says nothing on its first frame.
 */
struct FaceplantTold {
    uint32_t start; // when the faceplant began (an lv_tick_get() time)
    int32_t e;      // ms into it its moments are told up to, or -1
};
FaceplantTold g_faceplant_told = {0, -1};

/* Tells the faceplant begun at `start` from where its moments were told up to `e` ms into it, before `end`; `drawn`, a
 * mascot whose work was on view the frame before (into the faceplant from the typing). */
void faceplant_moments(const Motion &motion, uint32_t start, int32_t e, uint32_t end, bool drawn)
{
    const bool same = g_faceplant_told.start == start && g_faceplant_told.e >= 0;
    const int32_t from = motion.fp_e >= 0 ? motion.fp_e : same ? g_faceplant_told.e : drawn ? -1 : INT32_MAX;
    tell_faceplant(motion, std::max(from, same ? g_faceplant_told.e : from), e, end);
    if (!same || e > g_faceplant_told.e) {
        g_faceplant_told = {start, e};
    }
}

/* DOWN INTO A SQUAT — a wind-up, a crouch to the keys: once the body drops past a quarter of a cell, told again only
 * once it has stood up. `drop` px down, the rig's negative lift or a pixel move's crouch. */
void tell_crouch(Motion &motion, int32_t drop)
{
    const int32_t from = motion.m.cell / 4;
    if (!motion.crouched && drop >= from) {
        motion.crouched = true;
        tell(&motion, MascotSfx::kCrouch);
    } else if (motion.crouched && drop <= 0) {
        motion.crouched = false;
    }
}

/* OFF THE GROUND AND BACK ON IT, the rig's or a pixel move's: a hop once the body rises past half a cell — a walk's
 * bob of a step stays under it — and a landing once it is down within a quarter of one, or squashing into the ground
 * below the hop's height: a key that touches the ground for an instant may fall between frames, so the landing is
 * never waited for at 0.
 * Below the ground, or `crouch` px down in a pixel move's squat, on the ground: a crouch. */
void tell_lift(Motion &motion, int32_t lift, int32_t crouch = 0)
{
    const int32_t from = motion.m.cell / 2;
    if (!motion.airborne && lift >= from) {
        motion.airborne = true;
        tell(&motion, MascotSfx::kHop);
    } else if (motion.airborne && (lift <= motion.m.cell / 4 || (crouch > 0 && lift < from))) {
        /* A squat lands only below the hop's height: a key springing from a squat to a hop passes poses both lifted
         * and squatting, where a land would flip with the hop every frame. */
        motion.airborne = false;
        motion.crouched = true;  // the landing's squash is the landing's sound, not a crouch of its own
        tell(&motion, MascotSfx::kLand);
        return;
    }
    tell_crouch(motion, std::max(-lift, crouch));
}

/* A front-view move's body and props from one frame to the next: off the ground and back on it, a leg lifted, a hand
 * going up past the body's top quarter or slammed back down, a lean, sway or tilt setting in, a turn to a new view, the
 * flag up, the confetti and the dust as they burst, a glance. */
constexpr int32_t kSwayFrom = 3;     // px of sway at kMascotCell that is a lean, not a wobble: a quarter of a cell
constexpr int32_t kTiltFrom = 40;    // 0.1° of tilt that is a lean
constexpr int32_t kHandUpRow = 11;   // a hand's top row at or above this is raised; the rest pose's is 14
constexpr uint8_t kLookHeld = 3;     // ticks the eyes hold a look, still frames counted, before leaving it is a glance
int32_t side_of(int32_t v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }
void tell_front(Motion &motion, const PixelPose &was, const PixelPose &now)
{
    tell_lift(motion, now.lift, now.crouch);
    if (now.raised != was.raised && now.raised != Raised::kNone && now.raised != Raised::kBoth) {
        tell(&motion, MascotSfx::kStep);
    }
    /* A lean or sway setting in, and a rock back through the middle from one: once a side, the rock clearing it. The
     * sway is drawn in px at the mascot's own cell, so its threshold is the same share of a cell at any size — an
     * avatar's lean is heard as the page's is (AGENT_DECK 4.7). */
    const int32_t sway_from = std::max<int32_t>(1, kSwayFrom * motion.m.cell / theme::kMascotCell);
    if ((std::abs(now.sway) >= sway_from && std::abs(was.sway) < sway_from) ||
        (std::abs(now.lean) >= kTiltFrom && std::abs(was.lean) < kTiltFrom)) {
        tell(&motion, MascotSfx::kSway);
    } else if (motion.sway_side != 0 && (side_of(now.sway) == -motion.sway_side ||
                                         (now.sway == 0 && side_of(now.lean) == -motion.sway_side))) {
        tell(&motion, MascotSfx::kSway);
        motion.sway_side = 0;
    }
    if (std::abs(now.sway) >= sway_from) {
        motion.sway_side = static_cast<int8_t>(side_of(now.sway));
    } else if (std::abs(now.lean) >= kTiltFrom) {
        motion.sway_side = static_cast<int8_t>(side_of(now.lean));
    }
    /* The hands: one moment a frame, however many go up or down on it — two flapping together are one flap. */
    bool up = false;
    bool down = false;
    for (const auto &[a, b] : {std::pair{was.left_row, now.left_row}, std::pair{was.right_row, now.right_row}}) {
        up = up || (a > kHandUpRow && b <= kHandUpRow);
        down = down || (a <= kHandUpRow && b > kHandUpRow);
    }
    if (up) {
        tell(&motion, MascotSfx::kHand);
    } else if (down) {
        tell(&motion, MascotSfx::kHandDown);
    }
    if (now.view != was.view) {
        tell(&motion, MascotSfx::kTurn);
    }
    if (now.flag >= 0 && was.flag < 0) {
        tell(&motion, MascotSfx::kFlag);
    }
    for (size_t i = 0; i < now.bursts.size(); i++) {
        if (now.bursts[i] >= 0 && was.bursts[i] < 0) { /* -1 for none, else ms into the burst */
            tell(&motion, MascotSfx::kBurst);
        }
    }
    if (now.dust > 0 && was.dust == 0) {
        tell(&motion, MascotSfx::kPuff);
    }
    /* A glance: the eyes leaving a look they held — the in-betweens of a dart pass by untold — for a look aside. */
    if (now.look != was.look || now.look_y != was.look_y) {
        if (motion.look_still >= kLookHeld && (now.look != 0 || now.look_y != 0)) {
            tell(&motion, MascotSfx::kLook);
        }
        motion.look_still = 0;
    } else if (motion.look_still < kLookHeld) {
        motion.look_still++;
    }
}

Motion *motion_of(const Mascot &m)
{
    return m.root == nullptr ? nullptr : static_cast<Motion *>(lv_obj_get_user_data(m.root));
}

/* The pixels' rectangles go through here: drawn, or — while `measuring` points at a box, crop_pixels() working out what
 * a frame covers — only added to that box. */
struct Measure {
    bool any;
    lv_area_t box;
};
Measure *measuring = nullptr;

void put_rect(lv_layer_t *layer, const lv_draw_rect_dsc_t *dsc, const lv_area_t *a)
{
    if (measuring == nullptr) {
        lv_draw_rect(layer, dsc, a);
        return;
    }
    if (!measuring->any) {
        measuring->box = *a;
        measuring->any = true;
    } else {
        lv_area_t &box = measuring->box;
        box = {std::min(box.x1, a->x1), std::min(box.y1, a->y1), std::max(box.x2, a->x2), std::max(box.y2, a->y2)};
    }
}

/*
 * THE PIXEL STYLE (AGENT_DECK 9.2): a frame of half-cell pixels, its body where the rig's is — the GIF's
 * body at column 17, row 10 on the rig's body at kBody — drawn as runs, one rectangle a run, in one draw callback.
 */
constexpr int32_t kGifBodyCol = 17;
constexpr int32_t kGifBodyRow = 10;
constexpr int32_t kGifRightFootEdge = 33; // the column right of the rest pose's right foot (GIF columns 31–32)
static_assert((kGifBodyRow - detail::kPixelTop) * theme::kMascotCell / 2 <= theme::kMascotHeadroom,
              "a frame draws past the room above the mascot");

/* A canvas of the pixel style: the GIF cell at its top-left, and its size in pixels. */
struct Canvas {
    int32_t left, top, cols, rows;
};

/* The work's canvas: its frames, and the faceplant's, the same width now that draft 4's hand reaches no further. */
constexpr Canvas kWorkCanvas = {detail::kPixelLeft, detail::kPixelTop, detail::kFaceplantCols, detail::kPixelRows};
static_assert(detail::kFaceplantCols >= detail::kPixelCols, "the work's canvas holds its frames and the faceplant's");
/* THE FINISH TILTS ON SMALL CANVASES: LVGL draws a transformed object through a layer of its whole area, 4 bytes a
 * pixel, in PSRAM on the board (HARDWARE_SPECS 8.1) — one canvas holding the body and the flag overhead, 160 x 128 px,
 * would fill 82 KB a frame. So the body tilts on its own canvas, the raised hand's reach and the pop's stretch inside
 * it, and the flag on its own (Mascot::flag), carried to the hand — each a layer the size of the work's tilted canvas
 * or smaller, about the same pivot, his hips. */
constexpr Canvas kFinishCanvas = {11, 2, 28, 22};
/* The celebrate's: the hands up at the top of its jump, splayed wide on its impact, the body down in the squat. A layer
 * of it is at most a tenth bigger than the work's canvas — drawn only while it leans, during its arm punches. */
constexpr Canvas kCelebrateCanvas = {11, -2, 28, 28};
/* Its confetti, in the glass's frame, upright: the two bursts over his head, each side. */
constexpr Canvas kConfettiCanvas = {8, -8, 34, 20};
/* The dust kicked up at his feet, upright on the ground: a puff each side, out to its widest (kPuff). */
constexpr Canvas kDustCanvas = {8, 22, 34, 4};
/* How far the finish reaches past the grid, flag and tilt and all: the root's room to draw it in. */
constexpr Canvas kFinishReach = {9, -6, 40, 32};
/* The flag at its highest — the raised hand thrust up a pixel, the pop's stretch and lift carrying it up to 4 pixels
 * more — and out to its widest, inside that room with the body's canvas. */
static_assert(kFinishReach.left <= kFinishCanvas.left && kFinishReach.top <= kFinishCanvas.top &&
                  kFinishReach.left + kFinishReach.cols >= kFinishCanvas.left + kFinishCanvas.cols &&
                  kFinishReach.top + kFinishReach.rows >= kFinishCanvas.top + kFinishCanvas.rows,
              "the finish's room holds the body's canvas");
static_assert(kFinishReach.top <= kGripRow - 1 - detail::kFlagPoleAbove - 4 &&
                  kFinishReach.left + kFinishReach.cols >= kGripCol + detail::kFlagCols,
              "the finish's room holds the flag at its highest and widest");
/* Its legs: the four, a knee's bend and the squash's reach, from over the hips at the top of the celebrate's jump down
 * to the ground. */
constexpr Canvas kLegsCanvas = {13, 14, 24, 12};
constexpr int32_t kPivotCol = 25;  // the body's centre line (GIF columns 17–32)
constexpr int32_t kGroundRow = 26; // under the feet
constexpr int32_t kHipRow = 22;    // the body's bottom edge, where the legs start
constexpr int32_t kFinishLegs[4] = {17, 21, 27, 31};
/* THE SKIP TRIP'S CANVAS: the faceplant's frames — the work's canvas, kTripShift columns left so his turned body
 * stands over his feet, and going right mirrored about his centre line; never tilted. */
constexpr int32_t kTripShift = 5;
constexpr Canvas kTripCanvas[2] = {
    {detail::kPixelLeft - kTripShift, detail::kPixelTop, detail::kFaceplantCols, detail::kPixelRows},
    {2 * kPivotCol - detail::kPixelLeft - detail::kFaceplantCols + kTripShift, detail::kPixelTop,
     detail::kFaceplantCols, detail::kPixelRows},
};
static_assert(kTripCanvas[1].left == 12 && kTripCanvas[0].left == 4, "the trip's canvases, either way");
static_assert(kLegsCanvas.top + kLegsCanvas.rows == kGroundRow, "the legs' canvas stands on the ground");
static_assert(kFinishCanvas.cols * kFinishCanvas.rows <= kWorkCanvas.cols * kWorkCanvas.rows,
              "the finish's tilted canvas needs a bigger layer than the work's");
static_assert(kCelebrateCanvas.cols * kCelebrateCanvas.rows * 10 <= kWorkCanvas.cols * kWorkCanvas.rows * 11,
              "the celebrate's tilted canvas needs a layer more than a tenth bigger than the work's");
static_assert(kFinishReach.top <= kCelebrateCanvas.top && kFinishReach.top <= kConfettiCanvas.top + 2 &&
                  kFinishReach.left <= kConfettiCanvas.left + 2,
              "the root's room holds the celebrate and its confetti's drawn bits");
static_assert(kFinishReach.left - 4 <= kDustCanvas.left &&
                  kDustCanvas.left + kDustCanvas.cols <= kFinishReach.left + kFinishReach.cols &&
                  kDustCanvas.top + kDustCanvas.rows <= kFinishReach.top + kFinishReach.rows,
              "the root's room — the finish's reach and the two cells past it (root_ext_cb()) — holds the dust");

int32_t pixel(const Mascot &m) { return m.cell / 2; }
int32_t canvas_x(const Mascot &m, const Canvas &c) { return kBody.x * m.cell + (c.left - kGifBodyCol) * pixel(m); }
int32_t canvas_y(const Mascot &m, const Canvas &c) { return kBody.y * m.cell + (c.top - kGifBodyRow) * pixel(m); }

/* How far past the grid a canvas reaches. */
int32_t overhang(const Mascot &m, const Canvas &c)
{
    const int32_t right = canvas_x(m, c) + c.cols * pixel(m) - theme::kMascotCols * m.cell;
    const int32_t bottom = canvas_y(m, c) + c.rows * pixel(m) - theme::kMascotRows * m.cell;
    return std::max({m.cell, -canvas_x(m, c), -canvas_y(m, c), right, bottom});
}

bool pixel_style(MascotMove move)
{
    return move == MascotMove::kWork || move == MascotMove::kFaceplant || move == MascotMove::kFinish ||
           move == MascotMove::kCelebrate || move == MascotMove::kIdle || move == MascotMove::kIdleInPlace ||
           move == MascotMove::kWave || move == MascotMove::kHello || mascot_reaction(move);
}

/* A front-view pixel move: the body on its canvas, the legs planted on theirs. */
bool front_pixels(MascotMove move)
{
    return move == MascotMove::kFinish || move == MascotMove::kCelebrate || move == MascotMove::kIdle ||
           move == MascotMove::kIdleInPlace || move == MascotMove::kWave || move == MascotMove::kHello ||
           mascot_reaction(move);
}

/* The celebrate's canvas for the moves that fling the arms up at the top of a jump: the spin's and the dust jump's,
 * their turned views' tucked legs inside it too; neither leans in its own frames (upright()), so neither draws through
 * a layer while it plays — the idle after one leans on it, as after the celebrate. */
const Canvas &body_canvas(MascotMove move)
{
    return move == MascotMove::kCelebrate || move == MascotMove::kSpin || move == MascotMove::kDust ? kCelebrateCanvas
                                                                                                    : kFinishCanvas;
}

/* How far a move's travel takes the parts from the root, either way: the whole route off the glass and back, at the
 * mascot's cell — past the most a walk or a skip goes (on_route()). */
int32_t travel_reach(const Mascot &m) { return scale(kRouteOff + kRouteBack, m.cell, kFinishRefCell); }

/*
 * The root's room past its box: a cell for the rig's squash and stretch; the laptop's reach while the work plays; the
 * flag's overhead while the finish does, and two cells more for its tilt's corners — and, for the moves of the front
 * view, the whole route a walk or a skip travels, since the travel moves the parts inside a root that stays home
 * (set_travel()): LVGL draws a child only inside its parent's box and this room.
 *
 * THE ROOM NEVER MOVES A FRAME (the owner's ask, 2026-11-07, measured on the board): LVGL redraws the root's box and
 * the whole of this room each time the root moves — about the whole glass at the front view's room — so nothing moves
 * the root while a move plays: the walk's travel and the hop's lift move the parts, each redrawing only its own box.
 */
void root_ext_cb(lv_event_t *event)
{
    const auto *motion = static_cast<Motion *>(lv_event_get_user_data(event));
    const Mascot &m = motion->m;
    switch (motion->move) {
    case MascotMove::kWork:
    case MascotMove::kFaceplant: lv_event_set_ext_draw_size(event, overhang(m, kWorkCanvas)); return;
    case MascotMove::kFinish:
    case MascotMove::kCelebrate:
    case MascotMove::kIdle:
    case MascotMove::kWave:
    case MascotMove::kHello:
    case MascotMove::kSkip:
    case MascotMove::kSkipTrip:
    case MascotMove::kSpin:
    case MascotMove::kDust:
    case MascotMove::kNeedsYou:
        lv_event_set_ext_draw_size(event, overhang(m, kFinishReach) + 2 * m.cell + travel_reach(m));
        return;
    case MascotMove::kIdleInPlace: /* it never travels: the front view's room without the route */
        lv_event_set_ext_draw_size(event, overhang(m, kFinishReach) + 2 * m.cell);
        return;
    case MascotMove::kJump: /* the hop lifts the parts kMascotJumpRise, past the rig's cell: the room takes both, or the
                             * body's top is cut off in the air — tests/host/mascot_lvgl_test.cpp checks every part */
        lv_event_set_ext_draw_size(event, m.cell + theme::kMascotJumpRise * m.cell / theme::kMascotCell);
        return;
    case MascotMove::kRest: break;
    }
    lv_event_set_ext_draw_size(event, m.cell);
}

int32_t floor_div(int32_t a, int32_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

/* THE FLAG'S CHECKS ARE BIGGER THAN THE PIXEL — the developer's choice, 2026-10-29: three halves of the pixel, 12 px at
 * this board's 8 px, across the sheet's outline, so the pattern reads on the glass. */
int32_t flag_check(const Mascot &m) { return 3 * pixel(m) / 2; }

/* The checkered flag (Mascot::flag): its frame's outline on the mascot's own pixel, the pole a light grey, the cloth
 * cut into the checks it crosses, counted from the pole's foot. Rigid: the hand carries it (tick_cb()). */
void draw_flag_cb(lv_event_t *event)
{
    const auto *motion = static_cast<Motion *>(lv_event_get_user_data(event));
    if (motion->move != MascotMove::kFinish || motion->pose.flag < 0) {
        return;
    }
    const detail::FlagFrame &frame = detail::kFlagFrames[motion->pose.flag];
    lv_area_t box;
    lv_obj_get_coords(lv_event_get_current_target_obj(event), &box);
    lv_layer_t *layer = lv_event_get_layer(event);
    const int32_t p = pixel(motion->m);
    const int32_t k = flag_check(motion->m);
    const int32_t gx = box.x1 + frame.pole * p; /* the pole's foot */
    const int32_t gy = box.y1 + detail::kFlagPoleAbove * p;
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    const auto rect = [&](int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t colour) {
        const lv_area_t a = {x1, y1, x2, y2};
        dsc.bg_color = colour;
        lv_draw_rect(layer, &dsc, &a);
    };
    /* The cloth from (x0, y0) to (x1, y1), px from the pole's foot, cut into the checks it crosses. */
    const auto cloth = [&](int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
        for (int32_t y = y0; y < y1;) {
            const int32_t ye = std::min(y1, (floor_div(y, k) + 1) * k);
            for (int32_t x = x0; x < x1;) {
                const int32_t xe = std::min(x1, (floor_div(x, k) + 1) * k);
                const bool check = ((floor_div(x, k) + floor_div(y, k)) & 1) != 0;
                rect(gx + x, gy + y, gx + xe - 1, gy + ye - 1, check ? theme::mascot_flag_check() : theme::mascot_flag());
                x = xe;
            }
            y = ye;
        }
    };
    for (int32_t r = 0; r < detail::kFlagRows; r++) {
        const char *row = frame.rows[r];
        const int32_t y = (r - detail::kFlagPoleAbove) * p;
        for (int32_t c = 0; c < detail::kFlagCols;) {
            if (row[c] != 'x') {
                c++;
                continue;
            }
            if (c == frame.pole) {
                if (r < detail::kFlagPoleAbove) { /* the pole below is behind the hand */
                    rect(gx, gy + y, gx + p - 1, gy + y + p - 1, theme::mascot_flag_pole());
                }
                c++;
                continue;
            }
            int32_t end = c + 1;
            while (end < detail::kFlagCols && row[end] == 'x' && end != frame.pole) {
                end++;
            }
            cloth((c - frame.pole) * p, y, (end - frame.pole) * p, y + p);
            c = end;
        }
    }
}

/* Where the finish puts things, in px from its canvas's top-left: through the stretch about the feet, the sway and the
 * lift, as the work's runs are. */
struct FinishMap {
    int32_t p, pivot, ground;
    const Canvas &c;
    const PixelPose &f;
    int32_t x(int32_t px) const { return f.x + f.sway + pivot + scale(px - pivot, f.wide, 1000); }
    int32_t y(int32_t py) const { return ground - scale(ground - py, f.tall, 1000) - f.lift + f.crouch; }
    /* the pole's foot, on the stretched hand's top row: the flag's own pixels unstretched */
    int32_t grip_x() const { return x((f.grip_col - c.left) * p + p / 2) - p / 2; }
    int32_t grip_y() const { return y((f.grip_row + 1 - c.top) * p) - p; }
    int32_t hip_y() const { return ground - f.lift + f.crouch - scale((kGroundRow - kHipRow) * p, f.tall, 1000); }
};

FinishMap finish_map(const Motion &motion)
{
    const int32_t p = pixel(motion.m);
    const Canvas &c = body_canvas(motion.move);
    return {p, (kPivotCol - c.left) * p, (kGroundRow - c.top) * p, c, motion.pose};
}


/* THE SPIN'S TURN (AGENT_DECK 9.3, 9.6): the model sheet's three-quarter and side views, and two the
 * developer approved on the spin's draft, 2026-10-29 — the back three-quarter, its side in shade, and the back, no
 * eyes, the hands half in front of the body now, each wholly in the tint (the developer's ask, 2026-11-24, over their
 * outer halves in the body's colour); the legs tucked a pixel shorter.
 * GIF columns kTurnLeft on, rows kTurnTop on: `o` the body, `s` its shade, `l` its tint, `k` an eye. Views 5–7 are 3–1
 * mirrored about the body's centre line, round to the front. */
constexpr int32_t kTurnLeft = 15;
constexpr int32_t kTurnTop = 10;
constexpr int32_t kTurnCols = 20;
constexpr int32_t kTurnRows = 15;
constexpr const char *kTurnViews[4][kTurnRows] = {
    {// three-quarter, 25° to its left
     "..oooooooooooossss..", "..oooooooooooossss..", "..ookkooookkoossss..", "..ookkooookkoossss..",
     "oooooooooooooollll..", "oooooooooooooolllls.", "oooooooooooooolllls.", "oooooooooooooolllls.",
     "..oooooooooooossss..", "..oooooooooooossss..", "..oooooooooooossss..", "..oooooooooooossss..",
     "...oo..ss..oo..ss...", "...oo..ss..oo..ss...", "...oo..ss..oo..ss..."},
    {// side, 90° to its left
     ".....oooooooooo.....", ".....oooooooooo.....", ".....okkooooooo.....", ".....okkooooooo.....",
     ".....oooolllloo.....", ".....oooollllso.....", ".....oooollllso.....", ".....oooollllso.....",
     ".....oooooooooo.....", ".....oooooooooo.....", ".....oooooooooo.....", ".....oooooooooo.....",
     "......oo....oo......", "......oo....oo......", "......oo....oo......"},
    {// back three-quarter
     "..ssssoooooooooooo..", "..ssssoooooooooooo..", "..ssssoooooooooooo..", "..ssssoooooooooooo..",
     "..ssssoooooooooollll", "..ssssoooooooooollll", "..ssssoooooooooollll", "..ssssoooooooooollll",
     "..ssssoooooooooooo..", "..ssssoooooooooooo..", "..ssssoooooooooooo..", "..ssssoooooooooooo..",
     "....ss..oo..ss..oo..", "....ss..oo..ss..oo..", "....ss..oo..ss..oo.."},
    {// back
     "..oooooooooooooooo..", "..oooooooooooooooo..", "..oooooooooooooooo..", "..oooooooooooooooo..",
     "lllloooooooooooollll", "lllloooooooooooollll", "lllloooooooooooollll", "lllloooooooooooollll",
     "..oooooooooooooooo..", "..oooooooooooooooo..", "..oooooooooooooooo..", "..oooooooooooooooo..",
     "..oo..oo....oo..oo..", "..oo..oo....oo..oo..", "..oo..oo....oo..oo.."},
};
static_assert(kTurnLeft + kTurnCols / 2 == kPivotCol, "the turned views mirror about the body's centre line");
/* Turned, at the top of the spin — 16 px up, four pixels, and stretched — the views stay on the spin's canvas. */
static_assert(kTurnLeft >= kCelebrateCanvas.left && kTurnLeft + kTurnCols <= kCelebrateCanvas.left + kCelebrateCanvas.cols &&
                  kTurnTop - 5 >= kCelebrateCanvas.top && kTurnTop + kTurnRows <= kCelebrateCanvas.top + kCelebrateCanvas.rows,
              "the turned views, lifted and stretched, stay on the spin's canvas");

/* The dust a landing kicks up (AGENT_DECK 9.3): until `until` ms after it, these GIF cells at his left
 * foot, the same mirrored at his right — a dot at the feet, a wisp out, thinning up and away. */
struct Puff {
    int16_t until;
    bool faint;
    int8_t cells[4][2];
    int8_t count;
};
constexpr Puff kPuff[] = {
    {90, false, {{15, 25}, {14, 25}}, 2},
    {200, false, {{13, 25}, {12, 24}, {11, 24}, {10, 25}}, 4},
    {320, true, {{10, 24}, {9, 23}, {8, 23}}, 3},
};
static_assert(kPuff[std::size(kPuff) - 1].until == kDustPuffMs, "a puff's frames run its length");
constexpr bool puff_on_canvas()
{
    for (const Puff &p : kPuff) {
        for (int32_t i = 0; i < p.count; i++) {
            const int32_t c = p.cells[i][0];
            const int32_t r = p.cells[i][1];
            if (c < kDustCanvas.left || 2 * kPivotCol - 1 - c >= kDustCanvas.left + kDustCanvas.cols ||
                r < kDustCanvas.top || r >= kDustCanvas.top + kDustCanvas.rows) {
                return false;
            }
        }
    }
    return true;
}
static_assert(puff_on_canvas(), "every puff's pixel, and its mirror, lands on the dust's canvas");

/* A turned view (kTurnViews), view 5–7 mirrored, in runs of its pixels: `part` draws each run, from its GIF cell. */
template <typename Part> void draw_turned(const Motion &motion, const Part &part)
{
    const PixelPose &f = motion.pose;
    const bool mirrored = f.view > 4;
    const char *const *rows = kTurnViews[(mirrored ? 8 - f.view : f.view) - 1];
    for (int32_t r = 0; r < kTurnRows; r++) {
        int32_t i = 0;
        while (i < kTurnCols) {
            const char ch = rows[r][mirrored ? kTurnCols - 1 - i : i];
            int32_t n = 1;
            while (i + n < kTurnCols && rows[r][mirrored ? kTurnCols - 1 - (i + n) : i + n] == ch) {
                n++;
            }
            if (ch != '.') {
                part(kTurnLeft + i, kTurnTop + r, n, 1,
                     ch == 's'   ? motion.shade
                     : ch == 'l' ? motion.tint
                     : ch == 'k' ? theme::background()
                                 : motion.body);
            }
            i += n;
        }
    }
}

/* The front view's eyes, where they look: > <, ^ ^, shut or open; `part` draws each of their pixels' runs. */
template <typename Part> void draw_front_eyes(const Motion &motion, const Part &part)
{
    const PixelPose &f = motion.pose;
    for (const int32_t eye : {19, 29}) {
        const int32_t ex = eye + f.look;
        const int32_t ey = 12 + f.look_y;
        if (f.chevron) { /* > <: squeezed shut, each pointing in toward the other, 4 wide, 3 tall */
            const bool left = eye == 19;
            const int32_t tip = left ? ex + 1 : ex - 1;
            const int32_t back = left ? ex - 1 : ex + 1;
            part(back, ey, 2, 1, theme::background());
            part(tip, ey + 1, 2, 1, theme::background());
            part(back, ey + 2, 2, 1, theme::background());
        } else if (f.happy) { /* ^ */
            part(ex, ey, 2, 1, theme::background());
            part(ex - 1, ey + 1, 1, 1, theme::background());
            part(ex + 2, ey + 1, 1, 1, theme::background());
        } else if ((motion.blink && !f.stumble) || f.shut) { /* shut to its lower row; none in the trip's stumble */
            part(ex, ey + 1, 2, 1, theme::background());
        } else {
            part(ex, ey, 2, 2, theme::background());
        }
    }
}

/* The finish's or the celebrate's body (draw_pixels_cb()): its parts through the stretch, the pole's foot over the
 * raised hand last; the flag behind it and the confetti are their own canvases, and the tilt the canvases' transform
 * (tick_cb()). */
void draw_finish(const Motion &motion, lv_layer_t *layer, const lv_area_t &box)
{
    const PixelPose &f = motion.pose;
    const FinishMap map = finish_map(motion);
    const Canvas &canvas = map.c;
    const int32_t p = map.p;
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    const auto rect = [&](int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t colour) {
        const lv_area_t a = {box.x1 + x1, box.y1 + y1, box.x1 + x2, box.y1 + y2};
        dsc.bg_color = colour;
        put_rect(layer, &dsc, &a);
    };
    /* A part: `w` x `h` pixels from GIF cell (c, r), stretched with the body. */
    const auto part = [&](int32_t c, int32_t r, int32_t w, int32_t h, lv_color_t colour) {
        const int32_t x = (c - canvas.left) * p;
        const int32_t y = (r - canvas.top) * p;
        rect(map.x(x), map.y(y), map.x(x + w * p) - 1, map.y(y + h * p) - 1, colour);
    };
    const lv_color_t body = motion.body;
    if (f.view != 0) { /* turned: the view's own pixels, legs and all, in runs */
        draw_turned(motion, part);
        return;
    }
    /* A hand: its top two rows leading a swing by `shear`. */
    const auto hand = [&](int32_t c, int32_t r, int32_t shear) {
        part(c + shear, r, 4, 2, body);
        part(c, r + 2, 4, 2, body);
    };
    hand(f.left_col, f.left_row, f.left_shear); /* the left hand, half behind the body at rest */
    part(kGifBodyCol, kGifBodyRow, 16, 12, body);
    hand(f.right_col, f.right_row, f.right_shear);
    draw_front_eyes(motion, part);
    if (f.flag >= 0 && f.grip_over) { /* the pole over the raised hand's top row */
        rect(map.grip_x(), map.grip_y(), map.grip_x() + p - 1, map.grip_y() + p - 1, theme::mascot_flag_pole());
    }
}

/* THE FINISH'S AND THE CELEBRATE'S LEGS, planted while the body tilts above them: each from the tilted body's bottom
 * edge — its hips — down to the ground, or the air in a hop; shorter and straight in a squat; the legs he leans away
 * from, or both tucked in a jump, raised a pixel shorter, the knee bowed out half a pixel away from the body's centre,
 * a slight bend — the developer's asks, 2026-10-29. */
/* The legs' frame, their canvas at `box`. */
void draw_legs(const Motion *motion, lv_layer_t *layer, const lv_area_t &box)
{
    if (!motion->front || motion->pose.view != 0) { /* a turned view draws its own legs */
        return;
    }
    const PixelPose &f = motion->pose;
    const int32_t p = pixel(motion->m);
    const int32_t pivot = (kPivotCol - kLegsCanvas.left) * p;
    const int32_t hip_x = box.x1 + f.x + f.sway + pivot; /* the hips over the feet, swayed */
    const int32_t foot_x = box.x1 + f.x + pivot;
    const int32_t foot = box.y1 + (kGroundRow - kLegsCanvas.top) * p - f.lift;
    const int32_t hip_y = foot + f.crouch - scale((kGroundRow - kHipRow) * p, f.tall, 1000);
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.bg_color = motion->body;
    const auto rect = [&](int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
        const lv_area_t a = {x1, y1, x2, y2};
        put_rect(layer, &dsc, &a);
    };
    for (size_t i = 0; i < std::size(kFinishLegs); i++) {
        const auto bx = [&](int32_t col) {
            return hip_x + scale((col - kLegsCanvas.left) * p - pivot, f.wide, 1000);
        };
        const int32_t x0 = bx(kFinishLegs[i]);
        const int32_t x1 = bx(kFinishLegs[i] + 2);
        /* tan of the lean, small: 0.1° is 0.001745 rad */
        const int32_t top = hip_y + scale((x0 + x1) / 2 - hip_x, f.lean * 1745, 1000000);
        const int32_t len = foot - top;
        const bool left = i < std::size(kFinishLegs) / 2;
        const bool raised = f.raised == Raised::kBoth || (f.raised == Raised::kLeft && left) ||
                            (f.raised == Raised::kRight && !left);
        const int32_t out = left ? -1 : 1;
        const bool outer = i == 0 || i + 1 == std::size(kFinishLegs);
        /* Rows y0..y1 of the leg shifted `dx`; swayed, slanting a row at a time from over the hips to the foot. */
        const auto run = [&](int32_t y0, int32_t y1, int32_t dx) {
            if (f.sway == 0) {
                rect(x0 + dx, y0, x1 - 1 + dx, y1 - 1);
                return;
            }
            for (int32_t y = y0; y < y1; y++) {
                const int32_t slant = scale(foot_x - hip_x, y - top, std::max<int32_t>(len, 1));
                rect(x0 + dx + slant, y, x1 - 1 + dx + slant, y);
            }
        };
        if (raised) { /* a pixel shorter, the knee bent half a pixel — out, or the way it walks */
            const int32_t knee = (f.knee_dir != 0 ? f.knee_dir : out) * p / 2;
            run(top, top + len / 4, 0);
            run(top + len / 4, top + len / 2, knee);
            run(top + len / 2, top + 3 * len / 4, 0);
            continue;
        }
        /* the foot, the lower half, swung out in the air: the legs curve (the hop's) */
        const int32_t swing = f.swing == 0 ? 0 : out * (outer ? f.swing : f.swing / 2);
        run(top, top + len / 2, 0);
        run(top + len / 2, foot, swing);
    }
}

/*
 * THE CONFETTI (the celebrate's): the celebrate's contact sheet's bursts — bits of a pixel, two, or a diagonal pair in
 * orange, pink and blue (theme::mascot_confetti()) — popping up in kConfettiLaunchMs, then floating down the article's
 * curve over its 8 frames, spreading out and thinning, in the glass's frame: no lean, no squash. The article's rise,
 * −65 … 0 units, comes in at 55 %, its peak 8 pixels over the burst, so the bits stay inside the rings. Per bit: its
 * start (columns, rows from the burst), its drift outward, its shape, its colour and the frame it is gone at.
 */
struct ConfettiBit {
    int8_t col, row, drift;
    char shape; // 'o' a pixel, 'h' two across, 'v' two down, 'd' a diagonal pair
    uint8_t colour;
    uint8_t gone;
};

constexpr ConfettiBit kConfetti[] = {
    {-6, -1, -3, 'h', 0, 7}, {-3, -3, -1, 'd', 1, 5}, {-1, 0, -2, 'v', 2, 8}, {1, -4, 0, 'd', 0, 6},
    {3, -2, 1, 'h', 1, 8},   {5, 0, 2, 'd', 2, 4},    {7, -3, 3, 'v', 1, 6},  {-8, 1, -4, 'o', 2, 6},
    {2, 1, 1, 'o', 0, 7},    {8, 0, 4, 'h', 2, 7},    {-4, 2, -2, 'd', 0, 5}, {0, -6, 0, 'o', 1, 8},
};
constexpr int32_t kConfettiRise[8] = {-6765, -7480, -7865, -7260, -5995, -4345, -2310, 0}; // milli-pixels
constexpr int32_t kConfettiLaunchMs = 90;
constexpr int32_t kBurstCol = 30; // the first burst over his right side; the second mirrored about the centre line
constexpr int32_t kBurstRow = 8;

/* The dust (Mascot::dust): the puff's frame for how long ago it was kicked up, on the ground at both feet — under
 * him as he lands, his lift its own. */
void draw_dust_cb(lv_event_t *event)
{
    const auto *motion = static_cast<Motion *>(lv_event_get_user_data(event));
    const int32_t into = motion->pose.dust - 1;
    if ((motion->move != MascotMove::kSpin && motion->move != MascotMove::kDust) || into < 0) {
        return;
    }
    const Puff *puff = nullptr;
    for (const Puff &p : kPuff) {
        if (into < p.until) {
            puff = &p;
            break;
        }
    }
    if (puff == nullptr) {
        return;
    }
    lv_area_t box;
    lv_obj_get_coords(lv_event_get_current_target_obj(event), &box);
    lv_layer_t *layer = lv_event_get_layer(event);
    const int32_t p = pixel(motion->m);
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.bg_color = puff->faint ? theme::mascot_dust_faint() : theme::mascot_dust();
    for (int32_t i = 0; i < puff->count; i++) {
        for (const int32_t col : {int32_t{puff->cells[i][0]}, 2 * kPivotCol - 1 - puff->cells[i][0]}) {
            const int32_t x = box.x1 + (col - kDustCanvas.left) * p;
            const int32_t y = box.y1 + (puff->cells[i][1] - kDustCanvas.top) * p;
            const lv_area_t a = {x, y, x + p - 1, y + p - 1};
            lv_draw_rect(layer, &dsc, &a);
        }
    }
}

void draw_confetti_cb(lv_event_t *event)
{
    const auto *motion = static_cast<Motion *>(lv_event_get_user_data(event));
    if (motion->move != MascotMove::kCelebrate) {
        return;
    }
    lv_area_t box;
    lv_obj_get_coords(lv_event_get_current_target_obj(event), &box);
    lv_layer_t *layer = lv_event_get_layer(event);
    const int32_t p = pixel(motion->m);
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    const auto cube = [](int64_t v) { return static_cast<int32_t>(v * v / 1000 * v / 1000); };
    const auto round_px = [](int32_t milli) { return floor_div(milli + 500, 1000); };
    for (size_t b = 0; b < motion->pose.bursts.size(); b++) {
        const int32_t into = motion->pose.bursts[b];
        if (into < 0) {
            continue;
        }
        int32_t frame = 0;
        int32_t rise = 0;   // milli-pixels
        int32_t spread = 0; // per mille of each bit's drift
        if (into < kConfettiLaunchMs) {
            const int32_t x = into * 1000 / kConfettiLaunchMs;
            rise = scale(kConfettiRise[0], 1000 - cube(1000 - x), 1000);
            spread = 150 * x / 1000;
        } else {
            const int32_t u = (into - kConfettiLaunchMs) * 7000 / (kBurstMs - kConfettiLaunchMs); // frames, per mille
            frame = std::min<int32_t>(u / 1000, 7);
            const int32_t next = kConfettiRise[std::min<int32_t>(frame + 1, 7)];
            rise = kConfettiRise[frame] + scale(next - kConfettiRise[frame], u % 1000, 1000);
            spread = 150 + 850 * u / 7000;
        }
        for (const ConfettiBit &bit : kConfetti) {
            if (frame >= bit.gone) {
                continue;
            }
            const int32_t c = (bit.col * 1000 + bit.drift * spread) * 7 / 10;
            const int32_t col = b == 0 ? round_px(kBurstCol * 1000 + c) : round_px((2 * kPivotCol - 1 - kBurstCol) * 1000 - c);
            const int32_t row = round_px((kBurstRow + bit.row) * 1000 + scale(rise, 1000 + bit.row * 40, 1000));
            dsc.bg_color = theme::mascot_confetti(bit.colour);
            const auto dot = [&](int32_t dc, int32_t dr) {
                const int32_t x = box.x1 + (col + dc - kConfettiCanvas.left) * p;
                const int32_t y = box.y1 + (row + dr - kConfettiCanvas.top) * p;
                const lv_area_t a = {x, y, x + p - 1, y + p - 1};
                lv_draw_rect(layer, &dsc, &a);
            };
            dot(0, 0);
            if (bit.shape == 'h' || bit.shape == 'd') {
                dot(1, bit.shape == 'd' ? -1 : 0);
            } else if (bit.shape == 'v') {
                dot(0, 1);
            }
        }
    }
}

/*
 * THE SWEAT (AGENT_DECK 9.3), over the work's frame on its own canvas, carried by its squash: the wipe's hand lit,
 * with its shadow where it is over him; beads by his right eye, on 2 px steps, finer than the pixel so a teardrop
 * reads; and the beads bursting like a bubble, a ring of tiny bits springing out and thinning to nothing, a little
 * uneven so it is no circle, half of them gone first — the frame's eyes drawn again over it, so a bit is never over an
 * eye, whichever pose a stopped burst lands on. In px at a 4 px pixel, from a bead's foot; the ring's in per mille of
 * its reach.
 */
struct SweatRect {
    int8_t x, y, w, h;
};

constexpr SweatRect kSweatRects[] = {
    {-2, -4, 4, 4},                                                                     // 0: a bit
    {-1, -8, 2, 2}, {-2, -6, 4, 6},                                                     // 1: thin
    {-1, -12, 2, 2}, {-2, -10, 4, 2}, {-3, -8, 6, 2}, {-4, -6, 8, 4}, {-3, -2, 6, 2}, // 2: a teardrop
};
constexpr uint8_t kSweatSizes[3][2] = {{0, 1}, {1, 3}, {3, 8}}; // each size's rects, first to last
constexpr int16_t kSweatRing[8][2] = {{1000, 0},    {700, -760}, {-60, -1000}, {-740, -680},
                                      {-1000, 60}, {-690, 720}, {80, 900},    {760, 640}};
constexpr int32_t kBigBeadCol = 24;    // the big bead's column, past his right eye (kWorkPoses's)
constexpr int32_t kLittleBeadCol = 22; // the little one's, up and in from it: offset, not in line
constexpr int32_t kLittleBeadFoot = 9;

/* The beads bursting (draw_sweat()), `fill` drawing in dsc's colour: a ring of bits round each bead there is, then the
 * frame's eyes drawn again over it — each eye pixel but a blink's lid. */
template <typename X, typename Y, typename Fill>
void draw_sweat_pop(const Motion &motion, const detail::PixelFrame &f, bool blink, lv_draw_rect_dsc_t &dsc, const X &bx,
                    const Y &by, const Fill &fill)
{
    const SweatView &s = motion.sweat;
    const int32_t p = pixel(motion.m);
    const auto at = [&](int32_t px) { return scale(px, p, 4); };
    const int32_t into = s.pop * 1000 / kSweatPopMs;
    const int32_t reach = 5000 + 13 * ease(Ease::kOut, into); // milli-px
    const int32_t bit = std::max<int32_t>(at(into < 450 ? 3 : into < 800 ? 2 : 1), 1);
    const auto ring = [&](int32_t col, int32_t foot, int32_t up, int32_t tenths) {
        const int32_t cx = bx(col * p);
        const int32_t cy = by(foot * p) - at(up);
        for (size_t i = 0; i < std::size(kSweatRing); i++) {
            if (into > 600 && i % 2 != 0) {
                continue;
            }
            const int32_t x = cx + at(scale(kSweatRing[i][0], reach * tenths / 10, 1000000)) - bit / 2;
            const int32_t y = cy + at(scale(kSweatRing[i][1], reach * tenths / 10, 1000000)) - bit / 2;
            fill(x, y, x + bit - 1, y + bit - 1);
        }
    };
    if (s.big >= 0) {
        ring(kBigBeadCol, s.foot, 6, 10);
    }
    if (s.little >= 0) {
        ring(kLittleBeadCol, kLittleBeadFoot, 4, 8);
    }
    dsc.bg_color = theme::background();
    for (int32_t r = 0; r + 1 < detail::kPixelRows; r++) {
        for (int32_t c = 0; c < detail::kPixelCols; c++) {
            const bool lid = blink && f.rows[r + 1][c] == 'k';
            if (f.rows[r][c] == 'k' && !lid) {
                fill(bx(c * p), by(r * p), bx((c + 1) * p) - 1, by((r + 1) * p) - 1);
            }
        }
    }
}

template <typename X, typename Y>
void draw_sweat(const Motion &motion, const detail::PixelFrame &f, bool blink, lv_layer_t *layer, const X &bx,
                const Y &by)
{
    const SweatView &s = motion.sweat;
    const int32_t p = pixel(motion.m);
    const int32_t grain = std::max<int32_t>(p / 2, 1);
    const auto at = [&](int32_t px) { return scale(px, p, 4); };
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    const auto fill = [&](int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
        const lv_area_t a = {x1, y1, x2, y2};
        put_rect(layer, &dsc, &a);
    };
    if (s.hand_col >= 0) {
        const int32_t c = s.hand_col;
        dsc.bg_color = motion.shade;
        for (int32_t r = s.hand_row + 1; r < s.hand_row + 4; r++) {
            const char under = f.rows[r][c + 4];
            if (under != '.' && under != 'g' && under != 'k') {
                fill(bx((c + 4) * p), by(r * p), bx((c + 5) * p) - 1, by((r + 1) * p) - 1);
            }
        }
        dsc.bg_color = motion.tint;
        fill(bx(c * p), by(s.hand_row * p), bx((c + 4) * p) - 1, by((s.hand_row + 4) * p) - 1);
    }
    dsc.bg_color = theme::mascot_sweat();
    const auto bead = [&](int32_t col, int32_t foot, int32_t size) {
        const int32_t x = bx(col * p) / grain * grain;
        const int32_t y = by(foot * p) / grain * grain;
        for (int i = kSweatSizes[size][0]; i < kSweatSizes[size][1]; i++) {
            const SweatRect &r = kSweatRects[i];
            fill(x + at(r.x), y + at(r.y), x + at(r.x + r.w) - 1, y + at(r.y + r.h) - 1);
        }
    };
    if (s.pop >= 0) {
        draw_sweat_pop(motion, f, blink, dsc, bx, by, fill);
        return;
    }
    if (s.big >= 0) {
        bead(kBigBeadCol, s.foot, s.big);
    }
    if (s.little >= 0) {
        bead(kLittleBeadCol, kLittleBeadFoot, s.little);
    }
}

/* The faceplant's canvas's px `x1` to `x2` (exclusive) on the glass's, into `a`: as they are, or mirrored across the
 * canvas. */
void faceplant_across(const Motion &motion, const lv_area_t &box, int32_t x1, int32_t x2, lv_area_t &a)
{
    const int32_t span = detail::kFaceplantCols * pixel(motion.m);
    a.x1 = box.x1 + (motion.fp_mirror ? span - x2 : x1);
    a.x2 = box.x1 + (motion.fp_mirror ? span - x1 : x2) - 1;
}

/* The faceplant's hand in front in his colour, lit where it is over him — his body, his shade or the hand behind — at
 * the two lifts: each of its pixels over one of his rows takes the tint, as the draft's layers draw it. */
void draw_faceplant_front_hand(const Motion &motion, lv_layer_t *layer, const lv_area_t &box, lv_draw_rect_dsc_t &dsc,
                               int32_t sway)
{
    const int32_t p = pixel(motion.m);
    const WorkPose &w = motion.work;
    const detail::PixelFrame &hands = detail::kFaceplantHands[motion.fp_hand];
    const detail::PixelFrame &body = detail::kFaceplantPoses[motion.fp_pose];
    const int32_t hand_lift = motion.fp_hand_lift;
    const auto under = [&](int32_t c, int32_t y) {
        const int32_t rb = floor_div(y + w.lift, p);
        const int32_t rh = floor_div(y + hand_lift, p);
        const char b = rb >= 0 && rb < detail::kPixelRows ? body.rows[rb][c] : '.';
        const char h = rh >= 0 && rh < detail::kPixelRows ? hands.rows[rh][c] : '.';
        return b == 'o' || b == 's' || h == 'f';
    };
    for (int32_t r = 0; r < detail::kPixelRows; r++) {
        for (int32_t c = 0; c < detail::kFaceplantCols; c++) {
            if (hands.rows[r][c] != 'h') {
                continue;
            }
            const int32_t top = r * p - hand_lift;
            for (int32_t y = top; y < top + p;) { /* in slices, each over one of his rows or none */
                const int32_t step_end = std::min(top + p, (floor_div(y + w.lift, p) + 1) * p - w.lift);
                dsc.bg_color = under(c, y) ? motion.tint : motion.body;
                lv_area_t run = {0, box.y1 + y, 0, box.y1 + step_end - 1};
                faceplant_across(motion, box, c * p, (c + 1) * p, run);
                run.x1 += sway;
                run.x2 += sway;
                put_rect(layer, &dsc, &run);
                y = step_end;
            }
        }
    }
}

/* A FACEPLANT FRAME, as the draft draws its layers: the hand behind him at the hands' lift, the body — its legs and
 * eyes — at its own, the hand in front at the hands' lift; the laptop he lands on, the typing pose's, at its bounce and
 * never in the head shake's sway; and the stress lines, upright on the glass. No blink: the frames' eyes are the
 * draft's own. The skip trip's (fp_laptop false, fp_mirror going right): no laptop, and going right every column of
 * the canvas mirrored — the frames, the sway and the stress lines — so he faces the way he skips. */
void draw_faceplant(const Motion &motion, lv_layer_t *layer, const lv_area_t &box)
{
    const int32_t p = pixel(motion.m);
    const WorkPose &w = motion.work;
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    const auto across = [&](int32_t x1, int32_t x2, lv_area_t &a) { faceplant_across(motion, box, x1, x2, a); };
    const int32_t sway = motion.fp_mirror ? -w.x : w.x;
    /* Each run of a frame's rows of chars `pick` passes, at `x` and lifted `lift`. */
    const auto runs = [&](const detail::PixelFrame &f, int32_t cols, int32_t x, int32_t lift, auto pick) {
        for (int32_t r = 0; r < detail::kPixelRows; r++) {
            const char *row = f.rows[r];
            for (int32_t c = 0; c < cols;) {
                const char ch = row[c];
                int32_t end = c + 1;
                while (end < cols && row[end] == ch) {
                    end++;
                }
                if (pick(ch)) {
                    dsc.bg_color = ch == 's'   ? motion.shade
                                   : ch == 'l' ? motion.tint
                                   : ch == 'k' ? theme::background()
                                   : ch == 'g' ? theme::mascot_laptop()
                                               : motion.body;
                    lv_area_t run = {0, box.y1 + r * p - lift, 0, box.y1 + (r + 1) * p - 1 - lift};
                    across(c * p, end * p, run);
                    run.x1 += x;
                    run.x2 += x;
                    put_rect(layer, &dsc, &run);
                }
                c = end;
            }
        }
    };
    const detail::PixelFrame &hands = detail::kFaceplantHands[motion.fp_hand];
    const detail::PixelFrame &body = detail::kFaceplantPoses[motion.fp_pose];
    const int32_t cols = detail::kFaceplantCols;
    const int32_t hand_lift = motion.fp_hand_lift;
    runs(hands, cols, sway, hand_lift, [](char ch) { return ch == 'f'; });
    runs(body, cols, sway, w.lift, [](char ch) { return ch != '.'; });
    runs(hands, cols, sway, hand_lift, [](char ch) { return ch == 'l' || ch == 's'; });
    draw_faceplant_front_hand(motion, layer, box, dsc, sway);
    if (motion.fp_laptop) {
        runs(detail::kWorkPoses[detail::kFaceplantLaptopPose], detail::kPixelCols, 0, w.laptop,
             [](char ch) { return ch == 'g'; });
    }
    dsc.bg_color = theme::mascot_sweat();
    const auto at = [&](int32_t px) { return scale(px, motion.m.cell, theme::kMascotCell); };
    for (const StressRect &l : motion.stress.lines) {
        if (l.h > 0) {
            lv_area_t a = {0, box.y1 + at(l.y), 0, box.y1 + at(l.y + l.h) - 1};
            across(at(l.x), at(l.x + l.w), a);
            put_rect(layer, &dsc, &a);
        }
    }
}

/* Row `r` of the work's frame `f` in runs, through the physics `bx` and `by` (draw_pixels()): on the floor, the
 * laptop's runs in pass 1 and his in pass 0, the laptop only bouncing; else every run in pass 1. */
template <typename X, typename Y>
void draw_work_row(const Motion &motion, const detail::PixelFrame &f, int32_t r, int pass, bool on_floor, bool blink,
                   lv_layer_t *layer, const lv_area_t &box, lv_draw_rect_dsc_t &dsc, const X &bx, const Y &by)
{
    const int32_t p = pixel(motion.m);
    const WorkPose &w = motion.work;
    const char *row = f.rows[r];
    for (int32_t c = 0; c < detail::kPixelCols;) {
        const char ch = row[c];
        int32_t end = c + 1;
        while (end < detail::kPixelCols && row[end] == ch) {
            end++;
        }
        /* A blink: an eye's upper row — an eye pixel with one under it — is the body's, the eye shut to a line. */
        const bool lid = blink && ch == 'k' && r + 1 < detail::kPixelRows && f.rows[r + 1][c] == 'k';
        if (ch != '.' && (!on_floor || (ch == 'g') == (pass == 1))) {
            dsc.bg_color = lid         ? motion.body
                           : ch == 's' ? motion.shade
                           : ch == 'l' ? motion.tint
                           : ch == 'k' ? theme::background()
                           : ch == 'g' ? theme::mascot_laptop()
                                       : motion.body;
            lv_area_t run;
            if (ch == 'g' && on_floor) {
                run = {box.x1 + w.x + c * p, box.y1 + r * p - w.laptop, box.x1 + w.x + end * p - 1,
                       box.y1 + (r + 1) * p - 1 - w.laptop};
            } else {
                run = {bx(c * p), by(r * p), bx(end * p) - 1, by((r + 1) * p) - 1};
            }
            put_rect(layer, &dsc, &run);
        }
        c = end;
    }
}

/* The pixels' frame, its canvas at `box`. */
void draw_pixels(const Motion *motion, lv_layer_t *layer, const lv_area_t &box)
{
    if (motion->fp_pose >= 0) { /* the work's faceplant, or the skip trip's in the front view's place */
        draw_faceplant(*motion, layer, box);
        return;
    }
    if (motion->front) {
        draw_finish(*motion, layer, box);
        return;
    }
    if (motion->frame < 0) {
        return;
    }
    const detail::PixelFrame &f = detail::kWorkPoses[motion->frame];
    const int32_t p = pixel(motion->m);
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    /* The physics, about the feet: the body's centre column — drawn kTurnedShift right from the turn on — and the
     * ground. Both edges of every run go through the same mapping, so neighbouring runs still meet; a laptop on the
     * floor only bounces; the hop across moves every run alike. The tilt is the canvas's transform (tick_cb()). */
    const WorkPose &w = motion->work;
    const int32_t shift = motion->frame >= detail::kFirstTurnedPose ? detail::kTurnedShift : 0;
    const int32_t pivot = (kGifBodyCol + 8 + shift - detail::kPixelLeft) * p;
    const int32_t ground = detail::kPixelRows * p;
    const bool on_floor = motion->frame >= detail::kLaptopOnFloorFirst && motion->frame <= detail::kLaptopOnFloorLast;
    const bool blink =
        motion->blink && !(motion->frame >= detail::kFirstWinkPose && motion->frame <= detail::kLastWinkPose);
    const auto bx = [&](int32_t x) { return box.x1 + w.x + pivot + scale(x - pivot, w.wide, 1000); };
    const auto by = [&](int32_t y) { return box.y1 + ground - scale(ground - y, w.tall, 1000) - w.lift; };
    /* A laptop on the floor draws last, over him: squashed, his rows no longer meet its, and it would comb his edge. */
    for (int pass = on_floor ? 0 : 1; pass < 2; pass++) {
        for (int32_t r = 0; r < detail::kPixelRows; r++) {
            draw_work_row(*motion, f, r, pass, on_floor, blink, layer, box, dsc, bx, by);
        }
    }
    draw_sweat(*motion, f, blink, layer, bx, by);
}

/* The pixels' object covers only its crop of the canvas (crop_pixels()): the frame is drawn on the whole canvas, at the
 * object's corner less the crop's offset, LVGL clipping it to the object as it clipped it to the canvas. */
void draw_pixels_cb(lv_event_t *event)
{
    const auto *motion = static_cast<Motion *>(lv_event_get_user_data(event));
    lv_area_t box;
    lv_obj_get_coords(lv_event_get_current_target_obj(event), &box);
    box.x1 -= motion->crop.x;
    box.y1 -= motion->crop.y;
    box.x2 = box.x1 + lv_area_get_width(&motion->canvas) - 1;
    box.y2 = box.y1 + lv_area_get_height(&motion->canvas) - 1;
    draw_pixels(motion, lv_event_get_layer(event), box);
}

/* The tilt's pivot, px from the canvas's corner: on the object, less the crop's offset. */
void set_pixels_pivot(Motion *motion, int32_t x, int32_t y)
{
    lv_obj_t *pixels = motion->m.pixels;
    motion->pivot = {x, y};
    const int32_t px = x - motion->crop.x;
    const int32_t py = y - motion->crop.y;
    if (lv_obj_get_style_transform_pivot_x(pixels, LV_PART_MAIN) != px) {
        lv_obj_set_style_transform_pivot_x(pixels, px, LV_PART_MAIN);
    }
    if (lv_obj_get_style_transform_pivot_y(pixels, LV_PART_MAIN) != py) {
        lv_obj_set_style_transform_pivot_y(pixels, py, LV_PART_MAIN);
    }
}

/*
 * THE PIXELS' OBJECT IS CROPPED TO WHAT THE FRAME DRAWS (measured on the board, 2026-11-07): a tilt draws the object
 * through LVGL's transform — a layer of its area, every pixel of it sampled and blended, transparent or not — which
 * cost about 1 ms per 1,500 px of canvas, two thirds of a walking frame. So each frame the object covers only the
 * frame's rectangles' bounding box inside the canvas, the pivot moved with it to the same point on the glass: the same
 * pixels drawn, the same tilt about the same point, LVGL's transform and redraw only over the figure. Called as each
 * frame's state is set, before it is drawn.
 */
void crop_pixels(Motion *motion)
{
    const Mascot &m = motion->m;
    const lv_area_t &c = motion->canvas;
    if (lv_area_get_width(&c) <= 0 || lv_obj_is_hidden(m.pixels)) {
        return;
    }
    Measure measure = {false, {}};
    measuring = &measure;
    draw_pixels(motion, nullptr, c);
    measuring = nullptr;
    const lv_area_t &d = measure.box;
    lv_area_t keep = {std::max(d.x1, c.x1), std::max(d.y1, c.y1), std::min(d.x2, c.x2), std::min(d.y2, c.y2)};
    if (!measure.any || keep.x2 < keep.x1 || keep.y2 < keep.y1) {
        keep = {c.x1, c.y1, c.x1, c.y1}; /* nothing drawn: a pixel's object, drawing nothing */
    }
    motion->crop = {keep.x1 - c.x1, keep.y1 - c.y1};
    lv_obj_set_pos(m.pixels, keep.x1, keep.y1);
    lv_obj_set_size(m.pixels, lv_area_get_width(&keep), lv_area_get_height(&keep));
    set_pixels_pivot(motion, motion->pivot.x, motion->pivot.y);
}

/* The legs' canvas, kLegsCanvas, px from the root's corner. */
lv_area_t legs_canvas(const Mascot &m)
{
    const int32_t x = canvas_x(m, kLegsCanvas);
    const int32_t y = canvas_y(m, kLegsCanvas);
    return {x, y, x + kLegsCanvas.cols * pixel(m) - 1, y + kLegsCanvas.rows * pixel(m) - 1};
}

/* The legs' object covers only its crop of their canvas (crop_legs()), as the pixels' does. */
void draw_legs_cb(lv_event_t *event)
{
    const auto *motion = static_cast<Motion *>(lv_event_get_user_data(event));
    const lv_area_t c = legs_canvas(motion->m);
    lv_area_t box;
    lv_obj_get_coords(lv_event_get_current_target_obj(event), &box);
    box.x1 -= motion->legs_crop.x;
    box.y1 -= motion->legs_crop.y;
    box.x2 = box.x1 + lv_area_get_width(&c) - 1;
    box.y2 = box.y1 + lv_area_get_height(&c) - 1;
    draw_legs(motion, lv_event_get_layer(event), box);
}

/* The legs' object, cropped to what the frame draws of them, as the pixels' (crop_pixels()): a walking frame redraws
 * the planted legs, not the canvas's empty rows. No transform: the same pixels exactly. */
void crop_legs(Motion *motion)
{
    const Mascot &m = motion->m;
    if (lv_obj_is_hidden(m.pixel_legs)) {
        return;
    }
    const lv_area_t c = legs_canvas(m);
    Measure measure = {false, {}};
    measuring = &measure;
    draw_legs(motion, nullptr, c);
    measuring = nullptr;
    const lv_area_t &d = measure.box;
    lv_area_t keep = {std::max(d.x1, c.x1), std::max(d.y1, c.y1), std::min(d.x2, c.x2), std::min(d.y2, c.y2)};
    if (!measure.any || keep.x2 < keep.x1 || keep.y2 < keep.y1) {
        keep = {c.x1, c.y1, c.x1, c.y1}; /* nothing drawn: a pixel's object, drawing nothing */
    }
    motion->legs_crop = {keep.x1 - c.x1, keep.y1 - c.y1};
    lv_obj_set_pos(m.pixel_legs, keep.x1, keep.y1);
    lv_obj_set_size(m.pixel_legs, lv_area_get_width(&keep), lv_area_get_height(&keep));
}

/* The working loop's pose at `t` ms into it: the sequence's steps, each its hold of the GIF's frames. */
int work_frame_at(uint32_t t)
{
    uint32_t gif = 0;
    for (const detail::PixelStep &step : detail::kWorkSequence) {
        gif += step.hold;
        if (t < gif * theme::kMascotWorkMs / detail::kWorkGifFrames) {
            return step.pose;
        }
    }
    return 0;
}

constexpr uint32_t total_holds()
{
    uint32_t n = 0;
    for (const detail::PixelStep &step : detail::kWorkSequence) {
        n += step.hold;
    }
    return n;
}
static_assert(total_holds() == detail::kWorkGifFrames, "the sequence's holds add up to the GIF's frames");

constexpr bool poses_in_range()
{
    for (const detail::PixelStep &step : detail::kWorkSequence) {
        if (step.pose >= sizeof(detail::kWorkPoses) / sizeof(detail::kWorkPoses[0])) {
            return false;
        }
    }
    return true;
}
static_assert(poses_in_range(), "the sequence names a pose there is not");

/* The travel: every part `travel` px across the glass from home, the root staying put (root_ext_cb()) — each part that
 * moves redraws only its own box, old and new, and a hidden one nothing. */
void set_travel(const Mascot &m, int32_t travel)
{
    const uint32_t n = lv_obj_get_child_count(m.root);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *part = lv_obj_get_child(m.root, static_cast<int32_t>(i));
        if (lv_obj_get_style_translate_x(part, LV_PART_MAIN) != travel) {
            lv_obj_set_style_translate_x(part, travel, LV_PART_MAIN);
        }
    }
}

/* The rig on view for `move`, or the pixel frames in its place — the work's canvas, or the finish's and its legs'. */
void show(const Mascot &m, MascotMove move)
{
    const bool rig = !pixel_style(move);
    set_travel(m, 0); /* home: a move begun mid-walk starts where it stands */
    lv_obj_set_hidden(m.body, !rig);
    for (size_t i = 0; i < m.legs.size(); i++) {
        lv_obj_set_hidden(m.legs[i], !rig);
        lv_obj_set_hidden(m.feet[i], !rig);
    }
    lv_obj_set_hidden(m.pixels, rig);
    lv_obj_set_hidden(m.pixel_legs, !front_pixels(move));
    lv_obj_set_hidden(m.confetti, move != MascotMove::kCelebrate);
    lv_obj_set_hidden(m.dust, move != MascotMove::kSpin && move != MascotMove::kDust);
    lv_obj_set_hidden(m.flag, true); /* the finish shows it as its pose has it (tick_cb()) */
    lv_obj_set_style_transform_rotation(m.flag, 0, LV_PART_MAIN);
    const Canvas &c = front_pixels(move) ? body_canvas(move) : kWorkCanvas;
    lv_obj_set_pos(m.pixels, canvas_x(m, c), canvas_y(m, c));
    lv_obj_set_size(m.pixels, c.cols * pixel(m), c.rows * pixel(m));
    /* The work tilts about the right foot's outer edge on the ground; the finish about the hips (tick_cb()). */
    lv_obj_set_style_transform_pivot_x(m.pixels, (kGifRightFootEdge - kWorkCanvas.left) * pixel(m), LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(m.pixels, kWorkCanvas.rows * pixel(m), LV_PART_MAIN);
    /* The whole canvas, uncropped until the move's first frame is worked out (crop_pixels()). */
    if (Motion *motion = motion_of(m)) {
        motion->canvas = {canvas_x(m, c), canvas_y(m, c), canvas_x(m, c) + c.cols * pixel(m) - 1,
                          canvas_y(m, c) + c.rows * pixel(m) - 1};
        motion->crop = {0, 0};
        motion->pivot = {(kGifRightFootEdge - kWorkCanvas.left) * pixel(m), kWorkCanvas.rows * pixel(m)};
    }
}

/* The pose, in px: lifted `lift`, the legs `legs` long, the body `tall` taller and `wide` wider each side — the eyes
 * held at the centre, the hands and the outer legs carried out with its sides — the hands `hands` below its top, the
 * feet swung out `bend` (the outer legs; the inner half as far), each foot the leg's lower half. The lift raises the
 * parts, not the root, so a hop redraws the rig's own boxes and never the root's room (root_ext_cb()). */
void pose(const Mascot &m, int32_t lift, int32_t legs, int32_t tall, int32_t wide, int32_t hands, int32_t bend)
{
    const int32_t c = m.cell;
    lv_obj_set_pos(m.body, kBody.x * c - wide, kBody.y * c - lift);
    lv_obj_set_size(m.body, kBody.w * c + 2 * wide, kBody.h * c + tall);
    for (size_t i = 0; i < m.eyes.size(); i++) {
        lv_obj_set_x(m.eyes[i], (kEyes[i].x - kBody.x) * c + wide);
    }
    lv_obj_set_pos(m.hands[0], (kHands[0].x - kBody.x) * c, hands);
    lv_obj_set_pos(m.hands[1], (kHands[1].x - kBody.x) * c + 2 * wide, hands);
    const int32_t foot = legs / 2;
    const int32_t top = (kBody.y + kBody.h) * c + tall - lift;
    for (size_t i = 0; i < m.legs.size(); i++) {
        const bool outer = i == 0 || i + 1 == m.legs.size();
        const int32_t side = i < m.legs.size() / 2 ? -1 : 1;
        const int32_t x = kLegs[i].x * c + (outer ? side * wide : 0);
        lv_obj_set_pos(m.legs[i], x, top);
        lv_obj_set_height(m.legs[i], legs - foot);
        lv_obj_set_pos(m.feet[i], x + side * (outer ? bend : bend / 2), top + legs - foot);
        lv_obj_set_height(m.feet[i], foot);
    }
}

void rest(const Mascot &m)
{
    pose(m, 0, kLegs[0].h * m.cell, 0, 0, (kHands[0].y - kBody.y) * m.cell, 0);
}

/* The jump at `t` ms into its loop: the two keys either side, eased, then to the mascot's own cell and rise. */
Pose jump_at(const Mascot &m, uint32_t t)
{
    size_t i = 0;
    while (i + 2 < sizeof(kJump) / sizeof(kJump[0]) && static_cast<uint32_t>(kJump[i + 1].t) <= t) {
        i++;
    }
    const JumpKey &a = kJump[i];
    const JumpKey &b = kJump[i + 1];
    const int32_t span = b.t - a.t;
    const int32_t x = ease(b.ease, std::clamp<int32_t>(static_cast<int32_t>(t) - a.t, 0, span) * 1000 / span);
    const auto at = [&](int32_t from, int32_t to) {
        return scale(from * 1000 + (to - from) * x, m.cell, 1000 * kRefCell);
    };
    const int32_t raw = at(a.lift, b.lift);
    const int32_t lift = raw > 0 ? scale(raw, theme::kMascotJumpRise * kRefCell, kRefRise * m.cell) : raw;
    const int32_t tall = at(a.tall, b.tall);
    /* Between two grounded keys the legs follow the rounded lift and height, so rounding never lifts a foot off the
     * ground or sinks it in. */
    const int32_t legs = grounded(a) && grounded(b) ? 2 * m.cell + lift - tall : at(a.legs, b.legs);
    return {lift, legs, tall, at(a.wide, b.wide), at(a.hands, b.hands), at(a.bend, b.bend)};
}

/*
 * ONE MOVE HANDS OVER TO THE NEXT WITHOUT A JUMP (AGENT_DECK 9.4) — the developer's ask, 2026-10-29:
 * every move starts and ends on the rest pose, and one asked for while another plays waits for it to finish, then
 * begins from its start. Work plays the loop's opening once — the wink, the laptop out and opened — then only its
 * typing frames, f18–f36, over and over, the wipe's pose in place of a loop of them now and then; stopped, a wipe plays
 * out and the laptop is put away, f37–f47 — from the frame that mirrors where
 * the opening had got to if it was still taking it out, and not at all before the turn. A jump stopped lands and
 * settles first; stopped before its crouch or after its settle, it is at rest already.
 */
constexpr uint32_t frame_start(uint32_t n) { return n * theme::kMascotWorkMs / detail::kWorkGifFrames; }
constexpr uint32_t kTypingFromMs = frame_start(kTypingFirstFrame);
constexpr uint32_t kTypingLoopMs = frame_start(kTypingEndFrame) - kTypingFromMs;

/* For each frame of the opening (f1–f17), the frame of the putting away that mirrors it, or -1 before the turn. */
constexpr int8_t kPutAwayFrom[kTypingFirstFrame] = {-1, -1, -1, -1, -1, -1, -1, -1, 43, 43, 42, 42, 41, 40, 39, 38, 37};

constexpr uint32_t kJumpCrouchFrom = 300; // the jump at rest until its crouch…
constexpr uint32_t kJumpSettledAt = 1550; // …and again from its settle after the landing
static_assert(kJump[1].t == kJumpCrouchFrom && kJump[11].t == kJumpSettledAt,
              "the jump's rests are its 2nd and 12th keys");

/* The GIF frame `t` falls in, as work_frame_at() steps them. */
uint32_t frame_at(uint32_t t)
{
    uint32_t n = 0;
    while (n + 1 < static_cast<uint32_t>(detail::kWorkGifFrames) && t >= frame_start(n + 1)) {
        n++;
    }
    return n;
}

/*
 * WHEN HE SWEATS (AGENT_DECK 9.3): the typing runs in stretches of kSweatStretchLoops, and each but the first, at even
 * odds, has a sweat in it, starting on a loop of its own, never the stretch's first, so two never run back to back —
 * every 20 s or so, the gaps never a steady beat. Both come from a hash of the stretch's number and of when the work
 * began: each run of work has its own, and a record that rebuilds the page carries on. The beads swell for a typing
 * loop as the typing goes on, and the wipe takes the next, so the typing's clock needs no warp. Stopped while they
 * swell, they burst where they stand and the laptop goes away; stopped in the wipe, it plays out first, so the hand
 * never freezes on his brow.
 */
constexpr uint32_t kSweatStretchLoops = 8;
static_assert(theme::kMascotSweatMs == 2 * kTypingLoopMs, "the sweat is two typing loops: the beads, then the wipe");
static_assert(kWipe[std::size(kWipe) - 1].t == kTypingLoopMs, "the wipe's keys run a typing loop");

/*
 * WHEN HE FACEPLANTS (AGENT_DECK 9.3), the owner's rule, 2026-11-10: a separate roll in the sweat's pool, about one in
 * three of the stretches the sweat would play in, never two in a row. Each stretch with the sweat's slot rolls even
 * odds of its own hash; of a run of slots that all rolled it, the first faceplants, the next sweats, the next
 * faceplants and so on — so two never follow one another, and over time one slot in three faceplants. It starts on a
 * loop of the stretch's own, never its first, and takes kFaceplantLoops of them, the typing's clock running on under
 * it, so the typing it hands back to needs no warp.
 */
constexpr uint32_t kFaceplantUpMs = 3850;   // the head shake over: here the limit's puts the laptop away
constexpr uint32_t kFaceplantBackMs = 4080; // the work's: settled into the crouch, typing again
constexpr uint32_t kFaceplantLoops = 4;     // the typing loops it takes
constexpr uint32_t kFaceplantPutAwayFrom = frame_start(38); // the limit's: the work's put-away from the lid shutting
constexpr uint32_t kFaceplantChain = 64;    // the most stretches looked back over for a run of rolled slots
static_assert(kTripUpMs == static_cast<int32_t>(kFaceplantUpMs) && kTripFallFrom * 30 / 1000 == 26 &&
                  (kTripFallFrom - 1) * 30 / 1000 == 25 && kTripUpMs * 30 / 1000 < detail::kFaceplantSteps,
              "the skip trip plays the faceplant's baked frames from step 26, its rock back, to the head shake's end");
static_assert(static_cast<uint32_t>(detail::kFaceplantSteps) == (kFaceplantBackMs * 30 + 999) / 1000,
              "a baked frame each 33 ms step up to the typing again");
static_assert(kFaceplantBackMs <= kFaceplantLoops * kTypingLoopMs && kFaceplantLoops + 1 < kSweatStretchLoops,
              "the faceplant fits its loops, and they fit a stretch after its first loop");
static_assert(theme::kMascotFaceplantMs == kFaceplantUpMs + theme::kMascotWorkMs - kFaceplantPutAwayFrom,
              "the limit's faceplant: the faceplant to the head shake's end, then the work's put-away");

/* Stretch `stretch` of the work begun at `since`: whether it has the sweat's slot, and the slot's hash. */
bool interrupt_slot(uint32_t since, uint32_t stretch, uint32_t *hash)
{
    *hash = mix((since + stretch * 0x9E3779B9u) ^ 0x7C4A7C15u);
    return stretch != 0 && (*hash & 1u) == 0;
}

/* Whether the stretch's slot rolled the faceplant: even odds, a hash of its own. */
bool faceplant_rolled(uint32_t since, uint32_t stretch)
{
    return (mix((since + stretch * 0x9E3779B9u) ^ 0x3C6EF372u) & 1u) == 0;
}

/* Whether stretch `stretch` faceplants in place of the sweat: its slot rolled it, and the run of rolled slots straight
 * before it is even — none, or ended by a faceplant's sweat. */
bool faceplant_stretch(uint32_t since, uint32_t stretch)
{
    uint32_t hash = 0;
    if (!interrupt_slot(since, stretch, &hash) || !faceplant_rolled(since, stretch)) {
        return false;
    }
    uint32_t run = 0;
    for (uint32_t s = stretch, looked = 0; s > 1 && looked < kFaceplantChain; looked++) {
        s--;
        if (!interrupt_slot(since, s, &hash)) {
            continue;
        }
        if (!faceplant_rolled(since, s)) {
            break;
        }
        run++;
    }
    return run % 2 == 0;
}

/* `e` ms into the work begun at `since`: how far into the faceplant it is, or -1 for none — up to the typing again. */
int32_t faceplant_at(uint32_t since, uint32_t e)
{
    if (e < kTypingFromMs) {
        return -1;
    }
    const uint32_t typed = e - kTypingFromMs;
    const uint32_t stretch = typed / (kSweatStretchLoops * kTypingLoopMs);
    if (!faceplant_stretch(since, stretch)) {
        return -1;
    }
    uint32_t hash = 0;
    interrupt_slot(since, stretch, &hash);
    const uint32_t from =
        (stretch * kSweatStretchLoops + 1 + (hash >> 8) % (kSweatStretchLoops - kFaceplantLoops)) * kTypingLoopMs;
    return typed >= from && typed - from < kFaceplantBackMs ? static_cast<int32_t>(typed - from) : -1;
}

/* `e` ms into the work begun at `since`: how far into the sweat it is, or -1 for none. */
int32_t sweat_at(uint32_t since, uint32_t e)
{
    if (e < kTypingFromMs) {
        return -1;
    }
    const uint32_t typed = e - kTypingFromMs;
    const uint32_t stretch = typed / (kSweatStretchLoops * kTypingLoopMs);
    uint32_t h = 0;
    if (!interrupt_slot(since, stretch, &h) || faceplant_stretch(since, stretch)) {
        return -1;
    }
    const uint32_t from = (stretch * kSweatStretchLoops + 1 + (h >> 8) % (kSweatStretchLoops - 2)) * kTypingLoopMs;
    return typed >= from && typed - from < theme::kMascotSweatMs ? static_cast<int32_t>(typed - from) : -1;
}

/* Stopped work: the ms of the wipe still to play out before the laptop goes away. */
uint32_t sweat_left(const MascotPlay &play)
{
    const int32_t into = sweat_at(play.since, play.stopped - play.since);
    return into >= static_cast<int32_t>(kTypingLoopMs) ? theme::kMascotSweatMs - static_cast<uint32_t>(into) : 0;
}

/* The beads `u` ms into their typing loop: the big one swelling and running down past his right eye, a little one
 * coming up behind it. */
void sweat_beads(SweatView *view, int32_t u)
{
    if (u >= 150) {
        view->big = u < 300 ? 0 : u < 480 ? 1 : 2;
        view->foot = static_cast<int8_t>(10 + std::min<int32_t>(std::max<int32_t>(u - 700, 0) / 130, 3));
    }
    if (u >= 880) {
        view->little = u < 1020 ? 0 : 1;
    }
}

/* The sweat at `now`: `into` ms into it, as sweat_at() or the wipe playing out has it, or -1. */
SweatView sweat_view(const MascotPlay &play, int32_t into, uint32_t now)
{
    SweatView view;
    const auto loop = static_cast<int32_t>(kTypingLoopMs);
    if (play.stopped != kStillPlaying) {
        const int32_t was = sweat_at(play.since, play.stopped - play.since);
        const uint32_t since_stop = now - play.stopped;
        if (was >= 0 && was < loop) { /* stopped as they swelled: they burst where they stand */
            if (since_stop < static_cast<uint32_t>(kSweatPopMs)) {
                sweat_beads(&view, was);
                view.pop = view.big < 0 ? int16_t{-1} : static_cast<int16_t>(since_stop);
            }
            return view;
        }
    }
    if (into < 0) {
        return view;
    }
    if (into < loop) {
        sweat_beads(&view, into);
        return view;
    }
    const int32_t u = into - loop;
    for (const WipeHand &hand : kWipeHand) {
        if (u >= hand.t) {
            view.hand_col = hand.col;
            view.hand_row = hand.row;
        }
    }
    if (u < kSweatPopAtMs + kSweatPopMs) {
        sweat_beads(&view, loop);
        view.pop = u < kSweatPopAtMs ? int16_t{-1} : static_cast<int16_t>(u - kSweatPopAtMs);
    }
    return view;
}

/* Stopped work: the ms into the loop the putting away starts from, or -1 for none. */
int32_t put_away_from(const MascotPlay &play)
{
    const uint32_t e = play.stopped - play.since;
    const int32_t from = e >= kTypingFromMs ? static_cast<int32_t>(kTypingEndFrame) : kPutAwayFrom[frame_at(e)];
    return from < 0 ? -1 : static_cast<int32_t>(frame_start(static_cast<uint32_t>(from)));
}

/* A jump stopped `at` ms into its hop, at `stopped`: when it has landed and settled — at once if it was at rest. */
uint32_t jump_settled_at(uint32_t stopped, uint32_t at)
{
    return at < kJumpCrouchFrom || at >= kJumpSettledAt ? stopped : stopped + (kJumpSettledAt - at);
}

/* When a stopped wave or needs-you has finished: the hand down, or this turn's hop landed and settled. */
uint32_t wave_finished_at(const MascotPlay &play)
{
    const uint32_t e = play.stopped - play.since;
    const auto s = static_cast<int32_t>(e % theme::kMascotWaveMs);
    if (play.move == MascotMove::kWave || turn_waves(e / theme::kMascotWaveMs)) {
        int32_t from = 0;
        int32_t end = 0;
        wave_outro(s, &from, &end);
        return play.stopped + static_cast<uint32_t>(end - from);
    }
    if (s >= static_cast<int32_t>(kJumpTurnHops * theme::kMascotJumpMs)) {
        return play.stopped; /* resting after the turn's hops */
    }
    return jump_settled_at(play.stopped, static_cast<uint32_t>(s) % theme::kMascotJumpMs);
}

/* When a stopped move that plays to its end, `length` ms, has finished: at its end, or, played out, when the idle it
 * carried on into finishes. */
uint32_t played_out_at(const MascotPlay &play, uint32_t length)
{
    if (past_end(play, length, play.stopped)) {
        return idle_cycle_finishes(idle_after(play, length));
    }
    return static_cast<int32_t>(play.since + length - play.stopped) > 0 ? play.since + length : play.stopped;
}

/* When a stopped move has finished: the laptop away, the hop landed and settled. */
uint32_t finished_at(const MascotPlay &play)
{
    switch (play.move) {
    case MascotMove::kWork: {
        const int32_t from = put_away_from(play);
        return from < 0 ? play.stopped
                        : play.stopped + sweat_left(play) + (theme::kMascotWorkMs - static_cast<uint32_t>(from));
    }
    case MascotMove::kJump: return jump_settled_at(play.stopped, (play.stopped - play.since) % theme::kMascotJumpMs);
    case MascotMove::kWave:
    case MascotMove::kNeedsYou: return wave_finished_at(play);
    case MascotMove::kFinish:
        if (past_end(play, theme::kMascotFinishMs, play.stopped)) {
            return idle_cycle_finishes(idle_after(play, theme::kMascotFinishMs));
        }
        return play.stopped + finish_outro_ms(finish_stopped_at(play));
    case MascotMove::kIdle:
    case MascotMove::kIdleInPlace: return idle_cycle_finishes(play);
    case MascotMove::kHello: /* the hello plays to its end, then the next */
        if (past_end(play, kWaveHandMs, play.stopped)) {
            return idle_cycle_finishes(idle_after(play, kWaveHandMs));
        }
        return play.since + kWaveHandMs;
    case MascotMove::kSkip:
    case MascotMove::kSkipTrip:
    case MascotMove::kSpin:
    case MascotMove::kDust: return played_out_at(play, reaction_ms(play.move)); /* a reaction plays to its end */
    case MascotMove::kCelebrate: return played_out_at(play, theme::kMascotCelebrateMs); /* it plays to its end */
    case MascotMove::kFaceplant: /* it plays to its end, then rests */
        return static_cast<int32_t>(play.since + theme::kMascotFaceplantMs - play.stopped) > 0
                   ? play.since + theme::kMascotFaceplantMs
                   : play.stopped;
    case MascotMove::kRest: break;
    }
    return play.stopped;
}

/* `play` as it stands at `now`: a stopped move that has finished has handed over to the next, begun as it finished. */
MascotPlay settled(MascotPlay play, uint32_t now)
{
    while (play.stopped != kStillPlaying) {
        const uint32_t end = finished_at(play);
        if (static_cast<int32_t>(now - end) < 0) {
            break;
        }
        const MascotMove then = play.then;
        play = mascot_play(play.next, end);
        if (then != MascotMove::kRest) { /* a reaction begun: it plays to its end, then the move asked for */
            play.stopped = end;
            play.next = then;
        }
    }
    return play;
}

/* Where the work is at `now`, in ms into the loop: the typing looped while it works, the putting away once stopped. */
uint32_t work_time(const Motion &motion, uint32_t now)
{
    const MascotPlay &play = motion.play;
    if (play.stopped == kStillPlaying) {
        const uint32_t e = now - play.since;
        return e < kTypingFromMs ? e : kTypingFromMs + (e - kTypingFromMs) % kTypingLoopMs;
    }
    const uint32_t after = now - play.stopped;
    const uint32_t left = sweat_left(play);
    const uint32_t t = static_cast<uint32_t>(std::max<int32_t>(motion.outro_from, 0)) + (after > left ? after - left : 0);
    return std::min<uint32_t>(t, theme::kMascotWorkMs - 1);
}

/* How far into the sweat the work is at `now`, or -1: stopped, only a wipe playing out. */
int32_t sweat_into(const MascotPlay &play, uint32_t now)
{
    if (play.stopped == kStillPlaying) {
        return sweat_at(play.since, now - play.since);
    }
    const uint32_t left = sweat_left(play);
    const uint32_t after = now - play.stopped;
    return after < left ? static_cast<int32_t>(theme::kMascotSweatMs - left + after) : -1;
}

/* The faceplant `e` ms into it on view: its baked frame, the body's lift, the laptop's bounce, the head shake's sway
 * and the stress lines — redrawn only when one of them changes. */
void show_faceplant(Motion *motion, int32_t e)
{
    const int32_t step = std::min<int32_t>(e * 30 / 1000, detail::kFaceplantSteps - 1);
    const auto at = [&](int32_t px) { return scale(px, motion->m.cell, theme::kMascotCell); };
    const WorkPose work = {at(detail::kFaceplantLift[step]),   1000, 1000, at(detail::kFaceplantLaptop[step]), 0,
                           at(detail::kFaceplantSway[step])};
    const int pose = detail::kFaceplantFrame[step];
    const int hand = detail::kFaceplantHand[step];
    const int32_t hand_lift = at(detail::kFaceplantHandLift[step]);
    const StressView stress = stress_at(static_cast<int32_t>(step) * 1000 / 30); /* at its step's time, as the draft */
    motion->fp_e = e;
    if (motion->frame >= 0 && pose == motion->fp_pose && hand == motion->fp_hand && hand_lift == motion->fp_hand_lift &&
        work == motion->work && stress == motion->stress) {
        return;
    }
    if (work.lean != motion->work.lean) {
        lv_obj_set_style_transform_rotation(motion->m.pixels, work.lean, LV_PART_MAIN);
    }
    motion->fp_pose = pose;
    motion->fp_hand = hand;
    motion->fp_hand_lift = hand_lift;
    motion->frame = detail::kFaceplantLaptopPose; /* a frame on view: its moments are told from here on */
    motion->work = work;
    motion->stress = stress;
    motion->sweat = {};
    lv_obj_invalidate(motion->m.pixels);
    crop_pixels(motion);
}

/* The faceplant's frames left: the work's own drawn again from its next frame. */
void leave_faceplant(Motion *motion)
{
    motion->fp_e = -1;
    motion->fp_pose = -1;
    motion->fp_hand = -1;
    motion->fp_hand_lift = 0;
    motion->stress = {};
    lv_obj_invalidate(motion->m.pixels);
}

/* The skip trip going down: the front view's canvases away, the faceplant's frames on the trip's — mirrored going
 * right, no laptop — `f.down_ms` into the faceplant. */
void trip_down(Motion *motion, const PixelPose &f)
{
    const Mascot &m = motion->m;
    if (motion->fp_pose < 0) {
        const Canvas &c = kTripCanvas[f.down_mirror ? 1 : 0];
        lv_obj_set_hidden(m.pixel_legs, true);
        lv_obj_set_hidden(m.flag, true);
        lv_obj_set_style_transform_rotation(m.pixels, 0, LV_PART_MAIN);
        motion->canvas = {canvas_x(m, c), canvas_y(m, c), canvas_x(m, c) + c.cols * pixel(m) - 1,
                          canvas_y(m, c) + c.rows * pixel(m) - 1};
        motion->crop = {0, 0};
        motion->pivot = {0, 0};
        motion->fp_mirror = f.down_mirror;
        motion->fp_laptop = false;
        motion->sway_side = 0; /* down: the stumble's lean is behind him, no rock back to come */
        motion->work = {0, 1000, 1000, 0, 0, 0};
        motion->frame = -1;
    }
    show_faceplant(motion, f.down_ms);
}

/* The skip trip up again: the faceplant's frames gone, the front view's canvases back for the turn back and the
 * skip. */
void trip_up(Motion *motion)
{
    const Mascot &m = motion->m;
    leave_faceplant(motion);
    const Canvas &c = body_canvas(motion->move);
    motion->canvas = {canvas_x(m, c), canvas_y(m, c), canvas_x(m, c) + c.cols * pixel(m) - 1,
                      canvas_y(m, c) + c.rows * pixel(m) - 1};
    motion->crop = {0, 0};
    motion->frame = -1;
    motion->fp_mirror = false;
    motion->fp_laptop = true;
    motion->work = {0, 1000, 1000, 0, 0, 0};
    motion->sway_side = 0; /* up again from the front view's rest: the skip's wind-up is a wind-up, not a rock back */
    lv_obj_set_hidden(m.pixel_legs, false);
}

uint32_t period(MascotMove move)
{
    return move == MascotMove::kWork        ? theme::kMascotWorkMs
           : move == MascotMove::kFinish    ? theme::kMascotFinishMs
           : move == MascotMove::kCelebrate ? theme::kMascotCelebrateMs
           : move == MascotMove::kFaceplant ? theme::kMascotFaceplantMs
           : move == MascotMove::kIdle || move == MascotMove::kIdleInPlace ? kIdleStretchMs
           : mascot_reaction(move)          ? reaction_ms(move)
           : move == MascotMove::kWave || move == MascotMove::kNeedsYou || move == MascotMove::kHello
               ? theme::kMascotWaveMs
                                            : theme::kMascotJumpMs;
}

void begin(Motion *motion, const MascotPlay &play);

/* A rig pose on view, if it changed: its lift's moments told, then drawn. */
void show_rig_pose(Motion *motion, const Pose &p)
{
    if (motion->drawn && p == motion->last) {
        return;
    }
    motion->hushed = !motion->drawn; /* a first frame: its state learnt, nothing told */
    tell_lift(*motion, p.lift);
    motion->hushed = false;
    pose(motion->m, p.lift, p.legs, p.tall, p.wide, p.hands, p.bend);
    motion->last = p;
    motion->drawn = true;
}

/* The work's moments since the last frame, `t` ms into its loop and `into` into the sweat: its keys', each bead as it
 * comes, the wipe's beats, the pop and the wipe. */
void tell_work_frame(Motion *motion, uint32_t t, int32_t into, const SweatView &sweat)
{
    tell_work(*motion, static_cast<uint32_t>(motion->work_t), t);
    if ((sweat.big >= 0 && motion->sweat.big < 0) || (sweat.little >= 0 && motion->sweat.little < 0)) {
        tell(motion, MascotSfx::kBead); /* each bead as it comes */
    }
    /* The wipe's own beats past the pop: the flick off his side, the sigh's squash. */
    const int32_t wipe_from = static_cast<int32_t>(kTypingLoopMs);
    for (const auto &[at, s] : {std::pair{kWipeFlickMs, MascotSfx::kHandDown},
                                std::pair{kWipeSighMs, MascotSfx::kCrouch}}) {
        if (motion->sweat_t >= 0 && motion->sweat_t < wipe_from + at && into >= wipe_from + at) {
            tell(motion, s);
        }
    }
    if (sweat.pop >= 0 && motion->sweat.pop < 0) {
        tell(motion, MascotSfx::kPop);
    }
    if (sweat.hand_col >= 0 && motion->sweat.hand_col < 0) {
        tell(motion, MascotSfx::kWipe);
    }
}

/* The work's frame, its physics and its sweat on view, if any changed or the faceplant has just been left. */
void show_work(Motion *motion, int frame, const WorkPose &work, const SweatView &sweat, bool was_faceplant)
{
    if (was_faceplant || frame != motion->frame || !(work == motion->work) || !(sweat == motion->sweat)) {
        /* The tilt turns the whole drawn canvas about the right foot, as one rigid shape: LVGL's transform, which
         * draws it through a layer — so only while it is tilted, the rest of the loop drawn straight. */
        if (work.lean != motion->work.lean) {
            lv_obj_set_style_transform_rotation(motion->m.pixels, work.lean, LV_PART_MAIN);
        }
        if (frame == detail::kFirstWinkPose && motion->frame != frame && motion->frame >= 0) {
            tell(motion, MascotSfx::kWink);
        }
        motion->frame = frame;
        motion->work = work;
        motion->sweat = sweat;
        lv_obj_invalidate(motion->m.pixels);
        crop_pixels(motion);
    }
}

/* The work's frame (tick_cb()): the typing looped, the sweat and its wipe, now and then the faceplant, the putting
 * away once stopped. */
void tick_work(Motion *motion, const MascotPlay &play, uint32_t now)
{
    const uint32_t t = work_time(*motion, now);
    /* Now and then, the faceplant in place of the typing: its own frames and moments, the typing's clock running
     * on. */
    const int32_t fp = play.stopped == kStillPlaying ? faceplant_at(play.since, now - play.since) : -1;
    if (fp >= 0) {
        faceplant_moments(*motion, now - static_cast<uint32_t>(fp), fp, kFaceplantBackMs,
                          motion->work_t >= 0 && motion->frame >= 0);
        motion->work_t = static_cast<int32_t>(t);
        motion->sweat_t = -1;
        show_faceplant(motion, fp);
        return;
    }
    const bool was_faceplant = motion->fp_e >= 0;
    if (was_faceplant) {
        leave_faceplant(motion);
    }
    const int32_t into = sweat_into(play, now);
    const SweatView sweat = sweat_view(play, into, now);
    if (motion->work_t >= 0 && motion->frame >= 0) {
        tell_work_frame(motion, t, into, sweat);
    }
    motion->work_t = static_cast<int32_t>(t);
    motion->sweat_t = into;
    const bool wiping = sweat.hand_col >= 0;
    const int frame = wiping ? detail::kWipePose : work_frame_at(t);
    const WorkPose work = wiping ? motion_at(kWipe, into - static_cast<int32_t>(kTypingLoopMs), motion->m.cell)
                                 : work_motion_at(t, motion->m.cell);
    show_work(motion, frame, work, sweat, was_faceplant);
}

/* THE LIMIT HIT AT WORK (tick_cb()): the faceplant from the typing to the head shake's end, then the work's put-away to
 * rest from the lid shutting — its moments the work's own. */
void tick_faceplant(Motion *motion, const MascotPlay &play, uint32_t now)
{
    const uint32_t e = now - play.since;
    const bool told = motion->frame >= 0; /* the put-away's: never on a first frame */
    if (e < kFaceplantUpMs) {
        faceplant_moments(*motion, play.since, static_cast<int32_t>(e), kFaceplantUpMs, false);
        show_faceplant(motion, static_cast<int32_t>(e));
        return;
    }
    const uint32_t t = std::min(kFaceplantPutAwayFrom + (e - kFaceplantUpMs), theme::kMascotWorkMs - 1);
    const bool was_faceplant = motion->fp_e >= 0;
    if (was_faceplant) {
        faceplant_moments(*motion, play.since, INT32_MAX, kFaceplantUpMs, false);
        leave_faceplant(motion);
        motion->work_t = static_cast<int32_t>(kFaceplantPutAwayFrom);
    }
    if (told && motion->work_t >= 0) {
        tell_work(*motion, static_cast<uint32_t>(motion->work_t), t);
    }
    motion->work_t = static_cast<int32_t>(t);
    const int frame = work_frame_at(t);
    const WorkPose work = work_motion_at(t, motion->m.cell);
    if (was_faceplant || frame != motion->frame || !(work == motion->work)) {
        if (work.lean != motion->work.lean) {
            lv_obj_set_style_transform_rotation(motion->m.pixels, work.lean, LV_PART_MAIN);
        }
        motion->frame = frame;
        motion->work = work;
        lv_obj_invalidate(motion->m.pixels);
        crop_pixels(motion);
    }
}

/* Needs you: this turn's hops on the rig, or the wave in the pixels — switched as a turn begins. Whether it drew the
 * hops; the wave is the front view's to draw. */
bool tick_needs_you_hops(Motion *motion, const MascotPlay &play, uint32_t now)
{
    const NeedsAt n = needs_at(play, now);
    if (n.wave != motion->front) {
        show(motion->m, n.wave ? MascotMove::kWave : MascotMove::kJump);
        motion->front = n.wave;
        motion->drawn = false;
        if (!n.wave) {
            rest(motion->m);
        }
    }
    if (n.wave) {
        return false;
    }
    show_rig_pose(motion, jump_at(motion->m, n.u < static_cast<int32_t>(kJumpTurnHops * theme::kMascotJumpMs)
                                                 ? static_cast<uint32_t>(n.u) % theme::kMascotJumpMs
                                                 : 0));
    return true;
}

/* How long a front-view move plays before it carries on into the idle. */
uint32_t front_length(MascotMove move)
{
    return move == MascotMove::kFinish  ? theme::kMascotFinishMs
           : move == MascotMove::kHello ? static_cast<uint32_t>(kWaveHandMs)
           : mascot_reaction(move)      ? reaction_ms(move)
                                        : theme::kMascotCelebrateMs;
}

/* The front view's pose at `now`: the move's own, or, played out unstopped by `at` after `length`, the idle's. */
PixelPose front_pose_at(const Motion &motion, const MascotPlay &play, uint32_t now, uint32_t length, uint32_t at)
{
    const int32_t cell = motion.m.cell;
    return motion.move == MascotMove::kNeedsYou ? wave_pose_at(needs_at(play, now).u, cell)
           : motion.move == MascotMove::kWave   ? wave_pose_at(wave_time(play, now), cell)
           : motion.move == MascotMove::kIdle || motion.move == MascotMove::kIdleInPlace
               ? idle_cycle_pose(play, now, cell)
           : past_end(play, length, at)         ? idle_cycle_pose(idle_after(play, length), now, cell)
           : motion.move == MascotMove::kHello  ? wave_pose_at(static_cast<int32_t>(now - play.since), cell)
           : mascot_reaction(motion.move)
               ? reaction_pose_at(motion.move, static_cast<int32_t>(now - play.since), cell, travel_dir(play.since))
           : motion.move == MascotMove::kFinish ? finish_pose_at(finish_time(play, now), cell)
                                                : celebrate_pose_at(celebrate_time(play, now), cell);
}

/* The idle's stretch on view, the move's own or the one after it played out (`length`, by `at`), or -1 for none. */
void note_stretch(Motion *motion, const MascotPlay &play, uint32_t now, uint32_t length, uint32_t at)
{
    /* EACH IDLE STRETCH BEGINS WITH NO SIDE LEFT OVER — the owner's fix, 2026-11-17: a stretch begins at rest,
     * so the side the last one leant to is no lean to rock back from; kept, the next lean told a rock and its own
     * sway, two sways for one (AGENT_DECK 17, kSway). The skip trip's getting up clears its own (trip_up()). */
    const bool idling = motion->move == MascotMove::kIdle || motion->move == MascotMove::kIdleInPlace;
    int32_t stretch = -1;
    if (idling || (motion->move != MascotMove::kNeedsYou && motion->move != MascotMove::kWave &&
                   past_end(play, length, at))) {
        const MascotPlay idle = idling ? play : idle_after(play, length);
        stretch = static_cast<int32_t>(((idle.stopped == kStillPlaying ? now : idle.stopped) - idle.since) /
                                       kIdleStretchMs);
    }
    if (stretch >= 0 && stretch != motion->stretch) {
        motion->sway_side = 0;
    }
    motion->stretch = stretch;
}

/* The body and props' moments for the front view's pose `f`: told, never on a first frame — there the state is learnt
 * from the pose, so a page rebuilt mid-air does not hop again. From the skip trip's catch to its turn back
 * (`tripping`), its own table's (tell_trip()). */
void tell_front_frame(Motion *motion, const PixelPose &f, bool tripping, uint32_t now)
{
    if (tripping && motion->drawn && motion->airborne) { /* the catch's landing, its threshold a frame late */
        motion->airborne = false;
        motion->crouched = true;
        tell(motion, MascotSfx::kLand);
    }
    motion->hushed = !motion->drawn || tripping;
    tell_front(*motion, motion->drawn ? motion->pose : f, f);
    motion->hushed = false;
    if (f.trip) {
        trip_moments(*motion, now - static_cast<uint32_t>(f.trip_u), f.trip_u,
                     !motion->drawn ? kTripNotDrawn : motion->pose.trip ? motion->pose.trip_u : -1);
    }
}

/* The eyes changed on this frame, from the pose drawn to `f`: told. */
void tell_front_eyes(Motion *motion, const PixelPose &f)
{
    if (f.happy && !motion->pose.happy) {
        tell(motion, MascotSfx::kHappy);
    }
    if (f.chevron && !motion->pose.chevron) {
        tell(motion, MascotSfx::kChevron);
    }
    if (f.shut && !motion->pose.shut && !motion->blink) {
        tell(motion, MascotSfx::kBlink);
    }
}

/* The front view's pose `f` drawn: the tilt about his hips, the flag carried to the hand, every canvas redrawn. */
void place_front(Motion *motion, const PixelPose &f)
{
    const Mascot &m = motion->m;
    motion->pose = f;
    /* The tilt turns the body and the flag about his hips as one rigid shape: LVGL's transform, which draws each
     * through a layer — so only while he leans; the legs stay planted on their own canvas. The flag is carried to
     * the hand, its pivot the same hips. */
    const FinishMap map = finish_map(*motion);
    const int32_t hip_x = map.x(map.pivot);
    const int32_t hip_y = map.hip_y();
    motion->pivot = {hip_x, hip_y};
    lv_obj_set_style_transform_rotation(m.pixels, f.lean, LV_PART_MAIN);
    lv_obj_set_hidden(m.flag, f.flag < 0);
    if (f.flag >= 0) {
        const int32_t p = map.p;
        const int32_t fx = map.grip_x() - detail::kFlagFrames[f.flag].pole * p;
        const int32_t fy = map.grip_y() - detail::kFlagPoleAbove * p;
        lv_obj_set_pos(m.flag, canvas_x(m, map.c) + fx, canvas_y(m, map.c) + fy);
        lv_obj_set_style_transform_pivot_x(m.flag, hip_x - fx, LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(m.flag, hip_y - fy, LV_PART_MAIN);
        lv_obj_set_style_transform_rotation(m.flag, f.lean, LV_PART_MAIN);
        lv_obj_invalidate(m.flag);
    }
    motion->drawn = true;
    lv_obj_invalidate(m.pixels);
    crop_pixels(motion); /* the object to this frame's figure, the pivot with it */
    lv_obj_invalidate(m.pixel_legs);
    crop_legs(motion);
    if (motion->move == MascotMove::kCelebrate) {
        lv_obj_invalidate(m.confetti);
    }
    if (motion->move == MascotMove::kSpin || motion->move == MascotMove::kDust) {
        lv_obj_invalidate(m.dust);
    }
}

/* The front view's frame (tick_cb()): the finish, celebrate, idle, wave, hello, a reaction, or needs-you's wave. */
void tick_front(Motion *motion, const MascotPlay &play, uint32_t now)
{
    /* the finish and the celebrate, played out unstopped, carry on into the idle */
    const uint32_t at = play.stopped == kStillPlaying ? now : play.stopped;
    const uint32_t length = front_length(motion->move);
    const PixelPose f = front_pose_at(*motion, play, now, length, at);
    note_stretch(motion, play, now, length, at);
    if (motion->drawn && f == motion->pose) {
        /* A STILL FRAME HOLDS THE LOOK TOO: the look-around's beat at rest redraws nothing, and counted only in
         * redrawn frames its first dart from the middle left no held look, so it went unheard (every movement is
         * heard). */
        if (motion->look_still < kLookHeld) {
            motion->look_still++;
        }
        return;
    }
    if (!motion->drawn || f.travel != motion->pose.travel) { /* walking: the whole mascot across the glass */
        set_travel(motion->m, f.travel);
    }
    const bool tripping = f.trip && f.trip_u < kTripBackU;
    tell_front_frame(motion, f, tripping, now);
    if (f.down) { /* the skip trip, down: the faceplant's frames on the trip's canvas, in the front view's place */
        motion->pose = f;
        trip_down(motion, f);
        motion->drawn = true;
        return;
    }
    if (motion->fp_pose >= 0) {
        trip_up(motion);
    }
    if (motion->drawn && !tripping) { /* the eyes changed on this frame: told, never on a first frame */
        tell_front_eyes(motion, f);
    }
    place_front(motion, f);
}

/* Every frame: the pose on the move's clock, from when it began, so a move never restarts with the mascot — and once
 * a stopped move has finished, the next begun. */
void tick_cb(void *var, int32_t)
{
    auto *motion = static_cast<Motion *>(var);
    const uint32_t now = lv_tick_get();
    const MascotPlay play = settled(motion->play, now);
    if (play.move != motion->play.move || play.since != motion->play.since) {
        begin(motion, play);
        return;
    }
    if (motion->move == MascotMove::kWork) {
        tick_work(motion, play, now);
        return;
    }
    if (motion->move == MascotMove::kFaceplant) {
        tick_faceplant(motion, play, now);
        return;
    }
    if (motion->move == MascotMove::kNeedsYou && tick_needs_you_hops(motion, play, now)) {
        return;
    }
    if (motion->front) {
        tick_front(motion, play, now);
        return;
    }
    if (motion->move != MascotMove::kJump) {
        return;
    }
    show_rig_pose(motion, jump_at(motion->m, (now - play.since) % theme::kMascotJumpMs));
}

/*
 * THE BLINK (AGENT_DECK 9.3): the eyes shut for kBlinkShutMs now and then at random — a blink in about seven seconds'
 * slots in twenty, every 3 s or so with no cap on a gap (one in a hundred runs past 11 s) — and one blink in four a
 * double; the developer's ask, 2026-10-29. In whatever move: the rig's eyes shut to their lower half, a pixel frame's
 * to their lower row; not over the wink, an eye already shut. The blinks are read off the clock — each second's slot
 * hashed to say whether it blinks, when and how — not from when the mascot was made, so a page rebuilt on each record
 * never puts the next blink off, and the simulator draws the same frames twice (HOST_SIMULATOR 3.1).
 */
constexpr uint32_t kBlinkSlotMs = 1000;
constexpr uint32_t kBlinkShutMs = 110;
constexpr uint32_t kBlinkOpenBetweenMs = 120; // a double blink's open beat
constexpr uint32_t kBlinkStartsWithinMs = 600; // into its slot, so the next slot's blink is a beat after at least

/* The eyes shut at `now`: a blink in this second's slot, or a double one from the slot before running on. */
bool blink_at(uint32_t now)
{
    for (uint32_t back = 0; back < 2; back++) {
        const uint32_t slot = now / kBlinkSlotMs - back;
        const uint32_t h = mix(slot ^ 0x9E3779B9u); /* salted, so slot 0 is not a blink every boot */
        if (h % 20 >= 7) {
            continue; /* no blink in this slot */
        }
        const uint32_t start = slot * kBlinkSlotMs + (h >> 8) % kBlinkStartsWithinMs;
        const uint32_t into = now - start;
        if (static_cast<int32_t>(into) < 0) {
            continue;
        }
        const bool twice = ((h >> 4) & 3) == 0;
        if (into < kBlinkShutMs ||
            (twice && into >= kBlinkShutMs + kBlinkOpenBetweenMs && into < 2 * kBlinkShutMs + kBlinkOpenBetweenMs)) {
            return true;
        }
    }
    return false;
}

void set_blink(Motion *motion, bool shut)
{
    const Mascot &m = motion->m;
    if (shut && !motion->blink && motion->fp_pose < 0 && !motion->pose.stumble) { /* a faceplant's eyes are its
                                                                                     * frames' own, and the trip's
                                                                                     * stumble has none: no blink */
        tell(motion, MascotSfx::kBlink);
    }
    motion->blink = shut;
    for (size_t i = 0; i < m.eyes.size(); i++) {
        const int32_t top = (kEyes[i].y - kBody.y) * m.cell;
        lv_obj_set_y(m.eyes[i], shut ? top + m.cell / 2 : top);
        lv_obj_set_height(m.eyes[i], shut ? m.cell / 2 : kEyes[i].h * m.cell);
    }
    if (pixel_style(motion->move)) {
        lv_obj_invalidate(m.pixels);
    }
}

/* Every refresh: the eyes as the clock has them. */
void blink_cb(lv_timer_t *timer)
{
    auto *motion = static_cast<Motion *>(lv_timer_get_user_data(timer));
    /* At rest, on a glass where a still page sends nothing, the eyes stay open (theme::kMascotBlinksAtRest). */
    const bool still = !theme::kMascotBlinksAtRest && motion->move == MascotMove::kRest;
    const bool shut = !still && blink_at(lv_tick_get());
    if (shut != motion->blink) {
        set_blink(motion, shut);
    }
}

void motion_deleted_cb(lv_event_t *event)
{
    auto *motion = static_cast<Motion *>(lv_event_get_user_data(event));
    lv_anim_delete(motion, nullptr);
    if (motion->blink_timer != nullptr) {
        lv_timer_delete(motion->blink_timer);
    }
    delete motion;
}

/* The rig's parts on `m.root`: the legs and feet, the body over them, its hands and eyes. */
void make_rig(Mascot &m)
{
    const int32_t cell = m.cell;
    /* The legs first, so the body draws over the row they share with it when a lean lifts it. */
    for (size_t i = 0; i < m.legs.size(); i++) {
        m.legs[i] = make_part(m.root, kLegs[i], cell, theme::mascot_body());
        m.feet[i] = make_part(m.root, kLegs[i], cell, theme::mascot_body());
    }
    m.body = make_part(m.root, kBody, cell, theme::mascot_body());
    /* The hands reach past the body's sides, so the body does not clip its children. */
    lv_obj_set_overflow_visible(m.body, true);
    lv_obj_add_event_cb(m.body, ext_draw_cb, LV_EVENT_REFR_EXT_DRAW_SIZE, reinterpret_cast<void *>(intptr_t{cell}));
    lv_obj_refresh_ext_draw_size(m.body);
    for (size_t i = 0; i < m.hands.size(); i++) {
        const Cells h = {kHands[i].x - kBody.x, kHands[i].y - kBody.y, kHands[i].w, kHands[i].h};
        m.hands[i] = make_part(m.body, h, cell, theme::mascot_body());
    }
    for (size_t i = 0; i < m.eyes.size(); i++) {
        const Cells e = {kEyes[i].x - kBody.x, kEyes[i].y - kBody.y, kEyes[i].w, kEyes[i].h};
        m.eyes[i] = make_part(m.body, e, cell, theme::background());
    }
}

/* The pixel style's canvases on `m.root`, in drawing order. */
void make_canvases(Mascot &m)
{
    /* The pixel style's canvases, hidden until a frame plays: the finish's legs first, under its body. */
    m.pixel_legs = lv_obj_create(m.root);
    lv_obj_remove_style_all(m.pixel_legs);
    lv_obj_set_pos(m.pixel_legs, canvas_x(m, kLegsCanvas), canvas_y(m, kLegsCanvas));
    lv_obj_set_size(m.pixel_legs, kLegsCanvas.cols * pixel(m), kLegsCanvas.rows * pixel(m));
    lv_obj_set_scrollable(m.pixel_legs, false);
    lv_obj_set_clickable(m.pixel_legs, false);
    lv_obj_set_hidden(m.pixel_legs, true);
    /* The finish's flag, behind the body: a frame's box, carried to the raised hand. */
    m.flag = lv_obj_create(m.root);
    lv_obj_remove_style_all(m.flag);
    lv_obj_set_size(m.flag, detail::kFlagCols * pixel(m), detail::kFlagRows * pixel(m));
    lv_obj_set_scrollable(m.flag, false);
    lv_obj_set_clickable(m.flag, false);
    lv_obj_set_hidden(m.flag, true);
    m.pixels = lv_obj_create(m.root);
    lv_obj_remove_style_all(m.pixels);
    lv_obj_set_scrollable(m.pixels, false);
    lv_obj_set_clickable(m.pixels, false);
    /* The dust a landing kicks up, at his feet, upright on the ground. */
    m.dust = lv_obj_create(m.root);
    lv_obj_remove_style_all(m.dust);
    lv_obj_set_pos(m.dust, canvas_x(m, kDustCanvas), canvas_y(m, kDustCanvas));
    lv_obj_set_size(m.dust, kDustCanvas.cols * pixel(m), kDustCanvas.rows * pixel(m));
    lv_obj_set_scrollable(m.dust, false);
    lv_obj_set_clickable(m.dust, false);
    lv_obj_set_hidden(m.dust, true);
    /* The celebrate's confetti, over the body, upright in the glass's frame. */
    m.confetti = lv_obj_create(m.root);
    lv_obj_remove_style_all(m.confetti);
    lv_obj_set_pos(m.confetti, canvas_x(m, kConfettiCanvas), canvas_y(m, kConfettiCanvas));
    lv_obj_set_size(m.confetti, kConfettiCanvas.cols * pixel(m), kConfettiCanvas.rows * pixel(m));
    lv_obj_set_scrollable(m.confetti, false);
    lv_obj_set_clickable(m.confetti, false);
}

} // namespace

Mascot make_mascot(lv_obj_t *parent, int32_t cell)
{
    Mascot m = {};
    m.cell = cell;
    m.root = lv_obj_create(parent);
    lv_obj_remove_style_all(m.root);
    lv_obj_set_size(m.root, theme::kMascotCols * cell, theme::kMascotRows * cell);
    lv_obj_set_scrollable(m.root, false);
    lv_obj_set_clickable(m.root, false);
    /* A move carries parts past the grid — the hands splayed by a squash, the feet of a stretch — so the root draws its
     * children a cell beyond its box, as the body does its hands, or the laptop's reach while a pixel frame plays
     * (root_ext_cb()). */
    lv_obj_set_overflow_visible(m.root, true);

    make_rig(m);
    make_canvases(m);
    show(m, MascotMove::kRest);

    auto *motion =
        new (std::nothrow) Motion{m, MascotMove::kRest, {}, false, -1, {0, 1000, 1000, 0, 0, 0}, theme::mascot_body(),
                                  theme::mascot_shade(), theme::mascot_tint(), mascot_play(MascotMove::kRest, 0), -1,
                                  nullptr, false, {}, false, {}, nullptr, nullptr, -1, false, false, false, 0, -1, 0,
                                  -1};
    if (motion != nullptr) {
        lv_obj_set_user_data(m.root, motion);
        lv_obj_add_event_cb(m.root, motion_deleted_cb, LV_EVENT_DELETE, motion);
        lv_obj_add_event_cb(m.root, root_ext_cb, LV_EVENT_REFR_EXT_DRAW_SIZE, motion);
        lv_obj_add_event_cb(m.pixels, draw_pixels_cb, LV_EVENT_DRAW_MAIN, motion);
        lv_obj_add_event_cb(m.pixel_legs, draw_legs_cb, LV_EVENT_DRAW_MAIN, motion);
        lv_obj_add_event_cb(m.flag, draw_flag_cb, LV_EVENT_DRAW_MAIN, motion);
        lv_obj_add_event_cb(m.confetti, draw_confetti_cb, LV_EVENT_DRAW_MAIN, motion);
        lv_obj_add_event_cb(m.dust, draw_dust_cb, LV_EVENT_DRAW_MAIN, motion);
        /* No memory for the blink's timer: the eyes stay open. */
        motion->blink_timer = lv_timer_create(blink_cb, LV_DEF_REFR_PERIOD, motion);
        lv_obj_refresh_ext_draw_size(m.root);
    }
    /* Each leg in two halves, meeting straight at rest. */
    rest(m);
    return m;
}

namespace {

/* Begins `play` on the mascot, from wherever it stands. */
void begin(Motion *motion, const MascotPlay &play)
{
    const Mascot &m = motion->m;
    const MascotMove move = play.move;
    lv_anim_delete(motion, nullptr);
    motion->play = play;
    motion->outro_from = move == MascotMove::kWork && play.stopped != kStillPlaying ? put_away_from(play) : -1;
    motion->move = move;
    motion->drawn = false;
    motion->front = front_pixels(move);
    motion->frame = -1;
    motion->work_t = -1;
    motion->airborne = false;
    motion->crouched = false;
    motion->sway_side = 0;
    motion->stretch = -1;
    motion->look_still = 0;
    motion->sweat_t = -1;
    motion->work = {0, 1000, 1000, 0, 0, 0};
    motion->sweat = {};
    motion->fp_e = -1;
    motion->fp_pose = -1;
    motion->fp_hand = -1;
    motion->fp_hand_lift = 0;
    motion->stress = {};
    motion->fp_mirror = false;
    motion->fp_laptop = true;
    lv_obj_set_style_transform_rotation(m.pixels, 0, LV_PART_MAIN); /* a move begun mid-tilt stands up */
    show(m, move);
    rest(m);
    lv_obj_refresh_ext_draw_size(m.root);
    if (move == MascotMove::kRest) {
        return;
    }
    /* A ticker: the pose comes from the move's clock in tick_cb(), not from the animation's own value — which still
     * runs through every ms of the loop, since LVGL calls the callback only when the value changes. */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, motion);
    lv_anim_set_exec_cb(&a, tick_cb);
    lv_anim_set_values(&a, 0, static_cast<int32_t>(period(move)));
    lv_anim_set_duration(&a, period(move));
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);  // a11y-exception: the mascot moves only where a page plays it — the hardware test's demo page, the person's ask — each move ending on rest; Agent Jam Sticky has no reduced-motion setting yet, and its branch, the move's end pose alone (MASCOT §6), is STICKY_DESIGN_TOKENS §8's open item
    tick_cb(motion, 0);
}

} // namespace

namespace {

/* Work stopped at `now` mid-faceplant, before the head shake's end: it plays on as kFaceplant from where it is, the
 * laptop put away after it. False, and `play` as it was, for anything else. */
bool faceplant_on(MascotPlay &play, uint32_t now)
{
    if (play.move != MascotMove::kWork || play.stopped != kStillPlaying) {
        return false;
    }
    const int32_t fp = faceplant_at(play.since, now - play.since);
    if (fp < 0 || fp >= static_cast<int32_t>(kFaceplantUpMs)) {
        return false;
    }
    play.move = MascotMove::kFaceplant;
    play.since = now - static_cast<uint32_t>(fp);
    return true;
}

/* A looping move's jaunt, `into` ms after it began, still playing: the jump's hop, needs-you's turn, the wave's loop.
 * False for any other move. */
bool looping_jaunt(const MascotPlay &play, uint32_t into, MascotJaunt *jaunt, uint32_t *since)
{
    switch (play.move) {
    case MascotMove::kJump: /* a hop each loop */
        *jaunt = MascotJaunt::kHop;
        *since = play.since + into / theme::kMascotJumpMs * theme::kMascotJumpMs;
        return true;
    case MascotMove::kNeedsYou: { /* each turn two hops or the wave */
        const uint32_t turn = into / theme::kMascotWaveMs;
        *jaunt = turn_waves(turn) ? MascotJaunt::kWave : MascotJaunt::kHops;
        *since = play.since + turn * theme::kMascotWaveMs;
        return true;
    }
    case MascotMove::kWave:
        *jaunt = MascotJaunt::kWave;
        *since = play.since + into / theme::kMascotWaveMs * theme::kMascotWaveMs;
        return true;
    default: break;
    }
    return false;
}

/* The idle's jaunt at `now`: this stretch's skip, walk or thinking, while it plays. False for a look, or once done. */
bool idle_jaunt(const MascotPlay &idle, uint32_t now, MascotJaunt *jaunt, uint32_t *since)
{
    const uint32_t e = (idle.stopped == kStillPlaying ? now : idle.stopped) - idle.since;
    const uint32_t stretch = e / kIdleStretchMs;
    const uint32_t began = idle.since + stretch * kIdleStretchMs;
    switch (stretch_kind(stretch, idle.since, idle.move == MascotMove::kIdleInPlace)) {
    case Stretch::kSkip: { /* stopped, it still plays to its end — tripped, its longer end */
        const int32_t stopped = idle.stopped == kStillPlaying ? INT32_MAX : static_cast<int32_t>(e % kIdleStretchMs);
        if (now - began < skip_ms(idle_skip_trip(idle.since, stretch, travel_dir(began), stopped))) {
            *jaunt = MascotJaunt::kSkip;
            *since = began;
            return true;
        }
        return false;
    }
    case Stretch::kWalk: /* stopped, it still walks on home */
        if (now - began < theme::kMascotWalkMs) {
            *jaunt = MascotJaunt::kWalk;
            *since = began;
            return true;
        }
        return false;
    case Stretch::kLean: /* a loop of the lean to look, from its start; stopped, it comes home: the thinking stops */
        if (idle.stopped == kStillPlaying) {
            *jaunt = MascotJaunt::kThink;
            *since = began + (now - began) / theme::kMascotIdleMs * theme::kMascotIdleMs;
            return true;
        }
        return false;
    case Stretch::kLook: return false;
    }
    return false;
}

} // namespace

void ask_mascot(MascotPlay &play, MascotMove move, uint32_t now)
{
    play = settled(play, now);
    if (move != play.move && faceplant_on(play, now)) {
        play.stopped = now;
        play.next = move;
        return;
    }
    if (play.stopped != kStillPlaying) {
        if (mascot_reaction(play.next) || play.then != MascotMove::kRest) {
            play.then = move; /* a reaction to come: what follows it is the latest asked for */
        } else {
            play.next = move; /* still finishing: what follows is the latest asked for */
        }
        return;
    }
    if (move != play.move) {
        play.stopped = now;
        play.next = move;
    }
}

void react_mascot(MascotPlay &play, MascotMove reaction, MascotMove move, uint32_t now)
{
    play = settled(play, now);
    faceplant_on(play, now);
    if (play.stopped == kStillPlaying) {
        play.stopped = now;
    }
    if (play.next == MascotMove::kCelebrate && play.then != MascotMove::kRest) {
        play.then = move; /* a waiting celebrate keeps its place: the reaction that came after it is dropped */
        return;
    }
    play.next = reaction;
    play.then = move;
}

void fall_mascot(MascotPlay &play, MascotMove move, uint32_t now)
{
    play = settled(play, now);
    if (play.move == MascotMove::kWork && play.stopped == kStillPlaying && move != MascotMove::kWork) {
        const uint32_t e = now - play.since;
        if (faceplant_on(play, now)) { /* a faceplant under way: on from where it is */
            play.stopped = now;
            play.next = move;
            return;
        }
        if (e >= kTypingFromMs && faceplant_at(play.since, e) < 0 && sweat_at(play.since, e) < 0) {
            play = mascot_play(MascotMove::kFaceplant, now); /* typing: down from its start */
            play.stopped = now;
            play.next = move;
            return;
        }
    }
    ask_mascot(play, move, now);
}

void play_mascot(const Mascot &m, const MascotPlay &play)
{
    if (m.root == nullptr) {
        return;
    }
    if (Motion *motion = motion_of(m)) {
        begin(motion, settled(play, lv_tick_get()));
    } /* else no memory for its state: it stays still */
}

void play_mascot(const Mascot &m, MascotMove move) { play_mascot(m, mascot_play(move, lv_tick_get())); }

MascotStageRun mascot_stage_run(MascotMove move)
{
    /* The deck's stage table (its deck_stage.cpp kActs), its numbers unchanged: a looping move's run, long enough to
     * show what it does — the work's 30 s for the sweat, which comes only after 10 s of typing; the idle in place's five
     * stretches and 3.5 s into a sixth (AGENT_DECK 9.3) — and 0 for a move that plays once. */
    switch (move) {
    case MascotMove::kRest: return {0, 0};
    case MascotMove::kJump: return {3 * theme::kMascotJumpMs, 3 * theme::kMascotJumpMs};
    case MascotMove::kWork: return {30000, 30000};
    case MascotMove::kFinish: return {theme::kMascotFinishMs, theme::kMascotFinishMs};
    case MascotMove::kCelebrate: return {theme::kMascotCelebrateMs, theme::kMascotCelebrateMs};
    case MascotMove::kIdle: return {3 * theme::kMascotIdleMs, 3 * theme::kMascotIdleMs};
    case MascotMove::kWave: return {2 * theme::kMascotWaveMs, 2 * theme::kMascotWaveMs};
    case MascotMove::kNeedsYou: return {3 * theme::kMascotWaveMs, 3 * theme::kMascotWaveMs};
    case MascotMove::kHello: return {0, static_cast<uint32_t>(kWaveHandMs)};
    case MascotMove::kSkip:
    case MascotMove::kSpin:
    case MascotMove::kDust:
    case MascotMove::kSkipTrip: return {0, reaction_ms(move)};
    case MascotMove::kFaceplant: return {0, theme::kMascotFaceplantMs};
    case MascotMove::kIdleInPlace: return {5 * 3 * theme::kMascotIdleMs + 3500, 5 * 3 * theme::kMascotIdleMs + 3500};
    }
    return {0, 0};
}

MascotPlay mascot_play_look(uint32_t now)
{
    /* The idle as if begun kLookStretch stretches ago: it is at the start of that stretch now. */
    return mascot_play(MascotMove::kIdle, now - kLookStretch * kIdleStretchMs);
}

void set_mascot_sfx(const Mascot &m, MascotSfxFn fn, void *user)
{
    if (Motion *motion = motion_of(m)) {
        motion->sfx = fn;
        motion->sfx_user = user;
    }
}

bool mascot_move_now(const Mascot &m, MascotMove *move, uint32_t *since)
{
    const Motion *motion = motion_of(m);
    if (motion == nullptr) {
        return false;
    }
    const MascotPlay now = settled(motion->play, lv_tick_get());
    *move = now.move;
    *since = now.since;
    return true;
}

bool mascot_jaunt(const Mascot &m, MascotJaunt *jaunt, uint32_t *since)
{
    const Motion *motion = motion_of(m);
    if (motion == nullptr) {
        return false;
    }
    const uint32_t now = lv_tick_get();
    const MascotPlay play = settled(motion->play, now);
    if (play.move == MascotMove::kSkip || play.move == MascotMove::kSkipTrip ||
        play.move == MascotMove::kDust) { /* a reaction: it plays to its end */
        *jaunt = play.move == MascotMove::kDust ? MascotJaunt::kDust : MascotJaunt::kSkip;
        *since = play.since;
        return true;
    }
    const uint32_t into = now - play.since;
    if (play.stopped == kStillPlaying && looping_jaunt(play, into, jaunt, since)) {
        return true;
    }
    if (play.move == MascotMove::kHello && into < static_cast<uint32_t>(kWaveHandMs)) { /* the wave's first hand */
        *jaunt = MascotJaunt::kWave;
        *since = play.since;
        return true;
    }
    /* The idle, or the idle a finish or a celebrate carries on into, played out unstopped. */
    MascotPlay idle = play;
    if (play.move == MascotMove::kFinish && play.stopped == kStillPlaying && now - play.since >= theme::kMascotFinishMs) {
        idle = idle_after(play, theme::kMascotFinishMs);
    } else if (play.move == MascotMove::kCelebrate && play.stopped == kStillPlaying &&
               now - play.since >= theme::kMascotCelebrateMs) {
        idle = idle_after(play, theme::kMascotCelebrateMs);
    } else if (play.move != MascotMove::kIdle && play.move != MascotMove::kIdleInPlace) {
        return false;
    }
    return idle_jaunt(idle, now, jaunt, since);
}

bool mascot_typing(const Mascot &m)
{
    const Motion *motion = motion_of(m);
    if (motion == nullptr) {
        return false;
    }
    const MascotPlay play = settled(motion->play, lv_tick_get());
    if (play.move != MascotMove::kWork || play.stopped != kStillPlaying) {
        return false;
    }
    const uint32_t e = lv_tick_get() - play.since;
    if (e < kTypingFromMs) {
        return false; /* still taking the laptop out */
    }
    /* The sweat's first loop types on under the beads; its second is the wipe, the hands off the keys. A faceplant is
     * no typing until it is back at the keys. */
    const int32_t sweat = sweat_at(play.since, e);
    return sweat < static_cast<int32_t>(kTypingLoopMs) && faceplant_at(play.since, e) < 0;
}

void set_mascot_colour(const Mascot &m, lv_color_t body, lv_color_t shade, lv_color_t tint)
{
    if (m.root == nullptr) {
        return;
    }
    if (Motion *motion = motion_of(m)) {
        motion->body = body;
        motion->shade = shade;
        motion->tint = tint;
        lv_obj_invalidate(m.pixels);
        lv_obj_invalidate(m.pixel_legs);
    }
    lv_obj_set_style_bg_color(m.body, body, LV_PART_MAIN);
    for (lv_obj_t *part : m.hands) {
        lv_obj_set_style_bg_color(part, body, LV_PART_MAIN);
    }
    for (lv_obj_t *part : m.feet) {
        lv_obj_set_style_bg_color(part, body, LV_PART_MAIN);
    }
    for (lv_obj_t *part : m.legs) {
        lv_obj_set_style_bg_color(part, body, LV_PART_MAIN);
    }
}

}  // namespace widgets
