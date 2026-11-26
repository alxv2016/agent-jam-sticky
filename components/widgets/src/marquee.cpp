#include "widgets/marquee.h"

#include <array>
#include <cstddef>
#include <cstring>

#include "design_tokens.h"
#include "in_psram.h"
#include "router/router.h"

// The stepped line (DESIGN → Exceptions → Stepped lines on the deck's screens; STICKY_DESIGN_TOKENS §14.6).
//
// A line is a bare box as wide as its room and one line box high, and in it a window and in that one label. A line that fits is the label alone,
// as wide as the box, aligned in it. One too long holds its words twice, a gap between, as wide as that, and a step moves the label left in the
// window, which clips it: after a pass the label stands `run` px along, where the words come round, and the next pass is the same. A line that
// has made its passes stands at its start, and its label holds the words once at the room's width, ending in LVGL's dots, as the dotted label it
// replaces does: never cut mid-word with no mark. It holds the words twice again when the line begins again. The window reaches `kBleed` px
// left of the box, so the one pixel by which a glyph's stem may stand left of its cell is drawn as it is in a dotted label: a line that fits,
// and one that stands after its passes, is pixel for pixel the label it replaces.
//
// The clock is one timer, `motion.marquee-step` apart, that steps every line that is due; a line that rests counts `motion.marquee-rest` in
// time from when it came to rest, so a line that was set between two steps still rests at least that long. The clock runs while a line is
// under it and deletes itself at a step that finds none.

namespace widgets {
namespace dt = design_tokens;

namespace {

constexpr int32_t kBleed = 1;       // the Bitcount fonts' most negative left bearing, in `caption`, `label`, `title` and `caption-strong` (`caption-regular`, `body`: none): 1 px
constexpr size_t kMaxLines = 24;    // the deck's most: a list of four rows' two lines, a page's three, and room for more
constexpr size_t kMaxOrphans = 24;  // lines deleted a moment ago whose place a screen built again may take over

// What the label holds now: the words once, aligned (they fit); twice with a gap, a pass to come or under way; or once ending in dots, the
// passes made.
enum class Shown : uint8_t { kNothing, kOnce, kTwice, kDots };

struct Line {
    lv_obj_t *root = nullptr;  // nullptr: the slot is free
    lv_obj_t *window = nullptr;
    lv_obj_t *label = nullptr;
    MarqueeOptions options;
    char *text = nullptr;
    uint32_t hash = 0;
    uint32_t since = 0;   // an lv_tick_get() time: when the line came to rest at its start
    int32_t room = -1;    // the content width the words were fitted to; -1 until a layout has given one
    int32_t run = 0;      // px of one pass; 0 while the words fit
    int32_t offset = 0;   // px the words stand left of their start
    uint32_t step = 0;    // steps made in the pass under way; 0 at its start
    bool stepping = false;
    bool stopped = false;  // after its passes, standing at its start until its words change, its control is focused, or its page is shown again
    bool focused = false;  // its control was focused or pressed at the last step
    bool holding = false;  // covered mid-pass: it rests its time again before it goes on
    bool restart_shown = false;  // a stopped reading covered: it keeps its dots, and begins again once shown
    Shown shown = Shown::kNothing;
};

struct Orphan {
    bool used = false;
    uint32_t key = 0, hash = 0, since = 0, step = 0;
    bool stopped = false, focused = false, restart_shown = false;
};

// The clock's places live in PSRAM (lvgl_display::InPsram): only the LVGL task's timer and events touch them.
std::array<Line, kMaxLines> &s_lines = lvgl_display::InPsram<std::array<Line, kMaxLines>>();
std::array<Orphan, kMaxOrphans> &s_orphans = lvgl_display::InPsram<std::array<Orphan, kMaxOrphans>>();
lv_timer_t *s_clock = nullptr;
bool s_purge_pending = false;

constexpr uint32_t kRestMs = dt::kMotionMarqueeRestDelayMs;
constexpr uint32_t kStepMs = dt::kMotionMarqueeStepDelayMs;
static_assert(kStepMs > 0 && kRestMs >= kStepMs, "a line rests at least a step");
static_assert(dt::kMarqueePasses >= 1 && dt::kMarqueeGapSpaces >= 1 && dt::kSpacingMarqueeStep > 0, "a pass moves, and the words come round after a gap");

uint32_t Hash(const char *text)
{
    uint32_t h = 2166136261u;  // FNV-1a
    for (const unsigned char *c = reinterpret_cast<const unsigned char *>(text); *c != 0; ++c) {
        h = (h ^ *c) * 16777619u;
    }
    return h;
}

char *Copy(const char *text)
{
    const size_t n = std::strlen(text) + 1;
    char *copy = static_cast<char *>(lv_malloc(n));
    if (copy != nullptr) {
        std::memcpy(copy, text, n);
    }
    return copy;
}

Line *Find(const lv_obj_t *root)
{
    for (Line &line : s_lines) {
        if (line.root == root && root != nullptr) {
            return &line;
        }
    }
    return nullptr;
}

int32_t WordsWidth(const lv_font_t *font, const char *text)
{
    lv_point_t size = {};
    lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return size.x;
}

// Steps in the passes: a pass is the run in `spacing.marquee-step` steps, the last one short; the line then rests.
uint32_t StepsInPasses(const Line &line)
{
    if (line.run <= 0) {
        return 0;
    }
    const uint32_t per_pass = static_cast<uint32_t>((line.run + dt::kSpacingMarqueeStep - 1) / dt::kSpacingMarqueeStep);
    return per_pass * static_cast<uint32_t>(dt::kMarqueePasses);
}

// Where the words stand after `step` steps: round on from where the last pass left off, the words coming round at `run`. The last step of
// the passes lands on the start.
int32_t OffsetAt(const Line &line, uint32_t step)
{
    if (step == 0 || line.run <= 0 || step >= StepsInPasses(line)) {
        return 0;
    }
    return static_cast<int32_t>((static_cast<uint64_t>(step) * static_cast<uint32_t>(dt::kSpacingMarqueeStep)) % static_cast<uint32_t>(line.run));
}

void Place(Line &line)
{
    lv_obj_set_x(line.label, kBleed - line.offset);
}

// The nearest ancestor in a focus group: the control a line stands in, or nullptr for a reading.
lv_obj_t *ControlOf(lv_obj_t *root)
{
    for (lv_obj_t *up = lv_obj_get_parent(root); up != nullptr; up = lv_obj_get_parent(up)) {
        if (lv_obj_get_group(up) != nullptr) {
            return up;
        }
    }
    return nullptr;
}

void EnsureClock(bool fresh);  // below: Fit and SetMarquee ask for the clock, and the clock fits
void Fit(Line &line);          // below: a line that begins again shows its words twice again

void Restart(Line &line, uint32_t now)
{
    line.step = 0;
    line.since = now;
    line.stopped = false;
    line.holding = false;
    line.offset = 0;
    if (line.label == nullptr) {
        return;
    }
    Place(line);
    if (line.shown == Shown::kDots && line.room >= 0) {
        Fit(line);  // the words twice in place of the dots: the line steps again
    }
}

// The label holds the words once, as wide as the room, aligned as the line was asked: they fit.
void ShowOnce(Line &line, int32_t room)
{
    lv_obj_set_width(line.label, room);
    if (line.shown != Shown::kOnce) {
        lv_label_set_long_mode(line.label, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_height(line.label, LV_SIZE_CONTENT);
        lv_label_set_text(line.label, line.text);
        lv_obj_set_style_text_align(line.label, line.options.align == MarqueeAlign::kCentre ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT, 0);
        line.shown = Shown::kOnce;
    }
}

// The label holds the words twice, a gap between, as wide as they are, for a step to move. False when there is no memory for the copy.
bool ShowTwice(Line &line)
{
    if (line.shown == Shown::kTwice) {
        return true;
    }
    const size_t n = std::strlen(line.text);
    char *twice = static_cast<char *>(lv_malloc(2 * n + static_cast<size_t>(dt::kMarqueeGapSpaces) + 1));
    if (twice == nullptr) {
        return false;
    }
    std::memcpy(twice, line.text, n);
    std::memset(twice + n, ' ', static_cast<size_t>(dt::kMarqueeGapSpaces));
    std::memcpy(twice + n + dt::kMarqueeGapSpaces, line.text, n + 1);
    lv_obj_set_width(line.label, LV_SIZE_CONTENT);
    lv_obj_set_height(line.label, LV_SIZE_CONTENT);
    lv_label_set_long_mode(line.label, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(line.label, twice);
    lv_free(twice);
    lv_obj_set_style_text_align(line.label, LV_TEXT_ALIGN_LEFT, 0);
    line.shown = Shown::kTwice;
    return true;
}

// The label holds the words once at the room's width on the box's line, ending in LVGL's dots as the dotted label it replaces does.
void ShowDots(Line &line, int32_t room)
{
    lv_obj_set_width(line.label, room);
    if (line.shown != Shown::kDots) {
        lv_obj_set_height(line.label, lv_obj_get_height(line.root));  // one line, its padding included, as `ui::FitLineBox` gives a dotted label
        lv_label_set_long_mode(line.label, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(line.label, line.text);
        lv_obj_set_style_text_align(line.label, LV_TEXT_ALIGN_LEFT, 0);
        line.shown = Shown::kDots;
    }
}

// A line too long for its room: the words in dots while it stands after its passes, twice while it steps or will. False when there is no
// memory for the words twice: the line then holds still at its start this time, and the next fit tries again.
bool FitSteps(Line &line, int32_t room)
{
    EnsureClock(false);  // the room shrank under words that fitted: the clock may have gone
    if (line.stopped) {
        ShowDots(line, room);
    } else if (!ShowTwice(line)) {
        line.stepping = false;
        return false;
    }
    if (line.step >= StepsInPasses(line)) {
        line.step = 0;  // the room changed under a pass: begin it again
    }
    line.offset = OffsetAt(line, line.step);
    return true;
}

// Fits the label to the room the box has now: the words once, aligned, when they fit; twice, a gap between, when they do not; in dots once
// their passes are made.
void Fit(Line &line)
{
    const int32_t room = lv_obj_get_content_width(line.root);
    if (room <= 0) {
        // Not laid out yet, or a parent collapsed or hidden: nothing to step, and nothing for the clock to keep alive for.
        line.stepping = false;
        return;
    }
    const lv_font_t *font = lv_obj_get_style_text_font(line.label, LV_PART_MAIN);
    const int32_t words = WordsWidth(font, line.text);
    const bool steps = words > room;
    const int32_t gap = lv_font_get_glyph_width(font, ' ', ' ') * dt::kMarqueeGapSpaces;
    line.room = room;
    lv_obj_set_width(line.window, room + kBleed);
    line.stepping = steps;
    line.run = steps ? words + gap : 0;
    if (steps) {
        if (!FitSteps(line, room)) {
            return;
        }
    } else {
        ShowOnce(line, room);
        line.step = 0;
        line.stopped = false;
        line.offset = 0;
    }
    Place(line);
}

bool AnyMoving()
{
    for (const Line &line : s_lines) {
        if (line.root != nullptr && line.stepping && line.step != 0) {
            return true;
        }
    }
    return false;
}

// A line on a screen that is not on top, or under an overlay, stands still and rests again when it is uncovered (DESIGN → Motion → Waits and
// things that update on their own). A reading that had stopped after its passes begins again when its page is shown again: it is
// made ready here, covered, to rest its time from the moment it is uncovered. A control's line begins again by its focus (FollowControl).
void StandStill(Line &line, uint32_t now)
{
    line.since = now;
    line.holding = line.step != 0;  // a pass under way stands where it is, and rests again before it goes on
    if (line.stopped && ControlOf(line.root) == nullptr) {
        line.restart_shown = true;  // not now: under a cover that leaves it in view it keeps its dots, never cut words
    }
}

// A control's line begins again when the control is focused or pressed again: the passes start from the start, resting first.
void FollowControl(Line &line, uint32_t now)
{
    const lv_obj_t *control = ControlOf(line.root);
    if (control == nullptr) {
        line.focused = false;
        return;
    }
    const bool held = lv_obj_has_state(control, static_cast<lv_state_t>(LV_STATE_FOCUSED | LV_STATE_PRESSED));
    if (held && !line.focused) {
        Restart(line, now);
    }
    line.focused = held;
}

// One step of a line that is due. The last step of the passes lands on the start, and the line stands there, reading or control: it
// begins again when its words change, its control is focused or pressed again, or its page is shown again.
void MoveOn(Line &line, uint32_t now)
{
    line.holding = false;
    ++line.step;
    if (line.step >= StepsInPasses(line)) {
        line.step = 0;
        line.since = now;
        line.stopped = true;
        line.offset = 0;
        ShowDots(line, line.room);  // the words once, ending in dots: the one change this stop draws
    } else {
        line.offset = OffsetAt(line, line.step);
    }
    Place(line);
}

void Tick(lv_timer_t *timer)
{
    const uint32_t now = lv_tick_get();
    const bool overlay = router::OverlayOpen();
    bool any = false;
    for (Line &line : s_lines) {
        if (line.root == nullptr) {
            continue;
        }
        if (line.room != lv_obj_get_content_width(line.root) && lv_obj_get_content_width(line.root) > 0) {
            Fit(line);
        }
        if (!line.stepping) {
            continue;
        }
        any = true;  // a stopped line keeps the clock too: it must see its page covered and shown again
        if (overlay || lv_obj_get_screen(line.root) != lv_screen_active()) {
            StandStill(line, now);
            continue;
        }
        if (line.restart_shown) {
            line.restart_shown = false;  // shown again after a cover: a stopped reading begins its passes again
            Restart(line, now);
        }
        FollowControl(line, now);
        if (line.stopped) {
            continue;
        }
        if ((line.step == 0 || line.holding) && lv_tick_diff(now, line.since) < kRestMs) {
            continue;  // resting with the start in view, or where a cover left it
        }
        MoveOn(line, now);
    }
    if (!any && s_clock == timer) {
        lv_timer_delete(timer);
        s_clock = nullptr;
    }
}

void EnsureClock(bool fresh)
{
    if (s_clock == nullptr) {
        s_clock = lv_timer_create(Tick, kStepMs, nullptr);
    } else if (fresh && !AnyMoving()) {
        lv_timer_reset(s_clock);  // nothing is mid-pass: a line that begins now rests its whole time before the first step
    }
}

void PurgeOrphans(void *)
{
    s_purge_pending = false;
    for (Orphan &orphan : s_orphans) {
        orphan.used = false;
    }
}

// The box lets the window's pixel out: LVGL clips a child to its parent's area plus the parent's draw-size reach.
void OnReach(lv_event_t *e)
{
    lv_event_set_ext_draw_size(e, kBleed);
}

void OnSize(lv_event_t *e)
{
    if (Line *line = Find(lv_event_get_target_obj(e))) {
        Fit(*line);
    }
}

void OnDelete(lv_event_t *e)
{
    Line *line = Find(lv_event_get_target_obj(e));
    if (line == nullptr) {
        return;
    }
    // A screen built again from its model deletes its lines and makes them at once: keep the place of this one a moment for that.
    if (line->options.key != 0) {
        for (Orphan &orphan : s_orphans) {
            if (!orphan.used) {
                orphan = {true, line->options.key, line->hash, line->since, line->step, line->stopped, line->focused,
                          line->restart_shown};
                if (!s_purge_pending) {
                    // A call LVGL could not queue leaves nothing pending, so the next deletion asks again.
                    s_purge_pending = lv_async_call(PurgeOrphans, nullptr) == LV_RESULT_OK;
                }
                break;
            }
        }
    }
    lv_free(line->text);
    *line = Line{};
}

// A free place on the clock, or nullptr when all are used.
Line *FreeLine()
{
    for (Line &line : s_lines) {
        if (line.root == nullptr) {
            return &line;
        }
    }
    return nullptr;
}

// A line the clock cannot take — all its places used, or no memory for the words — ends in dots as a plain label does, so
// the words it cannot step are never cut off with nothing to say so.
lv_obj_t *AsDots(lv_obj_t *root, lv_obj_t *window, lv_obj_t *label)
{
    LV_LOG_WARN("marquee: no place on the clock; the line ends in dots");
    lv_obj_set_pos(window, 0, 0);  // no step, so no bleed: the window is the box, and the last dot stands inside it
    lv_obj_set_pos(label, 0, 0);
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    return root;
}

// A screen built again takes over the pass of the line it replaced: the same words, or a line that counts a time. True when
// `slot` took one over; the orphan under its key is freed either way.
bool Adopt(Line *slot, const MarqueeOptions &options)
{
    bool adopted = false;
    if (options.key == 0) {
        return adopted;
    }
    for (Orphan &orphan : s_orphans) {
        if (orphan.used && orphan.key == options.key) {
            if (orphan.hash == slot->hash || options.carry) {
                slot->since = orphan.since;
                slot->step = static_cast<uint32_t>(orphan.step);
                slot->stopped = orphan.stopped;
                slot->focused = orphan.focused;
                slot->restart_shown = orphan.restart_shown;  // a covered stopped reading still begins again once shown
                adopted = true;
            }
            orphan.used = false;
        }
    }
    return adopted;
}

}  // namespace

lv_obj_t *MakeMarquee(lv_obj_t *parent, ui::Type role, const char *text, const MarqueeOptions &options)
{
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_scrollable(root, false);
    lv_obj_set_clickable(root, false);
    lv_obj_set_overflow_visible(root, true);  // the window clips, reaching a pixel past the box
    lv_obj_set_size(root, lv_pct(100), ui::LineBox(role));
    lv_obj_t *window = lv_obj_create(root);
    lv_obj_remove_style_all(window);
    lv_obj_set_scrollable(window, false);
    lv_obj_set_clickable(window, false);
    lv_obj_set_pos(window, -kBleed, 0);
    lv_obj_set_size(window, lv_pct(100), ui::LineBox(role));

    lv_obj_t *label = ui::NewLabel(window, role, text);
    lv_obj_remove_local_style_prop(label, LV_STYLE_TEXT_COLOR, 0);  // the line's colour is the box's: the words inherit it
    // Not LV_LABEL_LONG_MODE_CLIP, which clips to the label's own box and would cut the pixel a glyph's stem stands left of its cell: the window
    // clips, and the words are never wider than the label unless the line steps, when the label is as wide as they are.
    lv_obj_set_pos(label, kBleed, 0);
    ui::FitLineBox(label, role);
    lv_obj_set_style_text_align(label, options.align == MarqueeAlign::kCentre ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT, 0);

    Line *slot = FreeLine();
    if (slot == nullptr) {
        return AsDots(root, window, label);
    }
    *slot = Line{};
    slot->root = root;
    slot->window = window;
    slot->label = label;
    slot->options = options;
    slot->text = Copy(text);
    if (slot->text == nullptr) {
        *slot = Line{};
        return AsDots(root, window, label);
    }
    slot->hash = Hash(text);
    slot->since = lv_tick_get();
    const bool adopted = Adopt(slot, options);
    lv_obj_add_event_cb(root, OnReach, LV_EVENT_REFR_EXT_DRAW_SIZE, nullptr);
    lv_obj_refresh_ext_draw_size(root);
    lv_obj_add_event_cb(root, OnSize, LV_EVENT_SIZE_CHANGED, nullptr);
    lv_obj_add_event_cb(root, OnDelete, LV_EVENT_DELETE, nullptr);
    EnsureClock(!adopted);
    return root;
}

void SetMarquee(lv_obj_t *line_obj, const char *text)
{
    Line *line = Find(line_obj);
    if (line == nullptr) {
        // A line the clock could not take ends in dots (MakeMarquee): it takes new words as a plain label does.
        lv_obj_t *window = line_obj != nullptr ? lv_obj_get_child(line_obj, 0) : nullptr;
        lv_obj_t *label = window != nullptr ? lv_obj_get_child(window, 0) : nullptr;
        if (label != nullptr && lv_obj_check_type(label, &lv_label_class)) {
            lv_label_set_text(label, text);
        }
        return;
    }
    const uint32_t hash = Hash(text);
    if (hash == line->hash && std::strcmp(text, line->text) == 0) {
        return;  // the same words: a pass under way goes on
    }
    char *copy = Copy(text);
    if (copy == nullptr) {
        return;
    }
    lv_free(line->text);
    line->text = copy;
    line->hash = hash;
    line->shown = Shown::kNothing;  // the label holds the old words
    if (!line->options.carry) {
        Restart(*line, lv_tick_get());
    }
    if (line->room >= 0) {
        Fit(*line);
    } else {
        lv_label_set_text(line->label, text);
    }
    EnsureClock(!line->options.carry);
}

MarqueeProbe ProbeMarquee(const lv_obj_t *line_obj)
{
    MarqueeProbe probe;
    const Line *line = Find(line_obj);
    if (line == nullptr) {
        return probe;
    }
    probe.valid = true;
    probe.stepping = line->stepping;
    probe.resting = line->stepping && line->step == 0;
    probe.stopped = line->stepping && line->stopped;
    probe.offset = line->offset;
    probe.run = line->run;
    probe.steps = StepsInPasses(*line);
    return probe;
}

size_t MarqueeLiveCount()
{
    size_t n = 0;
    for (const Line &line : s_lines) {
        n += line.root != nullptr ? 1 : 0;
    }
    return n;
}

bool MarqueeClockRuns()
{
    return s_clock != nullptr;
}

}  // namespace widgets
