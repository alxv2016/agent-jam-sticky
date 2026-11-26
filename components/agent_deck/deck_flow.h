// How the deck's screens lead to one another: the routes, which request opens from where, and where a request leaves to (AGENT_DECK 4.7;
// STICKY_AGENT_DECK §5). The screens call these instead of the router, so the rules are in one place; the home screen reads what
// they leave for it — the agent's page to come back on, and a request allowed, whose spin plays as home comes back.

#pragma once

#include <cstddef>
#include <cstdint>

namespace agent_deck::flow {

// The routes the deck registers (router/route.h): ids are the contract.
inline constexpr const char *kRouteWaiting = "deck.waiting";    // the needs-you list
inline constexpr const char *kRouteRequest = "deck.request";    // a permission request: param "<id>" or "<id>/list"
inline constexpr const char *kRouteQuestion = "deck.question";  // a question: the same param
inline constexpr const char *kRouteClosed = "deck.closed";      // a request closed elsewhere: param "answered" or "timed-out"
// The Wi-Fi page and the settings: the foot's icons open them, the settings' network row opens the Wi-Fi page and its Hardware test row opens
// the hardware test's home.
inline constexpr const char *kRouteWifi = "deck.wifi";
inline constexpr const char *kRouteSettings = "deck.settings";
inline constexpr const char *kRouteHardware = "hw.home";  // the hardware test's own route, registered by that feature

// The param of a request's route: its id, and `/list` when it was opened from the needs-you list.
void ParamFor(char *out, unsigned size, uint32_t id, bool from_list);
// Reads that param: the id, and whether it came from the list. False for a param that is neither.
bool ParseParam(const char *param, uint32_t *id, bool *from_list);

// Opens the oldest request or question this agent has queued, over the screen that is up: the home page's Review, the list's row.
void OpenOldestOf(uint16_t agent, bool from_list);
// The overview's count: the one waiting agent's oldest request, or the needs-you list when several wait.
void OpenWaiting();

// A request or question ended: leaves where AGENT_DECK 4.7 says. `others` as rules::LeaveFor takes it: the items still waiting besides
// this one — for Dismiss, one fewer than the queue holds. `allowed` plays the spin on the agent's page.
void Leave(uint16_t agent, bool from_list, size_t others, bool allowed);

// What home is to do as it comes back: the asking agent's page, and whether a request was allowed there. Taken once.
struct Return {
    bool pending = false;
    uint16_t agent = 0;
    bool allowed = false;
};
Return TakeReturn();
// Leaves home a page to come back on, without leaving a screen: the closed page's way back.
void SetReturn(uint16_t agent, bool allowed);
// The needs-you list's Dismiss: home comes back on the overview, not an agent.
void ClearReturn();

}  // namespace agent_deck::flow
