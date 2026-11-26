// A list row: a title, and a supporting line under it when there is one — the kit's only list row, the Home list's and the
// shell the Wi-Fi network row is built on (DESIGN → Components → The kit).

#pragma once

#include <cstdint>

#include "lvgl.h"
#include "ui.h"

namespace widgets {

// A row `spacing.list-row-height` high, full width, `rounded.lg`, its ring outside it, no rule below: the title in `label`, one
// line, and under it the supporting line in `caption` and `text-secondary`, each ending in LVGL's three full stops when too long.
// A row with a click handler joins the focus group and is brought into view when focused; one without it (`on_click` null) is
// the shell of a composite, which joins the group itself. `on_click` runs on LV_EVENT_CLICKED with `user_data`.
// `title` is the title's role: `label` by default, `body` — 30 px regular — where a screen's names are the page's own words, as
// the agent deck's needs-you list (DESIGN → Components → The kit → List row). `supporting_role` is the supporting line's: `caption` by
// default, `caption-regular` — 20 px regular — under that list's `body` names, so the line is lighter than the name over it and the pair
// is no weight inversion. The row's height and its centring do not change, the two roles being 24 px lines.
//
// `fit` is what a line too long for the row does: end in LVGL's three full stops (`kDots`, the default, which every other user keeps), or
// step as a marquee (`kStepped`; `kSteppedCounting` when the supporting line counts a time, so new words under the same key carry the pass on)
// — each line then rests at its start after its passes, since a row is a control (widgets/marquee.h). `key` names the row across a list
// built again, `MarqueeKey(scope, owner, 0)`: the title takes it and the supporting line the next field.
enum class ListRowFit : uint8_t { kDots, kStepped, kSteppedCounting };
lv_obj_t *MakeListRow(lv_obj_t *parent, const char *label, const char *supporting, lv_event_cb_t on_click, void *user_data,
                      ui::Type title = ui::Type::kLabel, ListRowFit fit = ListRowFit::kDots, uint32_t key = 0,
                      ui::Type supporting_role = ui::Type::kCaption);
// list-row-chosen: the row inverted, `surface-chosen` with its words `text-on-fill` — never the focus, which is the ring.
void SetListRowChosen(lv_obj_t *row, bool chosen);

}  // namespace widgets
