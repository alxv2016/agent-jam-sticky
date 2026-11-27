# uart_line is UART0 as text lines, shared by whoever reads the cable: the hardware test's console and the agent deck's link to
# the bridge on the Mac. One driver and one reader task own the port; each line goes to the reader registered for its prefix,
# else to the default reader (STICKY_HARDWARE_SPECS §2; STICKY_AGENT_DECK §10). Ported from the agent deck's uart_line.
idf_component_register(SRCS "uart_line.cpp"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_driver_uart esp_hw_support freertos log vfs)
