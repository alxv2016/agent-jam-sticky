// A stand-in for ESP-IDF's driver/uart.h, for the host tests: UART0 is what the test scripts. A read hands out the script's bytes
// a chunk at a time, as the driver's buffer would; a blocking read past the script's end throws HostUartEnd, which ends a reader
// task's endless loop for the test that called it.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "esp_err.h"

typedef int uart_port_t;
#define UART_NUM_0 0

struct HostUartEnd {};
struct HostUart {
    bool installed = false;
    std::string script;
    size_t at = 0;
    size_t chunk = 64;  // the most a read finds buffered at once
};
inline HostUart g_host_uart;

inline bool uart_is_driver_installed(uart_port_t) { return g_host_uart.installed; }
inline esp_err_t uart_driver_install(uart_port_t, int, int, int, void *, int)
{
    g_host_uart.installed = true;
    return ESP_OK;
}
inline esp_err_t uart_get_buffered_data_len(uart_port_t, size_t *size)
{
    const size_t left = g_host_uart.script.size() - g_host_uart.at;
    *size = left < g_host_uart.chunk ? left : g_host_uart.chunk;
    return ESP_OK;
}
inline int uart_read_bytes(uart_port_t, void *buf, uint32_t length, uint32_t wait)
{
    const size_t left = g_host_uart.script.size() - g_host_uart.at;
    if (left == 0) {
        if (wait != 0) {
            throw HostUartEnd{};
        }
        return 0;
    }
    const size_t n = length < left ? length : left;
    std::memcpy(buf, g_host_uart.script.data() + g_host_uart.at, n);
    g_host_uart.at += n;
    return static_cast<int>(n);
}
