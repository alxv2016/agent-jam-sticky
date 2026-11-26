// Words one size down: a control's words set to a smaller role of the type scale (DESIGN → Components → The kit → Words one size down).

#pragma once

#include "lvgl.h"
#include "ui.h"

namespace widgets {

// Brings every label under `obj` — `obj` itself too — that is set in a larger size than `role` down to `role`: its font, its
// line space and its line box (`ui::FitLineBox`), so the words stay centred on their line box and the row they stand in stays
// centred on its height. A label already at `role`'s size or smaller is left as it is, so a toggle row's, a value row's, a
// dropdown's, a radio row's, a list row's or a choice card's name, state word and value come down to `caption` or `caption-strong`
// while a `caption` supporting line, a reason or a hidden label stays. One call is one choice for the whole control; it is a
// role of the scale, never a size. Call it after the control is made and its words set, and once: later words set on a label
// keep its role. A gap held between two labels by a negative margin (internal/glue.h) is kept. A name over its line in a column — a choice
// card's, a list row's, a detail row's — stands `pair-sm` between their own boxes after the call, whichever role either has come to
// (DESIGN → Layout → Spacing): the leading the two line boxes leave is made up in the name's margin, so a row may grow by a few
// pixels about its centre. Under lvgl_display::Lock(), as every call into LVGL.
void SetWordsRole(lv_obj_t *obj, ui::Type role);

// Sets one label to `role` whatever its size was — SetWordsRole only brings words down — with its line space and line box as that call
// gives them: a detail row's label at `caption-strong`, the size it had (DESIGN → Components → The kit → Detail row). Call it before
// SetWordsRole on the row, which makes the gap between the label and its value up from the line boxes as they stand.
void SetLabelRole(lv_obj_t *label, ui::Type role);

}  // namespace widgets
