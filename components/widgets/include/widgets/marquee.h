// A stepped line: one line of words that, too long for its room, rests and then moves along itself in steps instead of ending in dots —
// the agent deck's marquee (its marquee_label), stepped for e-paper's partial refresh (DESIGN → Exceptions → Stepped lines on the deck's
// screens; STICKY_DESIGN_TOKENS §14.6).
//
// A line that fits stands still, in its alignment. One too long rests `motion.marquee-rest` with its start in view, then moves
// `spacing.marquee-step` every `motion.marquee-step` — one partial refresh a step — `kMarqueePasses` passes round, the words coming round
// after a gap of `kMarqueeGapSpaces` spaces, and stands at its start. Every line on the glass steps on ONE clock, so two lines step in the
// same frame and cost one refresh, not one each, and a step lands in whatever frame the glass draws next, the mascot's included.
//
// After its passes a line stands still at its start (DESIGN → Accessibility; WCAG 2.2.2), a reading and a line in a control alike, its words
// ending in LVGL's three full stops as the dotted label it replaces ends them — never cut mid-word with no mark — and it draws nothing more.
// It holds its words twice again, and steps, when it begins again, resting first: when its words change (SetMarquee, or a screen built
// again with new words under its key), when its page is turned to or shown again (a screen entered, a carousel page turned: its lines are
// made again, or a reading's covered screen is uncovered), and, for a line inside a control — an ancestor in a focus group, a list row —
// when the control is focused or pressed again.
// A line on a screen that is not on top, or under an overlay, stands still and rests again when it is uncovered.

#pragma once

#include <cstdint>

#include "lvgl.h"
#include "ui.h"

namespace widgets {

enum class MarqueeAlign : uint8_t { kStart, kCentre };

struct MarqueeOptions {
    MarqueeAlign align = MarqueeAlign::kStart;  // where a line that fits stands in its room; one too long starts at the left
    // The same field across a screen that is built again from its model (home on every record, a list every `motion.self-update`): the new line
    // takes over the old one's pass when the words are the same, so a pass under way goes on. 0 for a line that is made once.
    uint32_t key = 0;
    // The words count a time ("Bash • 24s"): other words under the same key carry the pass on where it is, rather than start again.
    bool carry = false;
};

// What makes a key: a screen's scope, the thing the line is about (an agent's id, a request's), and which of its fields.
constexpr uint32_t MarqueeKey(uint8_t scope, uint32_t owner, uint8_t field)
{
    return (static_cast<uint32_t>(scope & 0xF) << 28) | ((owner & 0xFFFFF) << 8) | field;
}

// One line of `role` in `text`, as wide as its parent's content (set its width to change that) and as high as the role's line box. Colour
// it through the returned object (`lv_obj_set_style_text_color`, `ui::QuietWords`): the words inherit it. Place it as any object; it is no
// control and takes no press.
lv_obj_t *MakeMarquee(lv_obj_t *parent, ui::Type role, const char *text, const MarqueeOptions &options = {});

// The line's words set again: the same words do nothing — a pass under way goes on — and other words start from their start.
// `line` is a live line MakeMarquee made; one that fell back to dots takes the words as a plain label does.
void SetMarquee(lv_obj_t *line, const char *text);

// What a line is doing, for a test or a screen that wants to know.
struct MarqueeProbe {
    bool valid = false;     // `line` is a marquee line
    bool stepping = false;  // its words do not fit: they step
    bool resting = false;   // stepping, and at its start
    bool stopped = false;   // stepping, its passes done: it stays at its start until its words change, its page is shown again or, in a control, it is focused or pressed
    int32_t offset = 0;     // px its words are moved left of their start
    int32_t run = 0;        // px one pass is: the words and the gap
    uint32_t steps = 0;     // steps in the passes before it rests again
};
MarqueeProbe ProbeMarquee(const lv_obj_t *line);

// How many lines the clock has under it, and whether the clock runs. For a test.
size_t MarqueeLiveCount();
bool MarqueeClockRuns();

}  // namespace widgets
