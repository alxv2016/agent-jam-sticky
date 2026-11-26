# power_leave is how the board leaves — deep sleep, power-off, or power-off with the RTC set to turn it on again — shared by the hardware
# test's Sleep, Power off and Off, 60 s buttons and the agent deck's Power button, so neither feature includes the other's header
# (APP_ARCHITECTURE §2, E4). Each passes the frame the glass keeps; the sequence is the same (STICKY_HARDWARE_SPECS §5.3).
idf_component_register(SRCS "power_leave.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES lvgl__lvgl
                       PRIV_REQUIRES board devices lvgl_display router freertos log)
