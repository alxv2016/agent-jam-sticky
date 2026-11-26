#include "deck_flow.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "deck_model.h"
#include "deck_rules.h"
#include "router/router.h"

namespace agent_deck::flow {
namespace {

Return s_return;

// Whether `route` can be pushed: a press on a foot icon whose page is not registered yet does nothing, and logs nothing.
bool Pushable(const char *route)
{
    return router::FindRoute(route) != nullptr;
}

}  // namespace

void ParamFor(char *out, unsigned size, uint32_t id, bool from_list)
{
    std::snprintf(out, size, from_list ? "%u/list" : "%u", static_cast<unsigned>(id));
}

bool ParseParam(const char *param, uint32_t *id, bool *from_list)
{
    if (param == nullptr || param[0] < '0' || param[0] > '9') {
        return false;
    }
    char *end = nullptr;
    const unsigned long value = std::strtoul(param, &end, 10);
    if (end == param || value == 0) {
        return false;
    }
    if (*end == '\0') {
        *from_list = false;
    } else if (std::strcmp(end, "/list") == 0) {
        *from_list = true;
    } else {
        return false;
    }
    *id = static_cast<uint32_t>(value);
    return true;
}

void OpenOldestOf(uint16_t agent, bool from_list)
{
    const model::Item *item = model::OldestFor(agent);
    if (item == nullptr) {
        return;
    }
    char param[16];
    ParamFor(param, sizeof param, item->id, from_list);
    router::PushRoute(item->is_question ? kRouteQuestion : kRouteRequest, param);
}

void OpenWaiting()
{
    uint16_t queued[model::kMaxRequests] = {};
    size_t n = 0;
    for (size_t i = 0; i < model::QueueCount() && n < model::kMaxRequests; ++i) {
        queued[n++] = model::QueueAt(i)->agent;
    }
    uint16_t waiting[model::kMaxRequests] = {};
    const size_t agents = rules::WaitingAgents(queued, n, waiting);
    if (agents == 1) {
        OpenOldestOf(waiting[0], false);
    } else if (agents > 1 && Pushable(kRouteWaiting)) {
        router::PushRoute(kRouteWaiting);
    }
}

void Leave(uint16_t agent, bool from_list, size_t others, bool allowed)
{
    // Home comes back on the asking agent's page whenever it comes back: with the list still up, when the last request leaves.
    s_return = {true, agent, allowed};
    switch (rules::LeaveFor(from_list, others)) {
    case rules::LeaveTo::kAgentPage:
        router::PopToRoot();
        break;
    case rules::LeaveTo::kBackToList:
        router::Pop();
        break;
    case rules::LeaveTo::kList:
        // In place of the request: back to what it covered, then the list over that.
        router::Pop();
        router::PushRoute(kRouteWaiting);
        break;
    }
}

Return TakeReturn()
{
    const Return taken = s_return;
    s_return = {};
    return taken;
}

void SetReturn(uint16_t agent, bool allowed)
{
    s_return = {true, agent, allowed};
}

void ClearReturn()
{
    s_return = {};
}

}  // namespace agent_deck::flow
