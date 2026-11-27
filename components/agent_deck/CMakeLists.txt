# agent_deck is the agent deck's UI feature: the startup screen and the carousel that own home, standby, the needs-you list, a
# permission request, a question, the closed-request page, the settings and the Wi-Fi page, fed by the link to the bridge on the
# cable (deck_link.cpp; the made-up day, deck_mock.cpp, from the console's `mock`). It meets the shell only through
# app_hooks — it claims home there — and the router through its routes (APP_ARCHITECTURE §2, §7; STICKY_AGENT_DECK).
idf_component_register(SRCS "agent_deck.cpp"
                            "answer_screens.cpp"
                            "closed_page.cpp"
                            "deck_clock.cpp"
                            "deck_flow.cpp"
                            "deck_link.cpp"
                            "deck_mock.cpp"
                            "deck_model.cpp"
                            "deck_stage.cpp"
                            "home_screen.cpp"
                            "off_frame.cpp"
                            "settings_screen.cpp"
                            "waiting_screen.cpp"
                            "wifi_screen.cpp"
                            "logo.c"
                       PRIV_REQUIRES app_hooks board_config design_tokens devices lvgl_display mascot_voice power_leave router uart_line ui
                                     widgets lvgl__lvgl espressif__cjson esp_hw_support freertos log)
