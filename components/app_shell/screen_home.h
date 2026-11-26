// The root screen. The router calls Build under the LVGL lock, into the root it made, with home's focus group bound: it styles
// the root as the glass, and lets each feature add to it (app_hooks::OnHomeBuilt) — a control made then joins home's group. A
// feature that claimed home (app_hooks::ClaimHome) draws all of it: Make gives the screen of whichever home the app has.

#pragma once

#include <memory>

#include "lvgl.h"
#include "router/screen.h"

namespace screen_home {

void Build(lv_obj_t *home, lv_group_t *group, void *user);

// The home screen: the claimed one, or the shell's own (Build).
std::unique_ptr<router::Screen> Make();

}  // namespace screen_home
