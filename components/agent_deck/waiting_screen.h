// The needs-you list: who waits, the oldest request first, four to a page — a row opens that agent's oldest request over the list
// (AGENT_DECK 4.7; STICKY_AGENT_DECK §4.3). Its route is `deck.waiting`.

#pragma once

#include <memory>

#include "router/screen.h"

namespace agent_deck {

std::unique_ptr<router::Screen> WaitingScreen();

}  // namespace agent_deck
