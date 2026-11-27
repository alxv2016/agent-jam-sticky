// UART0 as text lines, shared by whoever reads the cable. On the reTerminal Sticky the USB-C port reaches the ESP32-S3 only
// through the CH343P on UART0, so the console log goes out on it, and each line that comes in goes to the reader registered
// for its prefix, else to the default reader: the agent deck's link takes `@deck ` lines, the hardware test's console the
// rest. One driver, one reader task, logging moved onto the driver (STICKY_HARDWARE_SPECS §2; STICKY_AGENT_DECK §10).
//
// A written line goes out in one write() through the console's VFS, whose write lock keeps a log line out of it: it arrives
// whole. A log line newlib writes in pieces can still have a written line land between them, so a reader of the cable on the
// Mac looks for its prefix anywhere in a line, not only at its start.

#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

namespace uart_line {

// A line that came in, without its prefix or line end, NUL-terminated, on the reader's task (kTaskStackBytes of stack, in
// PSRAM). A reader never calls the flash driver, nor what does — no NVS, esp_partition_*, esp_flash_*, spi_flash_*, OTA —
// nor a sleep start: a flash-driver call turns the cache, and with it the stack, off. A reader that does more than copy
// the line hands it to a task of its own. `line` lasts only for the call; the reader may change it.
using Reader = void (*)(char *line, size_t len);

// The longest line kept; a longer one is dropped whole. 50 KB: a 16 KB request body escaped, as the link's own limit. Its
// buffer is in PSRAM and wiped at each line's end, since a line can carry the bridge's Wi-Fi password and pairing key.
inline constexpr size_t kMaxLine = 50 * 1024;
// The longest line WriteLine() sends, built on the caller's stack: over the link's longest record out today, a decision, an
// answer or a hello (link::kMaxShortOut). The Wi-Fi scan's record raises it when it is ported.
inline constexpr size_t kMaxWrite = 512;
// The reader task's stack.
inline constexpr size_t kTaskStackBytes = 8 * 1024;

// Before or after Start(), from any task. `prefix` must outlive the line (a literal); a line starting with it goes to
// `reader` — as a string, so "mock" takes "mockup" too. ESP_ERR_NO_MEM past the table.
esp_err_t AddReader(const char *prefix, Reader reader);

// Every line no prefix claims; before or after Start().
void SetDefaultReader(Reader reader);

// Installs the UART0 driver at the console's baud rate, moves logging onto it and starts the reader task. A second call is
// ESP_OK and changes nothing, so each user of the line calls it.
esp_err_t Start();

// Writes `len` bytes and a line end in one write(), from any task; false before Start() or past kMaxWrite. The CH343P
// clocks the bytes out whether or not a host has the port open, so with none they are simply lost.
bool WriteLine(const char *text, size_t len);

}  // namespace uart_line
