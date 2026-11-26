// Host test of the stepped line (components/widgets/src/marquee.cpp) on LVGL's real layout, label code, timers and fonts, on the
// computer with no board, on a fake clock: a line too long rests `motion.marquee-rest`, steps `spacing.marquee-step` every
// `motion.marquee-step`, makes its passes and stands at its start; two lines step in the same frame; a line stops after its passes, a
// reading and a control's alike, ending in dots as the dotted label does and drawing no more; a reading begins again when its words
// change or its page is shown again, a control's when it is focused or pressed; the same words set again do nothing and other words
// start over; a line that fits stands still, centred when asked, and draws as the dotted label did; a screen built again takes over a
// line's pass; a covered or hidden screen's lines
// stand still; and the needs-you list's row opts in while every other list row keeps its dots (DESIGN → Exceptions → Stepped lines on the
// deck's screens; STICKY_DESIGN_TOKENS §14.6).
// run.sh builds LVGL's sources from managed_components once, into build/host/lvgl, and the sources named below, against stand-ins
// for the ESP-IDF headers (tests/host/stubs).
// host-sources: components/ui/ui.cpp components/ui/line_box.cpp components/ui/fonts/bitcount/*.c components/widgets/marks/marks.c components/widgets/src/*.cpp components/widgets/src/internal/*.cpp components/router/router.cpp components/router/overlays.cpp components/router/focus.cpp components/router/touch.cpp components/router/routes.cpp components/router/screen.cpp components/router/status.cpp
// Build and run: bash tests/host/run.sh marquee

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "design_tokens.h"
#include "lvgl.h"
#include "lvgl_display.h"
#include "router/router.h"
#include "ui.h"
#include "widgets/list_row.h"
#include "widgets/marquee.h"
#include "widgets/network_row.h"

namespace dt = design_tokens;

// What the widgets take from lvgl_display, which is not linked.
namespace {
lv_group_t *s_group = nullptr;
}  // namespace
namespace lvgl_display {
lv_group_t *Group() { return s_group; }
void ClearGroup()
{
    lv_group_set_editing(s_group, false);
    lv_group_remove_all_objs(s_group);
}
void SetGroup(lv_group_t *group)
{
    if (group == nullptr) {
        return;
    }
    s_group = group;
    for (lv_indev_t *indev = lv_indev_get_next(nullptr); indev != nullptr; indev = lv_indev_get_next(indev)) {
        lv_indev_set_group(indev, group);
    }
    lv_group_set_default(group);
}
void SetBackHandler(void (*)()) {}
void SetKeyReadHook(void (*)(lv_indev_t *, lv_indev_data_t *)) {}
void SetTouchReadHook(void (*)(lv_indev_t *, lv_indev_data_t *)) {}
void SetPartialCounting(PartialCounting) {}
void NoteTyping() {}
void RequestFull() {}
void PlayCue(Cue) {}
}  // namespace lvgl_display

namespace {

using widgets::MarqueeAlign;
using widgets::MarqueeKey;
using widgets::MarqueeProbe;
using widgets::ProbeMarquee;

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

// --- LVGL on a fake clock ------------------------------------------------------------------------------------------------------

constexpr int kW = 480, kH = 800;
uint8_t s_l8[kW * kH];
uint32_t s_ms = 100000;
uint32_t Ticks() { return s_ms; }
int s_frames = 0;  // each flush of a refresh's last area: one partial refresh on the glass

void Flush(lv_display_t *display, const lv_area_t *, uint8_t *)
{
    if (lv_display_flush_is_last(display)) {
        ++s_frames;
    }
    lv_display_flush_ready(display);
}

constexpr uint32_t kSlice = 20;  // the test's clock moves this much between two looks at LVGL's timers

void Advance(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += kSlice) {
        s_ms += kSlice;
        lv_tick_inc(0);
        lv_timer_handler();
    }
}

bool StartLvgl()
{
    lv_init();
    constexpr size_t kPool = 60 * 1024, kPools = 32;
    alignas(16) static uint8_t heap[kPool * kPools];
    for (size_t i = 0; i < kPools; ++i) {
        if (lv_mem_add_pool(heap + i * kPool, kPool) == nullptr) {
            return false;
        }
    }
    lv_tick_set_cb(Ticks);
    lv_display_t *display = lv_display_create(kW, kH);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_L8);
    lv_display_set_buffers(display, s_l8, nullptr, sizeof s_l8, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display, Flush);
    lv_display_set_theme(display, nullptr);
    lv_indev_t *keys = lv_indev_create();
    lv_indev_set_type(keys, LV_INDEV_TYPE_KEYPAD);
    s_group = lv_group_create();
    lv_group_set_default(s_group);
    lv_indev_set_group(keys, s_group);
    return true;
}

lv_obj_t *s_screen = nullptr;

// A white screen of its own, loaded, with the previous one gone.
lv_obj_t *NewScreen()
{
    lv_obj_t *old = s_screen;
    s_screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_size(s_screen, kW, kH);
    lv_obj_set_style_bg_color(s_screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_screen_load(s_screen);
    if (old != nullptr) {
        lv_obj_delete(old);
    }
    Advance(kSlice);  // the async calls the old screen's lines left
    return s_screen;
}

// A line as the screens make one: the content wide, at x = spacing.edge.
lv_obj_t *Line(lv_obj_t *parent, ui::Type role, const char *text, int32_t y, const widgets::MarqueeOptions &options = {})
{
    lv_obj_t *line = widgets::MakeMarquee(parent, role, text, options);
    lv_obj_set_width(line, dt::kSpacingContent);
    lv_obj_set_pos(line, dt::kSpacingEdge, y);
    lv_obj_update_layout(parent);
    return line;
}

// A line too long for a caption row of the content's width (about 36 characters at 12 px), and one more.
const char *kLong = "Edit src/components/agent_deck/home_screen.cpp and its tests, then run the host build";
const char *kLongToo = "Bash: npm run build && npm test -- --watch=false --coverage --runInBand --silent";
const char *kShort = "Bash: npm test";

struct Change {
    uint32_t at;
    int32_t offset;
};

// What a line did over `ms`: each time its offset changed, when (from now) and to what.
std::vector<Change> Watch(lv_obj_t *line, uint32_t ms)
{
    std::vector<Change> changes;
    int32_t last = ProbeMarquee(line).offset;
    const uint32_t start = s_ms;
    for (uint32_t t = 0; t < ms; t += kSlice) {
        Advance(kSlice);
        const int32_t now = ProbeMarquee(line).offset;
        if (now != last) {
            changes.push_back({s_ms - start, now});
            last = now;
        }
    }
    return changes;
}

// The ink of the frame in a box: its left and right pixel columns, and whether any.
struct Ink {
    int x1 = 1 << 20, x2 = -1;
    bool any = false;
};
Ink InkIn(int x, int y, int w, int h)
{
    Advance(kSlice * 3);  // the display draws on its refresh timer
    Ink ink;
    for (int j = y; j < y + h && j < kH; ++j) {
        for (int i = x; i < x + w && i < kW; ++i) {
            if (s_l8[j * kW + i] < 128) {
                ink.x1 = std::min(ink.x1, i);
                ink.x2 = std::max(ink.x2, i);
                ink.any = true;
            }
        }
    }
    return ink;
}

// The width `text` takes in `caption`.
int32_t CaptionWidth(const char *text)
{
    lv_point_t size = {};
    lv_text_get_size(&size, text, ui::Font(ui::Type::kCaption), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return size.x;
}

// --- The cases -----------------------------------------------------------------------------------------------------------------

void TestTiming()
{
    std::printf("a line too long rests, steps, and stands at its start\n");
    lv_obj_t *screen = NewScreen();
    lv_obj_t *line = Line(screen, ui::Type::kCaption, kLong, 100);
    const widgets::MarqueeProbe p = ProbeMarquee(line);
    Check(p.valid && p.stepping && p.resting && p.offset == 0, "a long line steps, and begins at rest with its start in view");
    const int32_t step = dt::kSpacingMarqueeStep;
    const uint32_t per_pass = static_cast<uint32_t>((p.run + step - 1) / step);
    Check(p.steps == per_pass * dt::kMarqueePasses, "its passes are three, a pass the run in steps of spacing.marquee-step");
    Check(p.run == CaptionWidth(kLong) + 3 * lv_font_get_glyph_width(ui::Font(ui::Type::kCaption), ' ', ' ') && p.run > dt::kSpacingContent,
          "a pass is its words and a gap of three spaces: " + std::to_string(p.run) + " px");

    // The first step is 4 s on and the last of the passes p.steps - 1 s after it; the window ends a second after that landing.
    const std::vector<Change> first = Watch(line, 4 * 1000 + p.steps * 1000);
    Check(first.size() == p.steps, "it makes every step of its passes and no other move: " + std::to_string(first.size()) + " of " + std::to_string(p.steps));
    if (first.size() == p.steps && p.steps > 2) {
        Check(first[0].at >= 4000 - kSlice && first[0].at <= 4000 + kSlice, "the first step is motion.marquee-rest, 4 s, after it was made: " + std::to_string(first[0].at) + " ms");
        bool even = true, whole = true;
        for (size_t i = 1; i < first.size(); ++i) {
            const uint32_t gap = first[i].at - first[i - 1].at;
            even = even && gap >= 1000 - kSlice && gap <= 1000 + kSlice;
        }
        for (size_t i = 0; i + 1 < first.size(); ++i) {
            whole = whole && first[i].offset == static_cast<int32_t>((i + 1) * static_cast<size_t>(step)) % p.run;
        }
        Check(even, "a step comes every motion.marquee-step, 1 s");
        Check(whole, "each step is spacing.marquee-step, 48 px, on from the last, the words coming round at the run");
        Check(first.back().offset == 0, "the last step lands on the start");
        Check(ProbeMarquee(line).resting, "and the line rests there");
    }
    // After its passes a reading stands at its start, as a control's line does: it makes no more moves and draws nothing.
    const MarqueeProbe done = ProbeMarquee(line);
    Check(done.stopped && done.resting && done.offset == 0, "after its passes a reading rests at its start and stops");
    Advance(kSlice * 3);
    const int frames = s_frames;
    const std::vector<Change> again = Watch(line, 30 * 1000);
    Check(again.empty() && s_frames == frames, "and stands there, however long, drawing nothing: " + std::to_string(again.size()) + " moves");
}

void TestOneClock()
{
    std::printf("every line on one clock\n");
    lv_obj_t *screen = NewScreen();
    lv_obj_t *name = Line(screen, ui::Type::kTitle, "agent-deck-studio-with-a-very-long-folder-name", 100, {MarqueeAlign::kCentre, 0, false});
    lv_obj_t *doing = Line(screen, ui::Type::kCaption, kLong, 140);
    Advance(4000 + 2 * 1000);  // past the rest and the first steps
    int moves_name = 0, moves_doing = 0, together = 0, steps = 0, frames = 0;
    int32_t last_n = ProbeMarquee(name).offset, last_d = ProbeMarquee(doing).offset;
    for (uint32_t t = 0; t < 12 * 1000; t += kSlice) {
        const int before = s_frames;
        Advance(kSlice);
        const int32_t n = ProbeMarquee(name).offset, d = ProbeMarquee(doing).offset;
        moves_name += n != last_n;
        moves_doing += d != last_d;
        together += n != last_n && d != last_d;
        if (n != last_n || d != last_d) {
            ++steps;
            frames += s_frames - before;
        }
        last_n = n;
        last_d = d;
    }
    Check(moves_name >= 10 && moves_doing >= 10, "both lines move: " + std::to_string(moves_name) + " and " + std::to_string(moves_doing) + " steps in 12 s");
    Check(together == moves_name && together == moves_doing, "every step of one is a step of the other, in the same pass of the clock: " + std::to_string(together));
    // One partial refresh a step: the glass draws once for a step both lines make.
    Check(frames == steps, "two lines stepping cost one refresh a step, not two: " + std::to_string(frames) + " frames for " + std::to_string(steps) + " steps");
}

// Another screen over `screen` for two steps, then `screen` shown again and looked at a moment later.
void CoverAndShow(lv_obj_t *screen)
{
    lv_obj_t *over = lv_obj_create(nullptr);
    lv_screen_load(over);
    Advance(2000);
    lv_screen_load(screen);
    Advance(1000 + kSlice);  // the clock's next step sees the screen shown again and begins a stopped reading
    lv_obj_delete(over);
}

void TestControl()
{
    std::printf("a control's line stops after its passes\n");
    lv_obj_t *screen = NewScreen();
    // A row in the focus group: its line is inside a control.
    lv_obj_t *row = lv_obj_create(screen);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, dt::kSpacingContent, 64);
    lv_obj_set_pos(row, dt::kSpacingEdge, 200);
    lv_obj_t *other = lv_obj_create(screen);
    lv_obj_remove_style_all(other);
    lv_obj_set_size(other, 100, 40);
    lv_obj_set_pos(other, dt::kSpacingEdge, 300);
    lv_group_add_obj(s_group, row);
    lv_group_add_obj(s_group, other);
    lv_group_focus_obj(other);
    lv_obj_t *control_line = widgets::MakeMarquee(row, ui::Type::kCaption, kLong);
    lv_obj_set_width(control_line, lv_pct(100));
    lv_obj_t *reading = Line(screen, ui::Type::kCaption, kLong, 100);
    lv_obj_update_layout(screen);
    const uint32_t steps = ProbeMarquee(control_line).steps;
    Check(steps > 0, "the control's line is stepping");
    Watch(control_line, 4000 + steps * 1000 + 2000);
    const widgets::MarqueeProbe done = ProbeMarquee(control_line);
    Check(done.stopped && done.resting && done.offset == 0, "after its passes a control's line rests at its start and stops");
    Check(ProbeMarquee(reading).stopped && ProbeMarquee(reading).stepping, "a reading beside it stops after its passes too");
    lv_obj_delete(reading);
    Advance(kSlice * 3);
    const int frames = s_frames;
    const std::vector<Change> more = Watch(control_line, 30 * 1000);
    Check(more.empty() && s_frames == frames, "and stays there on its own, however long, drawing nothing");
    CoverAndShow(screen);
    Check(ProbeMarquee(control_line).stopped, "a control's line stays stopped when its screen is shown again: its focus begins it, below");

    // Focused again, it begins again: resting first, then its passes.
    lv_group_focus_obj(row);
    Advance(1000 + 2 * kSlice);
    Check(!ProbeMarquee(control_line).stopped, "focused again, it is no longer stopped");
    const std::vector<Change> restart = Watch(control_line, 4000 + 1000);
    Check(!restart.empty() && restart[0].at >= 3000, "it rests with its start in view before it moves: first move at " + (restart.empty() ? std::string("never") : std::to_string(restart[0].at)) + " ms");
    // A press does the same.
    Watch(control_line, steps * 1000 + 4000);
    Check(ProbeMarquee(control_line).stopped, "its passes done, it stops again");
    lv_group_focus_obj(other);
    Advance(1000 + kSlice);
    lv_obj_add_state(row, LV_STATE_PRESSED);
    Advance(1000 + 2 * kSlice);
    lv_obj_remove_state(row, LV_STATE_PRESSED);
    Check(!ProbeMarquee(control_line).stopped, "pressed, a stopped line begins again");
    // Focus arriving mid-pass takes the line back to its start.
    Watch(control_line, 4000 + 3 * 1000);
    Check(ProbeMarquee(control_line).offset != 0, "mid-pass the line is moving");
    lv_group_focus_obj(other);
    Advance(1000 + kSlice);
    lv_group_focus_obj(row);
    Advance(1000 + 2 * kSlice);
    Check(ProbeMarquee(control_line).offset == 0 && ProbeMarquee(control_line).resting, "focus arriving mid-pass takes it back to its start");
}

// How long a line takes from its making to the last step of its passes and a second more, from a state of rest.
uint32_t UntilStopped(lv_obj_t *line)
{
    return 4000 + ProbeMarquee(line).steps * 1000 + 2000;
}

// A reading made again, with its page covered and shown again, or set with other words, goes round once more.
void TestReadingBegins()
{
    std::printf("a reading begins again when its words change or its page is shown again\n");
    lv_obj_t *screen = NewScreen();
    lv_obj_t *line = Line(screen, ui::Type::kCaption, kLong, 100);
    Advance(UntilStopped(line));
    Check(ProbeMarquee(line).stopped && ProbeMarquee(line).offset == 0, "a reading's passes are done: it stands at its start");
    widgets::SetMarquee(line, kLong);
    Check(ProbeMarquee(line).stopped, "the same words set again do not begin it");
    widgets::SetMarquee(line, kLongToo);
    Check(!ProbeMarquee(line).stopped && ProbeMarquee(line).resting, "other words begin it again, resting first");
    const std::vector<Change> next = Watch(line, 4000 + 1000);
    Check(!next.empty() && next[0].at >= 3000 && next[0].at <= 4000 + 2 * 1000, "and it moves 4 s after: first move at " + (next.empty() ? std::string("never") : std::to_string(next[0].at)) + " ms");
    Advance(UntilStopped(line));
    Check(ProbeMarquee(line).stopped, "and its passes done it stops again");

    // Its screen covered and shown again: a screen entered.
    CoverAndShow(screen);
    Check(!ProbeMarquee(line).stopped && ProbeMarquee(line).resting, "a reading's screen shown again begins it again");
    const std::vector<Change> shown = Watch(line, 4000 + 1000);
    Check(!shown.empty() && shown[0].at >= 3000 && shown[0].at <= 4000 + 2 * 1000, "resting first, then moving: first move at " + (shown.empty() ? std::string("never") : std::to_string(shown[0].at)) + " ms");
    Advance(UntilStopped(line));
    Check(ProbeMarquee(line).stopped, "and it stops again after its passes");

    // A page built again from its model, under a key: the same words stay stopped, other words begin again.
    lv_obj_t *holder = lv_obj_create(screen);
    lv_obj_remove_style_all(holder);
    lv_obj_set_size(holder, kW, 200);
    const uint32_t key = MarqueeKey(1, 4, 2);
    lv_obj_t *keyed = Line(holder, ui::Type::kCaption, kLong, 20, {MarqueeAlign::kStart, key, false});
    Advance(UntilStopped(keyed));
    Check(ProbeMarquee(keyed).stopped, "a keyed reading stops after its passes");
    lv_obj_clean(holder);
    keyed = Line(holder, ui::Type::kCaption, kLong, 20, {MarqueeAlign::kStart, key, false});
    Check(ProbeMarquee(keyed).stopped, "the page built again with the same words keeps it stopped");
    lv_obj_clean(holder);
    keyed = Line(holder, ui::Type::kCaption, kLongToo, 20, {MarqueeAlign::kStart, key, false});
    Check(!ProbeMarquee(keyed).stopped && ProbeMarquee(keyed).resting, "the page built again with new words begins it again");
    // A page turned to: its lines are another key's, new and at rest.
    lv_obj_clean(holder);
    keyed = Line(holder, ui::Type::kCaption, kLong, 20, {MarqueeAlign::kStart, MarqueeKey(1, 5, 2), false});
    Advance(kSlice * 2);
    Check(!ProbeMarquee(keyed).stopped && ProbeMarquee(keyed).resting, "a page turned to has lines at rest, to go round");
    lv_obj_delete(holder);
    Advance(kSlice * 2);
}

// The label in a line's window.
lv_obj_t *LabelIn(lv_obj_t *line)
{
    return lv_obj_get_child(lv_obj_get_child(line, 0), 0);
}

// The line stands in dots: its label holds the words once, cut where the room ends with LVGL's three full stops, as wide as the room.
bool EndsInDots(lv_obj_t *line, const char *words)
{
    lv_obj_t *label = LabelIn(line);
    const std::string held = lv_label_get_text(label);
    return lv_label_get_long_mode(label) == LV_LABEL_LONG_MODE_DOTS && held.size() < std::strlen(words) && held.compare(held.size() - 3, 3, "...") == 0 &&
           std::string(words).rfind(held.substr(0, held.size() - 3), 0) == 0 && lv_obj_get_width(label) == lv_obj_get_content_width(line);
}

// The line steps, or will: its label holds the words twice with a gap between.
bool HoldsTwice(lv_obj_t *line, const char *words)
{
    lv_obj_t *label = LabelIn(line);
    const std::string held = lv_label_get_text(label), once = words;
    return lv_label_get_long_mode(label) == LV_LABEL_LONG_MODE_WRAP && held.rfind(once, 0) == 0 && held.find(once, once.size()) != std::string::npos;
}

// How many pixels of the frame differ between two boxes of the same size.
int PixelsDiffer(lv_obj_t *a, lv_obj_t *b)
{
    Advance(kSlice * 3);  // the display draws on its refresh timer
    lv_area_t first, second;
    lv_obj_get_coords(a, &first);
    lv_obj_get_coords(b, &second);
    int differing = 0;
    for (int j = 0; j < lv_area_get_height(&first); ++j) {
        for (int i = 0; i < lv_area_get_width(&first); ++i) {
            differing += s_l8[(first.y1 + j) * kW + first.x1 + i] != s_l8[(second.y1 + j) * kW + second.x1 + i];
        }
    }
    return differing;
}

// A reading that has made its passes ends in dots, never cut mid-word with no mark; it holds its words twice again, and steps, when it begins again.
void TestStoppedReadingDots()
{
    std::printf("a reading that has stopped ends in dots, and steps again when it begins again\n");
    lv_obj_t *screen = NewScreen();
    lv_obj_t *line = Line(screen, ui::Type::kCaption, kLong, 100);
    // The dotted label the line replaces, on its own line box: the same role, room and words.
    lv_obj_t *plain = ui::NewLabel(screen, ui::Type::kCaption, kLong);
    lv_label_set_long_mode(plain, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(plain, dt::kSpacingContent);
    ui::FitLineBox(plain, ui::Type::kCaption);
    lv_obj_set_pos(plain, dt::kSpacingEdge, 160);
    lv_obj_update_layout(screen);
    Check(HoldsTwice(line, kLong) && !EndsInDots(line, kLong), "a line that steps holds its words twice, and has no dots");
    Advance(UntilStopped(line));
    Check(ProbeMarquee(line).stopped && EndsInDots(line, kLong), "its passes done, the words stand once, ending in dots");
    Check(PixelsDiffer(line, plain) == 0, "and the line draws pixel for pixel the dotted label it replaces");
    // The stop is the only frame: the line draws nothing more, however long it stands.
    const int frames = s_frames;
    const std::string held = lv_label_get_text(LabelIn(line));
    Check(Watch(line, 20 * 1000).empty() && s_frames == frames && held == lv_label_get_text(LabelIn(line)), "drawing no frame at any step after it stops");
    // Its words change: it holds them twice again, rests, steps, and stops in dots again.
    widgets::SetMarquee(line, kLongToo);
    Check(!ProbeMarquee(line).stopped && ProbeMarquee(line).resting && HoldsTwice(line, kLongToo), "other words begin it again, holding them twice");
    Check(!Watch(line, 4000 + 2 * 1000).empty(), "it steps as it did");
    Advance(UntilStopped(line));
    Check(EndsInDots(line, kLongToo), "and stands in dots again after its passes");
    // Its page shown again: the screen covered by another and uncovered.
    CoverAndShow(screen);
    Check(!ProbeMarquee(line).stopped && ProbeMarquee(line).resting && HoldsTwice(line, kLongToo), "its page shown again begins it again, holding the words twice");
    Advance(UntilStopped(line));
    Check(EndsInDots(line, kLongToo), "and in dots once more");
    // Under an overlay a stopped reading keeps its dots, and begins again only once it is shown.
    {
        router::Overlay over;
        Check(router::OpenOverlay(&over) != nullptr, "an overlay opens over the stopped line");
        Advance(3 * 1000);
        Check(ProbeMarquee(line).stopped && EndsInDots(line, kLongToo), "under it the stopped reading keeps its dots, never cut words");
        router::CloseOverlay(&over);
        Advance(1000 + kSlice);
        Check(!ProbeMarquee(line).stopped && HoldsTwice(line, kLongToo), "shown again, it begins its passes again");
        Advance(UntilStopped(line));
    }
    // A page built again from its model keeps the stop, in dots, for the same words.
    lv_obj_t *holder = lv_obj_create(screen);
    lv_obj_remove_style_all(holder);
    lv_obj_set_size(holder, kW, 200);
    const uint32_t key = MarqueeKey(1, 6, 2);
    lv_obj_t *keyed = Line(holder, ui::Type::kCaption, kLong, 20, {MarqueeAlign::kStart, key, false});
    Advance(UntilStopped(keyed));
    lv_obj_clean(holder);
    keyed = Line(holder, ui::Type::kCaption, kLong, 20, {MarqueeAlign::kStart, key, false});
    Check(ProbeMarquee(keyed).stopped && EndsInDots(keyed, kLong), "a page built again with the same words keeps the line standing in dots");
    lv_obj_clean(holder);
    keyed = Line(holder, ui::Type::kCaption, kLongToo, 20, {MarqueeAlign::kStart, key, false});
    Check(!ProbeMarquee(keyed).stopped && HoldsTwice(keyed, kLongToo), "and with new words it holds them twice");
    lv_obj_delete(holder);
    Advance(kSlice * 2);
}

// A control's line ends in dots while it stands, and holds its words twice again when its control is focused or pressed.
void TestStoppedControlDots()
{
    std::printf("a control's line that has stopped ends in dots, and steps again on focus or press\n");
    lv_obj_t *screen = NewScreen();
    lv_obj_t *row = lv_obj_create(screen);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, dt::kSpacingContent, 64);
    lv_obj_set_pos(row, dt::kSpacingEdge, 200);
    lv_obj_t *other = lv_obj_create(screen);
    lv_obj_remove_style_all(other);
    lv_obj_set_size(other, 100, 40);
    lv_obj_set_pos(other, dt::kSpacingEdge, 300);
    lv_group_remove_all_objs(s_group);
    lv_group_add_obj(s_group, row);
    lv_group_add_obj(s_group, other);
    lv_group_focus_obj(other);
    lv_obj_t *line = widgets::MakeMarquee(row, ui::Type::kCaption, kLong);
    lv_obj_set_width(line, lv_pct(100));
    lv_obj_update_layout(screen);
    Advance(UntilStopped(line));
    Check(ProbeMarquee(line).stopped && EndsInDots(line, kLong), "a control's line that has made its passes ends in dots");
    const int frames = s_frames;
    Advance(20 * 1000);
    Check(s_frames == frames, "and draws no frame while it stands");
    lv_group_focus_obj(row);
    Advance(1000 + 2 * kSlice);
    Check(!ProbeMarquee(line).stopped && ProbeMarquee(line).resting && HoldsTwice(line, kLong), "focused, it holds its words twice again and rests first");
    Check(!Watch(line, 4000 + 2 * 1000).empty(), "and steps");
    Advance(UntilStopped(line));
    Check(EndsInDots(line, kLong), "then stands in dots again");
    lv_group_focus_obj(other);
    Advance(1000 + kSlice);
    lv_obj_add_state(row, LV_STATE_PRESSED);
    Advance(1000 + 2 * kSlice);
    lv_obj_remove_state(row, LV_STATE_PRESSED);
    Check(!ProbeMarquee(line).stopped && HoldsTwice(line, kLong) && !EndsInDots(line, kLong), "pressed, the dots give way to the words twice");
    // A words change on a stopped control's line, as a carried line's count does: the new words end in dots at once.
    Advance(UntilStopped(line));
    widgets::SetMarquee(line, kLongToo);
    Check(HoldsTwice(line, kLongToo), "new words on a control's line hold twice and start over");
}

void TestSetAgain()
{
    std::printf("set again\n");
    lv_obj_t *screen = NewScreen();
    lv_obj_t *line = Line(screen, ui::Type::kCaption, kLong, 100);
    Advance(4000 + 3 * 1000 + kSlice);
    const int32_t at = ProbeMarquee(line).offset;
    Check(at != 0, "a pass is under way");
    widgets::SetMarquee(line, kLong);
    Check(ProbeMarquee(line).offset == at, "the same words set again do nothing: the pass goes on");
    Advance(1000);
    Check(ProbeMarquee(line).offset == (at + dt::kSpacingMarqueeStep) % ProbeMarquee(line).run, "and the next step follows from where it was");
    widgets::SetMarquee(line, kLongToo);
    Check(ProbeMarquee(line).offset == 0 && ProbeMarquee(line).resting, "other words start from their start");
    const std::vector<Change> next = Watch(line, 5200);
    Check(!next.empty() && next[0].at >= 3000, "and rest before they move");
    widgets::SetMarquee(line, kShort);
    Check(!ProbeMarquee(line).stepping && ProbeMarquee(line).offset == 0, "words that fit stand still");
    widgets::SetMarquee(line, kLong);
    Check(ProbeMarquee(line).stepping, "and long words step again");
}

void TestFits()
{
    std::printf("a line that fits stands still\n");
    lv_obj_t *screen = NewScreen();
    lv_obj_t *start = Line(screen, ui::Type::kCaption, kShort, 100);
    lv_obj_t *centred = Line(screen, ui::Type::kTitle, "studio", 200, {MarqueeAlign::kCentre, 0, false});
    Advance(1000 + kSlice);
    Check(!ProbeMarquee(start).stepping && !ProbeMarquee(centred).stepping, "short words do not step");
    Check(!widgets::MarqueeClockRuns(), "and with nothing to step the clock is gone");
    const int frames = s_frames;
    Advance(20 * 1000);
    Check(s_frames == frames, "a screen of lines that fit draws nothing on its own");
    const Ink ink = InkIn(0, 200, kW, 40);
    Check(ink.any && std::abs((ink.x1 + ink.x2) / 2 - kW / 2) <= 12, "a centred line that fits stays centred on the glass: ink " + std::to_string(ink.x1) + " to " + std::to_string(ink.x2));
    const Ink left = InkIn(0, 100, kW, 24);
    Check(left.any && left.x1 >= dt::kSpacingEdge - 1 && left.x1 <= dt::kSpacingEdge + 3, "an aligned-to-start line begins at its room's edge");

    // Centred and too long: it starts at the left, as every long line does, and stays inside its room as it moves.
    lv_obj_t *name = Line(screen, ui::Type::kTitle, "agent-deck-studio-with-a-very-long-folder-name", 300, {MarqueeAlign::kCentre, 0, false});
    Check(ProbeMarquee(name).stepping, "a centred name too long steps");
    const Ink at_rest = InkIn(0, 300, kW, 36);
    Check(at_rest.any && at_rest.x1 >= dt::kSpacingEdge - 1 && at_rest.x1 <= dt::kSpacingEdge + 3, "it rests with its start at the room's left");
    Advance(4000 + 3 * 1000);
    const Ink moving = InkIn(0, 300, kW, 36);
    Check(ProbeMarquee(name).offset > 0 && moving.any && moving.x1 >= dt::kSpacingEdge - 1 && moving.x2 < dt::kSpacingEdge + dt::kSpacingContent,
          "mid-pass its words stay inside its room, clipped at both ends: ink " + std::to_string(moving.x1) + " to " + std::to_string(moving.x2));
}

void TestRebuild()
{
    std::printf("a screen built again\n");
    lv_obj_t *screen = NewScreen();
    lv_obj_t *holder = lv_obj_create(screen);
    lv_obj_remove_style_all(holder);
    lv_obj_set_size(holder, kW, 200);
    const uint32_t key = MarqueeKey(1, 7, 2);
    lv_obj_t *line = Line(holder, ui::Type::kCaption, kLong, 20, {MarqueeAlign::kStart, key, false});
    Advance(4000 + 3 * 1000 + kSlice);
    const int32_t at = ProbeMarquee(line).offset;
    Check(at != 0, "a pass is under way");
    // Home builds its page again on every record: delete and make at once, the same words.
    lv_obj_clean(holder);
    line = Line(holder, ui::Type::kCaption, kLong, 20, {MarqueeAlign::kStart, key, false});
    Check(ProbeMarquee(line).offset == at, "a line made again with the same key and words takes the pass over");
    Advance(1000);
    Check(ProbeMarquee(line).offset == (at + dt::kSpacingMarqueeStep) % ProbeMarquee(line).run, "and goes on from there on the clock's next step");
    // Other words under the key start over.
    lv_obj_clean(holder);
    line = Line(holder, ui::Type::kCaption, kLongToo, 20, {MarqueeAlign::kStart, key, false});
    Check(ProbeMarquee(line).offset == 0 && ProbeMarquee(line).resting, "other words under the key start from their start");
    // A line that counts a time carries on through new words.
    const uint32_t counting = MarqueeKey(2, 9, 2);
    line = Line(holder, ui::Type::kCaption, "previous chat \xE2\x80\xA2 Explore \xE2\x80\xA2 Bash \xE2\x80\xA2 a long fact line of a row 19s", 60,
                {MarqueeAlign::kStart, counting, true});
    Advance(4000 + 3 * 1000 + kSlice);
    const int32_t counted = ProbeMarquee(line).offset;
    lv_obj_delete(line);
    line = Line(holder, ui::Type::kCaption, "previous chat \xE2\x80\xA2 Explore \xE2\x80\xA2 Bash \xE2\x80\xA2 a long fact line of a row 24s", 60,
                {MarqueeAlign::kStart, counting, true});
    Check(counted != 0 && ProbeMarquee(line).offset == counted % ProbeMarquee(line).run, "a line that counts a time carries the pass on through new words");
    // The place is held for the one rebuild only.
    lv_obj_delete(line);
    Advance(kSlice * 2);
    line = Line(holder, ui::Type::kCaption, "previous chat \xE2\x80\xA2 Explore \xE2\x80\xA2 Bash \xE2\x80\xA2 a long fact line of a row 24s", 60,
                {MarqueeAlign::kStart, counting, true});
    Check(ProbeMarquee(line).offset == 0 && ProbeMarquee(line).resting, "a line made a pass of the timers later starts from its start");
}

void TestStandStill()
{
    std::printf("a screen not on top, and an overlay\n");
    lv_obj_t *screen = NewScreen();
    lv_obj_t *line = Line(screen, ui::Type::kCaption, kLong, 100);
    Advance(4000 + 2 * 1000 + kSlice);
    const int32_t at = ProbeMarquee(line).offset;
    Check(at != 0, "a pass is under way");
    lv_obj_t *over = lv_obj_create(nullptr);
    lv_screen_load(over);
    Advance(20 * 1000);
    Check(ProbeMarquee(line).offset == at, "a screen no longer on top stands still");
    lv_screen_load(screen);
    Advance(2000);
    Check(ProbeMarquee(line).offset == at, "and rests again before it moves when it is back");
    Advance(4000);
    Check(ProbeMarquee(line).offset != at, "then goes on");
    lv_obj_delete(over);

    // An overlay stands over the screen: the screen's lines stand still while it is open.
    router::Overlay overlay;
    const int32_t before = ProbeMarquee(line).offset;
    lv_group_t *group = router::OpenOverlay(&overlay);
    Check(group != nullptr && router::OverlayOpen(), "an overlay opens");
    Advance(12 * 1000);
    Check(ProbeMarquee(line).offset == before, "a line under an overlay stands still");
    router::CloseOverlay(&overlay);
    Advance(2000);
    Check(ProbeMarquee(line).offset == before, "and rests before it moves when the overlay closes");
    Advance(4000);
    Check(ProbeMarquee(line).offset != before, "then goes on");
}

void TestClockAndRoom()
{
    std::printf("the clock and its room\n");
    lv_obj_t *screen = NewScreen();
    lv_obj_t *line = Line(screen, ui::Type::kCaption, kLong, 100);
    Check(widgets::MarqueeClockRuns() && widgets::MarqueeLiveCount() == 1, "a stepping line runs the clock");
    lv_obj_delete(line);
    Advance(2 * 1000 + kSlice);
    Check(!widgets::MarqueeClockRuns() && widgets::MarqueeLiveCount() == 0, "with no line left the clock deletes itself");

    // More lines than the clock holds: the extra ones end in dots, as a plain label does, and nothing breaks.
    std::vector<lv_obj_t *> lines;
    for (int i = 0; i < 30; ++i) {
        lines.push_back(Line(screen, ui::Type::kCaption, kLong, 10 + i * 24));
    }
    Check(widgets::MarqueeLiveCount() == 24, "the clock holds 24 lines");
    Check(!ProbeMarquee(lines.back()).valid && lv_obj_get_child_count(lines.back()) == 1, "a line past the 24th is plain and never steps");
    {
        lv_obj_t *label = lv_obj_get_child(lv_obj_get_child(lines.back(), 0), 0);
        Check(label != nullptr && lv_label_get_long_mode(label) == LV_LABEL_LONG_MODE_DOTS, "and ends in dots: its words are never cut off with no sign");
        lv_area_t box, words;
        lv_obj_get_coords(lines.back(), &box);
        lv_obj_get_coords(label, &words);
        Check(words.x1 >= box.x1 && words.x2 <= box.x2, "its dots stand inside its box");
        widgets::SetMarquee(lines.back(), "Short");
        Check(std::string(lv_label_get_text(label)) == "Short", "and it takes new words as a plain label does");
    }
    Advance(10 * 1000);
    for (lv_obj_t *l : lines) {
        lv_obj_delete(l);
    }
    Advance(2 * 1000 + kSlice);
    Check(widgets::MarqueeLiveCount() == 0, "every line is forgotten when it is deleted");

    // A line whose room grows to fit its words stops stepping, and one whose room shrinks starts.
    lv_obj_t *grow = Line(screen, ui::Type::kCaption, kLong, 100);
    Check(ProbeMarquee(grow).stepping, "too long for 432 px");
    lv_obj_set_width(grow, kW);
    lv_obj_update_layout(screen);
    Check(ProbeMarquee(grow).stepping == (CaptionWidth(kLong) > kW), "its room changed, it is fitted again");
    lv_obj_set_width(grow, CaptionWidth(kLong));
    lv_obj_update_layout(screen);
    Check(!ProbeMarquee(grow).stepping, "in a room as wide as its words it stands still");
    lv_obj_set_width(grow, 200);
    lv_obj_update_layout(screen);
    Check(ProbeMarquee(grow).stepping && widgets::MarqueeClockRuns(), "and in a narrower one it steps, the clock running again");
}

// What a list row's lines look like when they fit, against the dotted row it replaces: the same pixels.
void TestListRow()
{
    std::printf("the list row's opt-in\n");
    lv_obj_t *screen = NewScreen();
    lv_obj_t *column = lv_obj_create(screen);
    lv_obj_remove_style_all(column);
    lv_obj_set_size(column, dt::kSpacingContent, kH);
    lv_obj_set_pos(column, dt::kSpacingEdge, 0);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(column, dt::kSpacingItem, 0);
    lv_group_remove_all_objs(s_group);

    lv_obj_t *dotted = widgets::MakeListRow(column, "agent-deck-studio", "Bash \xE2\x80\xA2 24s", nullptr, nullptr, ui::Type::kBody);
    lv_obj_t *stepped = widgets::MakeListRow(column, "agent-deck-studio", "Bash \xE2\x80\xA2 24s", nullptr, nullptr, ui::Type::kBody,
                                             widgets::ListRowFit::kSteppedCounting, MarqueeKey(2, 1, 1));
    lv_obj_update_layout(screen);
    Check(lv_obj_get_height(dotted) == dt::kSpacingListRowHeight && lv_obj_get_height(stepped) == dt::kSpacingListRowHeight,
          "both rows are 80 px");
    lv_area_t a, b;
    lv_obj_get_coords(dotted, &a);
    lv_obj_get_coords(stepped, &b);
    const Ink ink_a = InkIn(a.x1, a.y1, lv_area_get_width(&a), lv_area_get_height(&a));
    const Ink ink_b = InkIn(b.x1, b.y1, lv_area_get_width(&b), lv_area_get_height(&b));
    bool same = ink_a.any && ink_b.any && ink_a.x1 == ink_b.x1 && ink_a.x2 == ink_b.x2;
    int differing = 0;
    for (int j = 0; j < lv_area_get_height(&a); ++j) {
        for (int i = 0; i < lv_area_get_width(&a); ++i) {
            differing += s_l8[(a.y1 + j) * kW + a.x1 + i] != s_l8[(b.y1 + j) * kW + b.x1 + i];
        }
    }
    same = same && differing == 0;
    Check(same, "a row whose lines fit draws pixel for pixel as the dotted row did");
    Check(widgets::MarqueeLiveCount() == 2 && !ProbeMarquee(lv_obj_get_child(stepped, 0)).stepping,
          "the opted-in row's two lines are the clock's, and stand still; the default row made none");

    // Too long, in a row in the group: a control, so both lines step together and stop after their passes.
    lv_obj_t *row = widgets::MakeListRow(column, kLong, "previous chat \xE2\x80\xA2 Explore \xE2\x80\xA2 Bash \xE2\x80\xA2 a fact line too long for a row 24s",
                                         [](lv_event_t *) {}, nullptr, ui::Type::kBody, widgets::ListRowFit::kSteppedCounting, MarqueeKey(2, 2, 1));
    lv_obj_set_style_pad_left(row, dt::kSpacingMd + 60 + dt::kSpacingMd, 0);
    lv_obj_update_layout(screen);
    lv_obj_t *title = lv_obj_get_child(row, 0), *facts = lv_obj_get_child(row, 1);
    Check(ProbeMarquee(title).stepping && ProbeMarquee(facts).stepping, "a long name and long facts step");
    Check(lv_obj_get_width(title) == lv_obj_get_content_width(row) && lv_obj_get_width(title) < dt::kSpacingContent - dt::kSpacingMd,
          "in the room the row's padding leaves, which the avatar narrows");
    Advance(4000 + 1000 + kSlice * 2);
    Check(ProbeMarquee(title).offset != 0 && ProbeMarquee(facts).offset != 0, "both lines have made their first step");
    const uint32_t steps = std::max(ProbeMarquee(title).steps, ProbeMarquee(facts).steps);
    Advance(steps * 1000 + 2000);
    Check(ProbeMarquee(title).stopped && ProbeMarquee(facts).stopped && ProbeMarquee(title).offset == 0 && ProbeMarquee(facts).offset == 0,
          "after their passes both lines rest at their start: a row is a control");
    lv_obj_delete(row);
    lv_obj_delete(dotted);
    lv_obj_delete(stepped);
    Advance(kSlice * 2);
}

// The network row's name: stepped on request, as the dotted row where it fits.
void TestNetworkRow()
{
    std::printf("the network row's opt-in\n");
    lv_obj_t *screen = NewScreen();
    lv_obj_t *column = lv_obj_create(screen);
    lv_obj_remove_style_all(column);
    lv_obj_set_size(column, dt::kSpacingContent, kH);
    lv_obj_set_pos(column, dt::kSpacingEdge, 0);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(column, dt::kSpacingItem, 0);
    lv_obj_set_style_pad_top(column, dt::kSpacingXl, 0);  // the rows' corners stand clear of the glass's edge
    lv_group_remove_all_objs(s_group);
    using widgets::NetworkLook;
    using widgets::NetworkName;
    using widgets::NetworkState;
    lv_obj_t *dotted = widgets::MakeNetworkRow(column, "Studio-Guest", true, 2, NetworkState::kConnected, NetworkLook::kOutlined);
    lv_obj_t *stepped = widgets::MakeNetworkRow(column, "Studio-Guest", true, 2, NetworkState::kConnected, NetworkLook::kOutlined,
                                                NetworkName::kStepped, MarqueeKey(4, 1, 2));
    lv_obj_t *long_row = widgets::MakeNetworkRow(column, "Studio-Upstairs-Guest-5G-Annex-2", true, 3, NetworkState::kRest, NetworkLook::kInverted,
                                                 NetworkName::kStepped, MarqueeKey(4, 2, 2));
    lv_obj_update_layout(screen);
    for (lv_obj_t *row : {dotted, stepped, long_row}) {
        lv_obj_remove_state(row, static_cast<lv_state_t>(LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY));  // the group focused the first; no ring in the compare
    }
    lv_area_t a, b;
    lv_obj_get_coords(dotted, &a);
    lv_obj_get_coords(stepped, &b);
    int differing = 0;
    Advance(kSlice * 3);
    for (int j = 0; j < lv_area_get_height(&a); ++j) {
        for (int i = 0; i < lv_area_get_width(&a); ++i) {
            differing += s_l8[(a.y1 + j) * kW + a.x1 + i] != s_l8[(b.y1 + j) * kW + b.x1 + i];
        }
    }
    Check(differing == 0 && lv_obj_get_height(stepped) == dt::kSpacingListRowHeight, "a stepped name that fits draws the dotted row's pixels: " + std::to_string(differing) + " differ");
    lv_obj_t *name = lv_obj_get_child(long_row, 0);
    Check(ProbeMarquee(name).stepping && lv_obj_get_width(name) == dt::kSpacingContent - 2 * dt::kSpacingMd - 40 - dt::kSpacingMd,
          "a name of 32 characters steps in the row's room of 344 px, clear of the signal glyph");
    Advance(4000 + 1000 + kSlice * 2);
    Check(ProbeMarquee(name).offset != 0, "after the rest it steps");
    widgets::SetNetworkRow(long_row, true, 1, NetworkState::kConnecting);
    Check(lv_obj_get_child(long_row, 0) == name && ProbeMarquee(name).offset != 0, "a row's facts changing leaves its name's pass under way");
    lv_obj_delete(dotted);
    lv_obj_delete(stepped);
    lv_obj_delete(long_row);
    Advance(kSlice * 2);
}

}  // namespace

int main()
{
    if (!StartLvgl()) {
        std::printf("FAIL  LVGL did not start\n");
        return 1;
    }
    if (ui::Start() != ESP_OK) {
        std::printf("FAIL  ui::Start failed\n");
        return 1;
    }
    TestTiming();
    TestOneClock();
    TestControl();
    TestSetAgain();
    TestReadingBegins();
    TestStoppedReadingDots();
    TestStoppedControlDots();
    TestFits();
    TestRebuild();
    TestStandStill();
    TestClockAndRoom();
    TestListRow();
    TestNetworkRow();
    std::printf("stepped line on LVGL: %d checks, %d failed\n", s_checks, s_failures);
    return s_failures == 0 ? 0 : 1;
}
