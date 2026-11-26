# hw_test is the bench feature: a row on its own home, `hw.home`, per part of the board, and a screen that exercises it on V1.0 and V2.0
# alike, each a route on the router's stack. It meets the shell only through app_hooks (APP_ARCHITECTURE §2).
idf_component_register(SRCS "hw_test.cpp" "mascot_page.cpp" "sound_ab.cpp"
                       PRIV_REQUIRES app_hooks board chiptune devices design_tokens lvgl_display mascot_voice power_leave router ui widgets lvgl__lvgl esp_app_format esp_hw_support
                                     esp_psram esp_timer spi_flash esp_driver_uart freertos log)
