// The link indicator: the agent deck's bridge link, up or down, as a glyph alone — an indicator, never a button
// (DESIGN → Exceptions → The link indicator).

#pragma once

#include "lvgl.h"

namespace widgets {

// The deck's link art, its up face (a whole chain) or its down face (a broken one), in `text-primary` on the white glass: no
// box, no fill, no hit area, and no part of the focus group, so no ring ever reaches it. The object is the image itself, as big as
// the face it shows (34 × 18 up, 34 × 32 down); a row that centres it vertically keeps it centred when it swaps. The face is
// the state and never a grey alone, and the words that matter stand elsewhere on the screen.
lv_obj_t *MakeLinkIndicator(lv_obj_t *parent, bool up);
// Swaps the face. Nothing else changes: the indicator keeps its place, and draws nothing when the face is the one it shows.
void SetLinkIndicator(lv_obj_t *indicator, bool up);

}  // namespace widgets
