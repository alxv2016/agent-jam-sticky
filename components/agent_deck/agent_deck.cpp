// The agent deck: Claude Code's desk companion on the Sticky's glass — the startup screen and the carousel that own home, the needs-you list, a
// permission request, a question, the page of a request closed elsewhere, the settings and the Wi-Fi page, on the deck's made-up day until the link is ported
// (STICKY_AGENT_DECK). It claims home with app_hooks and registers its routes in a kBoardUp step (APP_ARCHITECTURE §3, §4, §7).

#include "app_hooks.h"
#include "answer_screens.h"
#include "closed_page.h"
#include "deck_flow.h"
#include "deck_model.h"
#include "devices.h"
#include "home_screen.h"
#include "router/router.h"
#include "settings_screen.h"
#include "waiting_screen.h"
#include "wifi_screen.h"

#include <cstring>

#include "esp_log.h"

namespace agent_deck {
namespace {

constexpr const char *kTag = "agent_deck";

std::unique_ptr<router::Screen> ClosedRoute(const char *param)
{
    if (param != nullptr && std::strcmp(param, "answered") == 0) {
        return ClosedScreen(true);
    }
    if (param != nullptr && std::strcmp(param, "timed-out") == 0) {
        return ClosedScreen(false);
    }
    return nullptr;
}

constexpr router::Route kRoutes[] = {
    {flow::kRouteWaiting, router::NoParam<WaitingScreen>},
    {flow::kRouteRequest, RequestScreen},
    {flow::kRouteQuestion, QuestionScreen},
    {flow::kRouteClosed, ClosedRoute},
    {flow::kRouteSettings, router::NoParam<SettingsScreen>},
    {flow::kRouteWifi, router::NoParam<WifiScreen>},
};

// The sound setting is the whole buzzer's: off mutes every sound the glass makes, the press tick included (STICKY_AGENT_DECK §4.7).
void OnModel(const model::Change &change, void *)
{
    if (change.event == model::Event::kSound) {
        devices::SetMuted(!model::SoundOn());
    }
}

// A kBoardUp step, before anything is drawn: the routes are reachable by the time home is (APP_ARCHITECTURE §3); the buzzer
// follows the sound setting from then on.
void AddRoutes()
{
    devices::SetMuted(!model::SoundOn());
    if (model::Subscribe(OnModel, nullptr) == 0) {
        ESP_LOGE(kTag, "no room to follow the sound setting: the buzzer stays as it is");
    }
    if (router::AddRoutes({kRoutes, sizeof(kRoutes) / sizeof(kRoutes[0])}) != ESP_OK) {
        ESP_LOGE(kTag, "the deck's routes were refused: its screens open nothing");
    }
}

}  // namespace

void Install()
{
    app_hooks::ClaimHome({home::Create, home::Enter, home::Exit, home::Covered});
    app_hooks::OnBoot(app_hooks::Phase::kBoardUp, AddRoutes);
}

}  // namespace agent_deck
