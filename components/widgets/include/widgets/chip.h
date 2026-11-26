// A chip: a rounded box as wide as its words, on the light dither; chosen is the deck's black edge, the fill unchanged
// (DESIGN → Components → The kit).

#pragma once

#include "lvgl.h"

namespace widgets {

// A box `spacing.target` high, `rounded.lg` — the deck's chip is its base radius, not a pill — in the focus group with its
// ring outside it. A `spacing.stroke.strong` edge is reserved at rest, transparent, so choosing a chip moves none of its
// neighbours. The caller owns LV_STATE_CHECKED and listens for LV_EVENT_CLICKED.
lv_obj_t *MakeChip(lv_obj_t *parent, const char *text);

// The small chip: the chip's look — the light dither, black words, `rounded.lg` — at `spacing.chip-small`, 36 px high, as wide as its
// words in `title` and `spacing.md` either side (DESIGN → Components → The kit; STICKY_DESIGN_TOKENS §14). A reading by default:
// no ring, no press, no stop in the focus group, the deck's "2 working" or an agent's state. `control` makes it a stop — a
// button in the focus group with its ring outside it, the caller listening for LV_EVENT_CLICKED, as the home view's "1 needs you" —
// and pads its hit area to `spacing.target`, the light dither with no edge when disabled. A control's parents let its ring out.
lv_obj_t *MakeSmallChip(lv_obj_t *parent, const char *text, bool control = false);

}  // namespace widgets
