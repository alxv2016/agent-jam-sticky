// The Wi-Fi page (AGENT_DECK 4.9; STICKY_AGENT_DECK §4.8): what the deck is on, or how to put it on the Mac's network — joined with its
// facts, not connected, the Mac's networks to pick from, the join's three steps, a step that failed, and paired. It draws the model's Wi-Fi
// record and asks the link for each change. Its route is `deck.wifi`; the foot's Wi-Fi icon and the settings' network row open it.

#pragma once

#include <memory>

#include "router/screen.h"

namespace agent_deck {

std::unique_ptr<router::Screen> WifiScreen();

}  // namespace agent_deck
