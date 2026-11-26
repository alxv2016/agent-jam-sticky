// A permission request and a question the agent asks (AGENT_DECK 4.3, 4.6; STICKY_AGENT_DECK §4.4, §4.5): the tool or the question's header,
// the agent and its place in the queue, the body or the answers, and Deny and Allow, or Dismiss and Send. Holding OK is Dismiss on each. Their
// routes are `deck.request` and `deck.question`, whose param is the record's id (flow::ParamFor).

#pragma once

#include <memory>

#include "router/screen.h"

namespace agent_deck {

// nullptr for a param that names no request or question waiting, which a Push refuses.
std::unique_ptr<router::Screen> RequestScreen(const char *param);
std::unique_ptr<router::Screen> QuestionScreen(const char *param);

}  // namespace agent_deck
