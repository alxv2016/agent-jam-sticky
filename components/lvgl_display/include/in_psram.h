// A large mutable static of the LVGL task's, kept in PSRAM: internal DIRAM is the scarce memory on the ESP32-S3, so a table of
// records or slots that a screen, a widget or the deck's model keeps is made once in the 8 MB of external RAM, value-initialised as a
// static of its type would be (APP_ARCHITECTURE §2). Bind a namespace-scope reference to it: the startup code has added PSRAM to the
// heap before the C++ static initialisers run. Never for what an ISR or the buzzer's critical sections touch — PSRAM is out of reach
// while the flash cache is off. On the host the computer's heap stands in (tests/host/stubs/esp_heap_caps.h).

#pragma once

#include <cstdlib>
#include <new>
#include <type_traits>

#include "esp_heap_caps.h"

namespace lvgl_display {

// A new T in PSRAM, value-initialised, never freed: one call, one object, for the life of the firmware.
template <typename T>
T &InPsram()
{
    static_assert(!std::is_array_v<T>, "hold a table as a std::array: a new-expression of an array type yields its first element");
    void *at = heap_caps_aligned_alloc(alignof(T), sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (at == nullptr) {
        std::abort();  // before app_main, with all of PSRAM free: only a board whose PSRAM failed gets here, and it cannot run
    }
    return *new (at) T();
}

}  // namespace lvgl_display
