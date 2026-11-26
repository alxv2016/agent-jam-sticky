// The frame the glass keeps while the deck is off: the standby page's layout with the state chip `Off`, the mascot at rest, `Hold OK to
// turn on.` centred under it, and nothing else — no clock, no buttons, no foot (STICKY_AGENT_DECK §4.10).

#pragma once

#include "lvgl.h"
#include "power_leave.h"

namespace agent_deck {

// Builds the frame into `screen`, the router's root for it; power_leave's frame for the Power button.
void BuildOffFrame(lv_obj_t *screen, power_leave::How how);

}  // namespace agent_deck
