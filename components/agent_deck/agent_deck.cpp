// The agent deck: Claude Code's desk companion on the Sticky's glass — the startup screen and the carousel that own home, the needs-you list, a
// permission request, a question, the page of a request closed elsewhere, the settings and the Wi-Fi page, on the deck's made-up day until the link is ported
// (STICKY_AGENT_DECK). It claims home with app_hooks and registers its routes in a kBoardUp step (APP_ARCHITECTURE §3, §4, §7).

#include "app_hooks.h"
#include "answer_screens.h"
#include "closed_page.h"
#include "deck_flow.h"
#include "deck_link.h"
#include "deck_mock.h"
#include "deck_model.h"
#include "deck_rules.h"
#include "devices.h"
#include "home_screen.h"
#include "lvgl.h"
#include "lvgl_display.h"
#include "router/router.h"
#include "settings_screen.h"
#include "sounds.h"
#include "uart_line/uart_line.h"
#include "waiting_screen.h"
#include "wifi_screen.h"

#include <cstdio>
#include <cstring>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

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

rules::EventHeard s_heard;
rules::EventSound s_playing = rules::EventSound::kNone;
uint32_t s_playing_since_ms = 0;

devices::sound::Word WordOf(rules::EventSound sound)
{
    switch (sound) {
    case rules::EventSound::kAttention: return devices::sound::Word::kAttention;
    case rules::EventSound::kUsageLimit: return devices::sound::Word::kUsageLimit;
    case rules::EventSound::kLinkUp: return devices::sound::Word::kLinkUp;
    case rules::EventSound::kAgentStart:
    case rules::EventSound::kNone: break;
    }
    return devices::sound::Word::kAgentStart;
}

// The deck's event sounds, from every change wherever the glass is: a lower one never cuts a higher one still sounding, so a
// snapshot's agents starting do not drown the link coming up (DESIGN → Motion → Sound; STICKY_AGENT_DECK §6). The sound
// setting mutes them with the rest. On standby another deck is the active one, so this one's news is silent, as its mascot is.
void SoundChange(const model::Change &change)
{
    const model::Agent *agent = change.event == model::Event::kAgent ? model::FindAgent(static_cast<uint16_t>(change.id)) : nullptr;
    const rules::EventSound next =
        rules::EventSoundFor(change.event, change.id, change.added, change.was, agent != nullptr ? agent->state : model::AgentState::kIdle,
                             !model::LinkUp() || lv_tick_elaps(model::LinkedMs()) < rules::kSnapshotQuietMs,
                             model::GetUsage().limit != model::Limit::kNone, s_heard);
    if (model::Standby() != model::StandbyPage::kNone) {
        return;  // heard, so the news is current when standby ends, and not sounded
    }
    const uint32_t now = lv_tick_get();
    const uint32_t playing_ms =
        s_playing == rules::EventSound::kNone ? 0 : devices::sound::LengthMs(devices::sound::SoundOf(WordOf(s_playing)));
    if (!rules::EventSoundCuts(next, s_playing, lv_tick_diff(now, s_playing_since_ms), playing_ms)) {
        return;
    }
    s_playing = next;
    s_playing_since_ms = now;
    devices::Play(WordOf(next));
}

// The sound setting is the whole buzzer's: off mutes every sound the glass makes, the press tick included (STICKY_AGENT_DECK §4.7).
void OnModel(const model::Change &change, void *)
{
    if (change.event == model::Event::kSound) {
        devices::SetMuted(!model::SoundOn());
    }
    SoundChange(change);
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

// --- The link to the bridge on the cable (STICKY_AGENT_DECK §10) -----------------------------------------------------------------------

// A line from the cable, copied off uart_line's reader task into PSRAM, for LVGL's task to take: the link's records, or the console's
// `mock`, which starts the made-up day for the bench (STICKY_AGENT_DECK §10.4).
struct Incoming {
    char *text;  // heap_caps_malloc'd in PSRAM; wiped and freed once taken
    size_t len;
    bool mock;
};
constexpr size_t kIncomingDepth = 32;  // a snapshot's burst, as the deck's
constexpr uint32_t kDrainMs = 20;
QueueHandle_t s_incoming = nullptr;

// A line can carry the bridge's Wi-Fi password or pairing key: every copy is wiped before it is freed (STICKY_AGENT_DECK §10.1).
void WipeAndFree(char *text, size_t len)
{
    volatile char *wipe = text;
    for (size_t i = 0; i < len; ++i) {
        wipe[i] = '\0';
    }
    heap_caps_free(text);
}

// On uart_line's reader task, which only copies (uart_line.h): the line into PSRAM and onto the queue for LVGL's task.
void Hand(char *line, size_t len, bool mock)
{
    if (s_incoming == nullptr) {
        return;
    }
    char *copy = static_cast<char *>(heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (copy == nullptr) {
        ESP_LOGW(kTag, "a line from the bridge dropped: no memory for its copy");
        return;
    }
    std::memcpy(copy, line, len);
    copy[len] = '\0';
    const Incoming in{copy, len, mock};
    if (xQueueSend(s_incoming, &in, 0) != pdTRUE) {
        // A burst past the queue: the record is lost until the bridge's next snapshot, after its hello or a lost link.
        WipeAndFree(copy, len + 1);
        ESP_LOGW(kTag, "a line from the bridge dropped: the queue is full");
    }
}

void OnLinkLine(char *line, size_t len)
{
    Hand(line, len, false);
}

void OnMockWord(char *line, size_t len)
{
    Hand(line, len, true);
}

// On LVGL's task: every line queued since the last look, in order, each wiped once taken.
void Drain(lv_timer_t *)
{
    Incoming in{};
    while (xQueueReceive(s_incoming, &in, 0) == pdTRUE) {
        if (in.mock) {
            if (!mock::Start()) {
                ESP_LOGE(kTag, "the mock day did not start");
            }
        } else if (!link::Take(in.text, in.len)) {
            ESP_LOGW(kTag, "a line from the bridge not taken: its CRC, its JSON or its type is wrong");
        }
        WipeAndFree(in.text, in.len + 1);
    }
}

bool SendLine(const char *line, size_t len)
{
    return uart_line::WriteLine(line, len);
}

void OnDecision(uint32_t id, bool allow)
{
    link::Decided(id, allow);
}

void OnAnswer(uint32_t id, uint8_t part, uint8_t chosen)
{
    link::Answered(id, part, chosen);
}

// The standby page's Connect: `use` on the cable. Unwritten, the page's 15 s wait ends it as timed out (model::kConnectWaitMs).
void OnConnect()
{
    if (!link::Use()) {
        ESP_LOGW(kTag, "Connect: the line to the bridge could not be written");
    }
}

// A kReady step: the cable's line, the link's hello and its timer. The deck shows the real link from boot: "Connecting…" until the bridge
// answers (STICKY_AGENT_DECK §10).
//
// The readers join the line last, once the drain runs: a line is never queued with nobody to take it.
void StartLink()
{
    s_incoming = xQueueCreate(kIncomingDepth, sizeof(Incoming));
    if (s_incoming == nullptr || uart_line::Start() != ESP_OK) {
        ESP_LOGE(kTag, "no line to the bridge: the deck shows what the model holds");
        return;
    }
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char id[8];
    std::snprintf(id, sizeof(id), "%02x%02x", mac[4], mac[5]);
    lvgl_display::Lock();
    model::SetListeners(OnDecision, OnAnswer);
    model::SetConnectListener(OnConnect);
    const bool timer = lv_timer_create(Drain, kDrainMs, nullptr) != nullptr;
    const bool linked = link::Init(SendLine, {id, "sticky", "usb"});
    lvgl_display::Unlock();
    if (!timer || !linked) {
        ESP_LOGE(kTag, "the link did not start: the deck shows what the model holds");
        return;
    }
    if (uart_line::AddReader(link::kPrefix, OnLinkLine) != ESP_OK || uart_line::AddReader("mock", OnMockWord) != ESP_OK) {
        ESP_LOGE(kTag, "no room on the line for the bridge's records: the deck shows what the model holds");
        return;
    }
    ESP_LOGI(kTag, "link: deck %s says hello on the cable", id);
}

}  // namespace

void Install()
{
    app_hooks::ClaimHome({home::Create, home::Enter, home::Exit, home::Covered});
    app_hooks::OnBoot(app_hooks::Phase::kBoardUp, AddRoutes);
    app_hooks::OnBoot(app_hooks::Phase::kReady, StartLink);
}

}  // namespace agent_deck
