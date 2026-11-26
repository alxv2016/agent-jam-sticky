# mascot_voice is how a page that shows Claude's mascot lets it be heard: its moments to the buzzer as each frame is drawn, and its
# music kept in time with its move (DESIGN → Motion → Sound; STICKY_DESIGN_TOKENS §13). It is shared by the hardware test's mascot
# page and the agent deck's screens, so neither includes the other's header (APP_ARCHITECTURE §2, E4). mascot_voice_rules.h holds
# the rules with no LVGL and no board, which tests/host checks on the computer.
idf_component_register(SRCS "mascot_voice.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES lvgl__lvgl widgets
                       PRIV_REQUIRES devices esp_timer)
