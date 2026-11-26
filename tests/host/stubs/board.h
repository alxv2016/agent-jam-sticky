// A stand-in for the board component's board.h, for the host tests: lvgl_display.h names its Display type, and power_leave.cpp calls the
// board's power calls, declared as the real board.h declares them; a test defines what it needs.
#pragma once
#include "esp_err.h"
namespace board {
struct Display {};
bool UsbPresent();
esp_err_t EnterDeepSleep(bool keep_panel_powered = false);
esp_err_t PowerOff();
}  // namespace board
