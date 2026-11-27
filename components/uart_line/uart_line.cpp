#include "uart_line/uart_line.h"

#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"

namespace uart_line {

namespace {

constexpr const char *kTag = "uart_line";
constexpr uart_port_t kPort = UART_NUM_0;
constexpr size_t kMaxReaders = 4;
// Room for a burst from the host while the reader task is busy with a line; a longer record the reader keeps pace with as
// it arrives (11.5 KB/s at 115200 baud).
constexpr int kRxBufferBytes = 4096;
// Log output queues here instead of busy-waiting on the FIFO.
constexpr int kTxBufferBytes = 4096;

struct Entry {
    const char *prefix;
    size_t prefix_len;
    Reader reader;
};

// Append-only: an entry is written before the count that publishes it.
std::array<Entry, kMaxReaders> s_readers;
std::atomic<size_t> s_reader_count{0};
std::atomic<Reader> s_default_reader{nullptr};
std::atomic<bool> s_started{false};
portMUX_TYPE s_register_lock = portMUX_INITIALIZER_UNLOCKED;

// Zeroes `n` bytes through a volatile pointer, so the compiler keeps the stores.
void Wipe(char *p, size_t n)
{
    volatile char *v = p;
    for (size_t i = 0; i < n; ++i) {
        v[i] = '\0';
    }
}

void Dispatch(char *line, size_t len)
{
    const size_t n = s_reader_count.load(std::memory_order_acquire);
    for (size_t i = 0; i < n; ++i) {
        const Entry &e = s_readers[i];
        if (len >= e.prefix_len && std::memcmp(line, e.prefix, e.prefix_len) == 0) {
            e.reader(line + e.prefix_len, len - e.prefix_len);
            return;
        }
    }
    if (Reader fallback = s_default_reader.load(std::memory_order_acquire)) {
        fallback(line, len);
    }
}

void ReaderTask(void *arg)
{
    char *line = static_cast<char *>(arg);
    size_t n = 0;
    bool overlong = false;
    std::array<uint8_t, 128> buf;
    for (;;) {
        // Block for one byte, then take whatever else is already buffered, so a short line is read at once.
        int got = uart_read_bytes(kPort, buf.data(), 1, portMAX_DELAY);
        size_t buffered = 0;
        if (got == 1 && uart_get_buffered_data_len(kPort, &buffered) == ESP_OK && buffered > 0) {
            const size_t more = buffered < buf.size() - 1 ? buffered : buf.size() - 1;
            const int rest = uart_read_bytes(kPort, buf.data() + 1, more, 0);
            got += rest > 0 ? rest : 0;
        }
        for (int i = 0; i < got; ++i) {
            const char c = static_cast<char>(buf[i]);
            if (c == '\n' || c == '\r') {
                if (n > 0 && !overlong) {
                    line[n] = '\0';
                    Dispatch(line, n);
                }
                Wipe(line, n + 1);  // a line can carry a secret in the clear: none outlives its reader's call
                n = 0;
                overlong = false;
            } else if (n < kMaxLine - 1) {
                line[n++] = c;
            } else if (!overlong) {
                overlong = true;
                ESP_LOGW(kTag, "a line past %u B dropped", static_cast<unsigned>(kMaxLine));
            }
        }
        if (got > 0) {
            Wipe(reinterpret_cast<char *>(buf.data()), static_cast<size_t>(got));  // the chunk read, off the stack
        }
    }
}

}  // namespace

esp_err_t AddReader(const char *prefix, Reader reader)
{
    ESP_RETURN_ON_FALSE(prefix != nullptr && reader != nullptr, ESP_ERR_INVALID_ARG, kTag, "no prefix or reader");
    portENTER_CRITICAL(&s_register_lock);
    const size_t n = s_reader_count.load(std::memory_order_relaxed);
    if (n == kMaxReaders) {
        portEXIT_CRITICAL(&s_register_lock);
        ESP_LOGE(kTag, "no room for '%s': raise kMaxReaders past %u", prefix, static_cast<unsigned>(kMaxReaders));
        return ESP_ERR_NO_MEM;
    }
    s_readers[n] = {prefix, std::strlen(prefix), reader};
    s_reader_count.store(n + 1, std::memory_order_release);
    portEXIT_CRITICAL(&s_register_lock);
    return ESP_OK;
}

void SetDefaultReader(Reader reader)
{
    s_default_reader.store(reader, std::memory_order_release);
}

esp_err_t Start()
{
    portENTER_CRITICAL(&s_register_lock);
    const bool already = s_started.exchange(true);
    portEXIT_CRITICAL(&s_register_lock);
    if (already) {
        return ESP_OK;
    }
    // A start that fails is not started: the next caller tries again.
    esp_err_t err = ESP_OK;
    char *line = nullptr;
    if (!uart_is_driver_installed(kPort)) {
        // The console's own pins and baud rate, set by the bootloader and CONFIG_ESP_CONSOLE_UART_*: only the driver is added.
        err = uart_driver_install(kPort, kRxBufferBytes, kTxBufferBytes, 0, nullptr, 0);
        if (err == ESP_OK) {
            uart_vfs_dev_use_driver(kPort);
        }
    }
    if (err == ESP_OK) {
        // PSRAM, 50 KB kept out of internal RAM, wiped at each line's end.
        line = static_cast<char *>(heap_caps_calloc(1, kMaxLine, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        err = line != nullptr ? ESP_OK : ESP_ERR_NO_MEM;
    }
    // The stack in PSRAM: nothing on the reader's task calls the flash driver or a sleep start (uart_line.h).
    constexpr UBaseType_t kStackCaps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    if (err == ESP_OK && xTaskCreateWithCaps(ReaderTask, "uart_line", kTaskStackBytes, line, 2, nullptr, kStackCaps) != pdPASS) {
        err = ESP_ERR_NO_MEM;
    }
    if (err != ESP_OK) {
        heap_caps_free(line);
        s_started.store(false);
        ESP_LOGE(kTag, "not started: %s", esp_err_to_name(err));
    }
    return err;
}

bool WriteLine(const char *text, size_t len)
{
    if (!s_started.load() || len > kMaxWrite) {
        return false;
    }
    std::array<char, kMaxWrite + 1> out;
    std::memcpy(out.data(), text, len);
    out[len] = '\n';
    // stdout's VFS takes its write lock for the whole call: no log line inside this one.
    fflush(stdout);
    return ::write(fileno(stdout), out.data(), len + 1) == static_cast<ssize_t>(len + 1);
}

}  // namespace uart_line
