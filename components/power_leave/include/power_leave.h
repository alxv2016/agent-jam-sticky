// How the board leaves: the frame the glass keeps, then the panel's sleep, then deep sleep, power-off, or power-off with the RTC set
// to turn it on again (APP_ARCHITECTURE §1; STICKY_HARDWARE_SPECS §5.3). The hardware test's Sleep, Power off and Off, 60 s buttons and the
// agent deck's Power button run this one sequence, each with a frame of its own. Everything under the LVGL lock.

#pragma once

#include <cstdint>

#include "lvgl.h"

namespace power_leave {

// How the board leaves: deep sleep, power off, or power off with the RTC set to turn it on again.
enum class How : uint8_t { kSleep, kPowerOff, kPowerOffTimed };

// The seconds a timed power-off waits before the RTC turns the board on (DESIGN → Exceptions → The hardware-test demo: a hardware-test timer).
inline constexpr uint8_t kRtcWakeS = 60;

// The frame the glass keeps: builds the leaving screen into `screen`, the router's root for it (call ui::StyleScreen on it), as the board
// leaves by `how` — which is `kPowerOff`, not the timed one, where the timed one could not be set (on USB the board restarts instead of
// staying off, and the countdown is not running).
using Frame = void (*)(lv_obj_t *screen, How how);

// The sequence, which does not return but when the board is still on: silences the buzzer, sets or clears the RTC wake, replaces the
// screens above home with `frame`, draws it in full, puts the panel to sleep and then powers off or sleeps. Call it from a timer, never
// from the click that asked (LeaveSoon).
void Leave(How how, Frame frame);

// `Leave` from a timer 50 ms on, not inside the click: the press has to finish before the board waits for the buttons to be released.
void LeaveSoon(How how, Frame frame);

}  // namespace power_leave
