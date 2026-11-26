# The agent deck's 8-bit sounds and its synth (chiptune.h), ported from the deck's components/chiptune with only the
# namespace and the include path changed, so they sound as the deck's do: plain C++ — no ESP-IDF, FreeRTOS or LVGL
# header — whose notes the hardware test's `sound ab` plays on the buzzer and a host program can render to WAV
# (APP_ARCHITECTURE §1; STICKY_HARDWARE_SPECS §4.2). Nothing to require.
idf_component_register(SRCS "chiptune.cpp"
                       INCLUDE_DIRS "include")
