// A status block, the deck's: a mark beside a heading in label over a line in caption — an empty or a done state
// (DESIGN → Components → The kit).

#pragma once

#include <cstdint>

#include "lvgl.h"

namespace widgets {

// The deck's three status marks, compiled at one bit into this component (marks/make_marks.py); Agent Jam Sticky has no icon
// set. A block with kNone leaves the slot empty: its words stand `spacing.lg` in, where they would after a mark.
// kWifi is the deck's Wi-Fi art, 81 px wide: wider than the slot, so only a stacked block takes it (a beside block with it shows no mark).
enum class StatusMark : uint8_t { kNone, kCheck, kClock, kLinkOff, kWifi };

// The mark's ink flush with the block's left edge, `spacing.lg` before the words, the pair centred on each other; full
// width. Not a control. The heading's words say what the mark means.
lv_obj_t *MakeStatusBlock(lv_obj_t *parent, const char *heading, const char *description,
                          StatusMark mark = StatusMark::kNone);

// The same block stacked and centred: the mark, `spacing.item` under it the heading in `label`, and `spacing.pair-md` under that, between
// their own boxes, the description in `body` — each centred across the block, which is as wide as its parent, and the words wrapping
// inside it. There is no slot: the mark stands at its own size (the Wi-Fi's 81 px as well). Not a control. A screen that holds one
// block alone centres it on the glass's height itself.
lv_obj_t *MakeStackedStatusBlock(lv_obj_t *parent, const char *heading, const char *description,
                                 StatusMark mark = StatusMark::kNone);

}  // namespace widgets
