// Host test of Claude's mascot on LVGL's real objects and timers, on the computer with no board, sampled as the glass
// samples it — a frame every partial refresh, about 460 ms (MASCOT §6; STICKY_UI_KIT_PORT §4 item 8): every move the
// deck's stage plays draws only the glass's own codes — white, black and the mascot's fills, never a blended grey — at rest
// and in every sampled frame; every move tells moments; and every move, asked to rest as the deck's stage asks it, comes
// back to the rest pose. Beside it, the mascot's MascotSfx and the buzzer's Moment name the same moments in the same order
// (MASCOT §4 rule 10).
// run.sh builds LVGL's sources from managed_components (a firmware build fetches them) into build/host/lvgl, once, and the
// sources named below, against stand-ins for the ESP-IDF headers (tests/host/stubs).
// host-sources: components/ui/ui.cpp components/ui/line_box.cpp components/ui/fonts/bitcount/*.c components/widgets/src/mascot.cpp
// Build and run: bash tests/host/run.sh

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

#include <vector>
#include "src/core/lv_obj_draw_private.h"
#include "src/misc/lv_area_private.h"
#include "design_tokens.h"
#include "lvgl.h"
#include "lvgl_display.h"
#include "sounds.h"
#include "ui.h"
#include "widgets/mascot.h"
#include "../../components/widgets/src/internal/mascot_theme.h"

namespace dt = design_tokens;
using widgets::MascotMove;
using widgets::MascotSfx;
using devices::sound::Moment;

// The buzzer's moments mirror the mascot's, in order: a new MascotSfx is a new Moment, appended in both.
static_assert(static_cast<size_t>(MascotSfx::kFlop) + 1 == devices::sound::kMomentCount, "a Moment for every MascotSfx");
#define SAME(name) static_assert(static_cast<int>(MascotSfx::name) == static_cast<int>(Moment::name), #name " in order")
SAME(kBlink); SAME(kHappy); SAME(kChevron); SAME(kWink); SAME(kLook); SAME(kHop); SAME(kLand); SAME(kSway);
SAME(kStretch); SAME(kHand); SAME(kWipe); SAME(kLaptopDown); SAME(kLidOpen); SAME(kLidShut); SAME(kFlag); SAME(kBurst);
SAME(kPuff); SAME(kBead); SAME(kPop); SAME(kStep); SAME(kTurn); SAME(kHandDown); SAME(kCrouch); SAME(kTopple); SAME(kFlop);
#undef SAME

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

constexpr int kW = 480, kH = 800;
uint8_t s_buffer[kW * kH];
uint32_t s_ms = 1000;
constexpr uint32_t kFrameMs = 460;  // a partial refresh's frame on the V2.0 (STICKY_EPAPER_WAVEFORMS §4.1)
constexpr int32_t kStageTop = dt::kSpacingStatusBar + dt::kSpacingMd;
constexpr int32_t kStageHeight = 15 * dt::kSpacingMascotCell;  // mascot_page.cpp's stage

void Flush(lv_display_t *display, const lv_area_t *, uint8_t *) { lv_display_flush_ready(display); }
uint32_t Ticks() { return s_ms; }

// The codes the glass may be sent where the mascot stands: the white glass and every role's fill. The host's LVGL is
// built at 16-bit colour, so a layer it makes for a tilted part is RGB565 where the board's is L8: a code that went through
// one comes back a step or two off, which the host takes too — a blend between two codes lands nowhere near either.
std::set<int> s_allowed;
void Allow(uint8_t level)
{
    const int v = lv_color_luminance(ui::Fill(level));
    s_allowed.insert(v);
    const int r5 = v >> 3, g6 = v >> 2;
    const int via565 = lv_color_luminance(lv_color_make((r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (r5 << 3) | (r5 >> 2)));
    for (int d = -1; d <= 1; ++d) {
        s_allowed.insert(via565 + d);
    }
}

void AllowAll()
{
    for (uint8_t level : {dt::kColorBackground, dt::kColorMascotBody, dt::kColorMascotShade, dt::kColorMascotTint,
                          dt::kColorMascotLaptop, dt::kColorMascotFlag, dt::kColorMascotFlagCheck, dt::kColorMascotFlagPole,
                          dt::kColorMascotDust, dt::kColorMascotDustFaint, dt::kColorMascotSweat, dt::kColorMascotConfettiA,
                          dt::kColorMascotConfettiB}) {
        Allow(level);
    }
}

void Frame()
{
    lv_timer_handler();
    lv_refr_now(nullptr);
}

// The first value in the frame that is no code, or -1.
int Blended(int *at_x, int *at_y)
{
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            if (s_allowed.count(s_buffer[y * kW + x]) == 0) {
                *at_x = x;
                *at_y = y;
                return s_buffer[y * kW + x];
            }
        }
    }
    return -1;
}

// The frame with the eyes' rows blanked: a blink is the rest pose's own, on its clock.
std::string RestKey(const widgets::Mascot &m)
{
    lv_area_t eyes0, eyes1;
    lv_obj_get_coords(m.eyes[0], &eyes0);
    lv_obj_get_coords(m.eyes[1], &eyes1);
    std::string key(reinterpret_cast<const char *>(s_buffer), sizeof s_buffer);
    for (int y = std::min(eyes0.y1, eyes1.y1); y <= std::max(eyes0.y2, eyes1.y2); ++y) {
        for (int x = 0; x < kW; ++x) {
            key[y * kW + x] = 0;
        }
    }
    return key;
}

// The rows the ink reaches: any pixel not the glass's white.
void Reach(int *top, int *bottom)
{
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            if (s_buffer[y * kW + x] != s_buffer[0]) {
                *top = std::min(*top, y);
                *bottom = std::max(*bottom, y);
                break;
            }
        }
    }
}

// Whether a part shown under `obj` reaches past the room the root lets its children draw in — the root's box widened by
// its ext draw size, as LVGL clips a child of a parent whose overflow is visible — and the first such part's box.
bool OutsideRoom(lv_obj_t *root, lv_area_t *where)
{
    lv_area_t room;
    lv_obj_get_coords(root, &room);
    lv_area_increase(&room, lv_obj_get_ext_draw_size(root), lv_obj_get_ext_draw_size(root));
    std::vector<lv_obj_t *> todo = {root};
    while (!todo.empty()) {
        lv_obj_t *o = todo.back();
        todo.pop_back();
        for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i) {
            lv_obj_t *c = lv_obj_get_child(o, static_cast<int32_t>(i));
            if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) {
                continue;
            }
            lv_area_t a;
            lv_obj_get_coords(c, &a);
            if (!lv_area_is_in(&a, &room, 0)) {
                *where = a;
                return true;
            }
            todo.push_back(c);
        }
    }
    return false;
}

int s_moments = 0;
void CountMoment(MascotSfx, void *) { ++s_moments; }

struct Act {
    const char *name;
    MascotMove move;
};
namespace th = widgets::theme;
// The deck's stage's moves; how long each runs before it is asked to rest is the widget's own (mascot_stage_run).
const Act kActs[] = {
    {"jump", MascotMove::kJump},           {"work", MascotMove::kWork},
    {"finish", MascotMove::kFinish},       {"celebrate", MascotMove::kCelebrate},
    {"idle", MascotMove::kIdle},           {"wave", MascotMove::kWave},
    {"needs you", MascotMove::kNeedsYou},  {"hello", MascotMove::kHello},
    {"skip", MascotMove::kSkip},           {"spin", MascotMove::kSpin},
    {"dust", MascotMove::kDust},           {"faceplant", MascotMove::kFaceplant},
    {"skip trip", MascotMove::kSkipTrip},  {"idle in place", MascotMove::kIdleInPlace},
};

// A mascot where mascot_page.cpp stands it, on the whole glass: the page's stage band (kStageTop, kStageHeight) cuts off
// whatever the moves draw past it, so the reach is measured here, uncut, against the band.
widgets::Mascot StageMascot(lv_obj_t *screen)
{
    const int32_t cell = th::kMascotCell;
    widgets::Mascot m = widgets::make_mascot(screen, cell);
    lv_obj_set_pos(m.root, th::kMascotCentreX - th::kMascotCols * cell / 2, th::kMascotCentreY - th::kMascotRows * cell / 2);
    widgets::set_mascot_sfx(m, CountMoment, nullptr);
    return m;
}

// Still at rest: 30 s of frames, the eyes included, and not one changes — no blink (theme::kMascotBlinksAtRest).
void CheckStillAtRest(lv_obj_t *screen)
{
    const std::string still(reinterpret_cast<const char *>(s_buffer), sizeof s_buffer);
    bool same = true;
    for (int i = 0; i < 30000 / static_cast<int>(kFrameMs) && same; ++i) {
        s_ms += kFrameMs;
        lv_obj_invalidate(screen);
        Frame();
        same = std::string(reinterpret_cast<const char *>(s_buffer), sizeof s_buffer) == still;
    }
    Check(same, "at rest the mascot is still: no blink in 30 s of frames");
}

// What a move's pass showed, frame by frame: the first frame that blends, goes stale or cuts a part, and its ink's reach.
struct Watched {
    int blended = -1, frames = 0, stale = -1, sx = 0, sy = 0, reach_top = kH, reach_bottom = -1, cut = -1, bx = 0, by = 0;
    lv_area_t cut_at = {};
    bool home = false;
};

// Between two of the glass's frames, every 20 ms — any pose a faster glass may catch: every part shown stays inside the
// room its root lets it draw in, or LVGL cuts it off there.
void WatchBetweenFrames(lv_obj_t *screen, const widgets::Mascot &m, Watched &w)
{
    for (uint32_t t = 20; t < kFrameMs; t += 20) {
        s_ms += 20;
        lv_anim_refr_now();
        lv_obj_update_layout(screen);
        if (w.cut < 0) {
            w.cut = OutsideRoom(m.root, &w.cut_at) ? w.frames : -1;
        }
    }
    s_ms += kFrameMs - 20 * ((kFrameMs - 1) / 20);
}

// What the glass gets — only the areas LVGL marked changed, redrawn — against the whole frame redrawn: a part that moved
// past the room its parent lets it draw in leaves itself half drawn, or a ghost of where it was.
void WatchRedraw(lv_obj_t *screen, Watched &w)
{
    const std::string partial(reinterpret_cast<const char *>(s_buffer), sizeof s_buffer);
    lv_obj_invalidate(screen);
    lv_refr_now(nullptr);
    const std::string full(reinterpret_cast<const char *>(s_buffer), sizeof s_buffer);
    if (partial == full) {
        return;
    }
    w.stale = w.frames;
    size_t i = 0;
    while (partial[i] == full[i]) {
        ++i;
    }
    w.sx = static_cast<int>(i % kW);
    w.sy = static_cast<int>(i / kW);
}

// One move played as the deck's stage plays it — a once-only move asked to rest as it begins, the rest after its run —
// sampled as the glass samples it, until it is home at rest.
Watched PlayAct(lv_obj_t *screen, const widgets::Mascot &m, const Act &act, const std::string &rest)
{
    s_moments = 0;
    widgets::MascotPlay play = widgets::mascot_play(act.move, s_ms);
    const widgets::MascotStageRun run = widgets::mascot_stage_run(act.move);
    bool asked_rest = widgets::mascot_reaction(act.move) || run.run_ms == 0;
    if (asked_rest) {
        play.stopped = s_ms;
        play.next = MascotMove::kRest;
    }
    widgets::play_mascot(m, play);
    const uint32_t began = s_ms;
    Watched w;
    while (s_ms - began < 120000) {
        WatchBetweenFrames(screen, m, w);
        if (!asked_rest && s_ms - began >= run.run_ms) {
            widgets::ask_mascot(play, MascotMove::kRest, s_ms);
            widgets::play_mascot(m, play);
            asked_rest = true;
        }
        Frame();
        ++w.frames;
        if (w.blended < 0) {
            w.blended = Blended(&w.bx, &w.by);
        }
        if (w.stale < 0) {
            WatchRedraw(screen, w);
        }
        Reach(&w.reach_top, &w.reach_bottom);
        MascotMove shown;
        uint32_t since;
        if (asked_rest && widgets::mascot_move_now(m, &shown, &since) && shown == MascotMove::kRest) {
            s_ms += kFrameMs;  // the frame after: the rest pose settled
            Frame();
            w.home = RestKey(m) == rest;
            break;
        }
    }
    return w;
}

void ReportAct(const Act &act, const Watched &w)
{
    const std::string name = act.name;
    Check(w.blended < 0, name + ": every sampled frame draws only the glass's codes" +
                             (w.blended < 0 ? "" : " — got " + std::to_string(w.blended) + " at (" + std::to_string(w.bx) +
                                                       ", " + std::to_string(w.by) + ")"));
    Check(w.stale < 0, name + ": every frame drawn by its changed areas is the whole frame redrawn" +
                           (w.stale < 0 ? "" : " — frame " + std::to_string(w.stale) + " differs first at (" +
                                                   std::to_string(w.sx) + ", " + std::to_string(w.sy) + ")"));
    Check(w.cut < 0, name + ": every part stays inside the room its root draws in" +
                         (w.cut < 0 ? "" : " — after frame " + std::to_string(w.cut) + " a part spans y " +
                                               std::to_string(w.cut_at.y1) + " to " + std::to_string(w.cut_at.y2) + ", x " +
                                               std::to_string(w.cut_at.x1) + " to " + std::to_string(w.cut_at.x2)));
    Check(w.reach_top >= kStageTop && w.reach_bottom < kStageTop + kStageHeight,
          name + ": its ink stays inside the stage band, y " + std::to_string(kStageTop) + " to " +
              std::to_string(kStageTop + kStageHeight - 1) + " — it reaches y " + std::to_string(w.reach_top) + " to " +
              std::to_string(w.reach_bottom));
    Check(s_moments > 0, name + ": its movements tell moments (" + std::to_string(s_moments) + ")");
    Check(w.home, name + ": asked to rest, it comes back to the rest pose (" + std::to_string(w.frames) + " frames)");
    std::printf("  %-14s %3d frames, %3d moments, ink y %d to %d\n", act.name, w.frames, s_moments, w.reach_top,
                w.reach_bottom);
}

void TestMoves()
{
    lv_obj_t *screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen);
    lv_obj_set_scrollable(screen, false);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(screen, ui::Fill(dt::kColorBackground), 0);
    lv_screen_load(screen);
    const widgets::Mascot m = StageMascot(screen);
    widgets::play_mascot(m, MascotMove::kRest);
    lv_obj_invalidate(screen);
    Frame();
    int bx = 0, by = 0;
    Check(Blended(&bx, &by) < 0, "at rest the mascot draws only the glass's codes");
    const std::string rest = RestKey(m);
    CheckStillAtRest(screen);
    for (const Act &act : kActs) {
        ReportAct(act, PlayAct(screen, m, act, rest));
    }
}

}  // namespace

int main()
{
    lv_init();
    lv_tick_set_cb(Ticks);
    lv_display_t *display = lv_display_create(kW, kH);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_L8);
    lv_display_set_buffers(display, s_buffer, nullptr, sizeof s_buffer, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, Flush);
    lv_display_set_theme(display, nullptr);
    // As on the board, where LVGL turns antialiasing off at 8-bit colour: a tilted part is drawn nearest-neighbour.
    lv_display_set_antialiasing(display, false);
    AllowAll();
    TestMoves();
    std::printf("mascot on LVGL: %d checks, %d failed\n", s_checks, s_failures);
    return s_failures == 0 ? 0 : 1;
}
