#include "power_leave.h"

#include "board.h"
#include "devices.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl_display.h"
#include "router/router.h"

namespace power_leave {
namespace {

constexpr const char *kTag = "power_leave";

// The frame left on the glass when the board leaves: drawn in full, then the panel sleeps (DESIGN → Motion).
constexpr uint32_t kSilenceSettleMs = 10;  // many of the buzzer task's 1 ms steps, so its duty 0 is out before sleep
constexpr uint32_t kLeaveDelayMs = 50;

// The frame being built and how the board leaves, for the leaving screen's create function: the board leaves once, so one is enough.
Frame s_frame = nullptr;
How s_how = How::kSleep;

void CreateLeaving(lv_obj_t *screen, lv_group_t *, void *)
{
    if (s_frame != nullptr) {
        s_frame(screen, s_how);
    }
}

// The leave a press asked for and not yet begun, run by a timer 50 ms on; a second press meanwhile is the same leave.
bool s_pending = false;
How s_pending_how = How::kSleep;
Frame s_pending_frame = nullptr;

void Soon(lv_timer_t *timer)
{
    lv_timer_delete(timer);
    s_pending = false;
    Leave(s_pending_how, s_pending_frame);
}

}  // namespace

void Leave(How how, Frame frame)
{
    // A sound still playing would leave the buzzer's pad high under the hold deep sleep puts on it, the coil drawing all
    // night (STICKY_HARDWARE_SPECS §5.3); this one is cut here and the last press's tick once more below.
    devices::Silence();
    const bool power_off = how != How::kSleep;
    // An RTC interrupt left low would power the board straight back on (STICKY_HARDWARE_SPECS §5.3); set it first, so
    // the glass promises only what is set. On USB the board restarts, and the start clears the countdown.
    if (how == How::kPowerOffTimed && (board::UsbPresent() || devices::SetRtcWake(kRtcWakeS) != ESP_OK)) {
        how = How::kPowerOff;
    }
    if (how != How::kPowerOffTimed && devices::ClearRtcWake() != ESP_OK) {
        ESP_LOGE(kTag, "RTC interrupts not cleared: the board may turn on again");
    }
    // The screens above home are let go, the board never comes back to them; the one on view stops its own timers as it is covered.
    s_frame = frame;
    s_how = how;
    if (router::GoTo(router::MakeScreen(CreateLeaving)) != ESP_OK) {
        ESP_LOGE(kTag, "no memory for the leaving screen: the glass keeps the last one");
    }
    lvgl_display::RequestFull();
    lvgl_display::Redraw();
    const bool panel_asleep = lvgl_display::SleepPanel() == ESP_OK;
    if (!panel_asleep) {
        ESP_LOGE(kTag, "the panel did not sleep: its power stays on");
    }
    devices::Silence();  // a press during the full refresh ticked; the buzzer task wakes within a step to stop it
    vTaskDelay(pdMS_TO_TICKS(kSilenceSettleMs));
    if (power_off) {
        board::PowerOff();  // returns only on battery when the rail did not fall
        ESP_LOGW(kTag, "the rail did not fall: deep sleep instead");
        // Only the buttons wake deep sleep, so a countdown left running would mislead the next start. A timed frame
        // already on the glass keeps its "Turns on in 60 s" here: the panel is asleep, and this path needs a fault.
        if (devices::ClearRtcWake() != ESP_OK) {
            ESP_LOGE(kTag, "RTC interrupts not cleared before deep sleep");
        }
    }
    board::EnterDeepSleep(!panel_asleep);
    ESP_LOGW(kTag, "still running after deep sleep");
}

void LeaveSoon(How how, Frame frame)
{
    if (s_pending) {
        return;
    }
    s_pending = true;
    s_pending_how = how;
    s_pending_frame = frame;
    // From a timer, not this click: the press has to finish before the board waits for the buttons to be released.
    if (lv_timer_create(Soon, kLeaveDelayMs, nullptr) == nullptr) {
        s_pending = false;  // no timer: nothing is pending, so the next press asks again
        ESP_LOGE(kTag, "no memory for the leave's timer: the board stays on");
    }
}

}  // namespace power_leave
