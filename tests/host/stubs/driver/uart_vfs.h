// A stand-in for ESP-IDF's driver/uart_vfs.h, for the host tests: stdout stays the computer's.
#pragma once
#include "driver/uart.h"
inline void uart_vfs_dev_use_driver(uart_port_t) {}
