// The settings (AGENT_DECK 4.4; STICKY_AGENT_DECK §4.7): Sound, Auto sleep, Wi-Fi, the joined network, the time zone and the battery, in the
// kit's controls on one page, Back at the foot. Its route is `deck.settings`; the foot's Settings icon opens it.

#pragma once

#include <memory>

#include "router/screen.h"

namespace agent_deck {

std::unique_ptr<router::Screen> SettingsScreen();

}  // namespace agent_deck
