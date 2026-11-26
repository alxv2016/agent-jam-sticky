// A detail row: a fact's name in `caption` over its value in `label`; never focused (DESIGN → Components → The kit).
// Both wrap and are never cut — no press is left to show the rest — so the row grows 24 px a name line and 36 px a value line.

#pragma once

#include "lvgl.h"

namespace widgets {

struct DetailRow {
    lv_obj_t *root;
    lv_obj_t *label;
    lv_obj_t *value;
};

// A screen that sets its facts one size under `label` calls SetDetailRowOneSizeDown (DESIGN → Components → The kit → Detail row): the name
// in `caption-strong` over its value in `caption`, the two still `pair-sm` apart between their own boxes. A detail row left alone is the
// kit's: the name in `caption` over the value in `label`.
DetailRow MakeDetailRow(lv_obj_t *parent, const char *name, const char *value);
void SetDetailRow(const DetailRow &row, const char *value);
void SetDetailRowOneSizeDown(const DetailRow &row);

}  // namespace widgets
