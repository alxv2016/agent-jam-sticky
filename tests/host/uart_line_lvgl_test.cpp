// Host test of UART0 as text lines (components/uart_line/uart_line.cpp), on the computer with no board: the reader table and its
// default, a line sent to the reader whose prefix it starts with, prefix stripped, the rest to the default, an empty line to none,
// a line split across reads and ended by \r or \n, a line past kMaxLine dropped whole and the next one kept, the line's buffer
// wiped once its reader returns, one start only, and a written line going out whole with its line end. UART0 is the test's
// script (tests/host/stubs/driver/uart.h); the reader task runs on the test's thread until the script ends. Named _lvgl_ for the
// stubs and host-sources that set gives; it uses no LVGL (STICKY_HARDWARE_SPECS §2; STICKY_AGENT_DECK §10.1).
// run.sh builds LVGL's sources from managed_components once, into build/host/lvgl, and the sources named below.
// host-sources: components/uart_line/uart_line.cpp
// Build and run: bash tests/host/run.sh uart_line

#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "freertos/idf_additions.h"
#include "uart_line/uart_line.h"

namespace {

int s_failures = 0;
int s_checks = 0;

void Check(bool ok, const std::string &what)
{
    ++s_checks;
    if (!ok) {
        ++s_failures;
        std::printf("FAIL  %s\n", what.c_str());
    }
}

std::vector<std::string> s_link;
std::vector<std::string> s_mock;
std::vector<std::string> s_rest;
void OnLink(char *line, size_t len) { s_link.emplace_back(line, len); }
void OnMock(char *line, size_t len) { s_mock.emplace_back(line, len); }
void OnRest(char *line, size_t len) { s_rest.emplace_back(line, len); }

// The reader task, on this thread, over `bytes` as they arrive on the cable; it returns when they run out.
void Play(const std::string &bytes, size_t chunk = 64)
{
    g_host_uart.script = bytes;
    g_host_uart.at = 0;
    g_host_uart.chunk = chunk;
    s_link.clear();
    s_mock.clear();
    s_rest.clear();
    try {
        g_host_task.fn(g_host_task.arg);
    } catch (const HostUartEnd &) {
    }
}

bool Wiped()
{
    const auto *line = static_cast<const unsigned char *>(g_host_task.arg);
    for (size_t i = 0; i < uart_line::kMaxLine; ++i) {
        if (line[i] != 0) {
            return false;
        }
    }
    return true;
}

void TestStart()
{
    std::printf("start\n");
    Check(uart_line::AddReader(nullptr, OnLink) == ESP_ERR_INVALID_ARG, "a reader with no prefix is refused");
    Check(!uart_line::WriteLine("x", 1), "nothing is written before the start");
    Check(uart_line::AddReader("@deck ", OnLink) == ESP_OK && uart_line::AddReader("mock", OnMock) == ESP_OK,
          "readers join before the start");
    uart_line::SetDefaultReader(OnRest);
    Check(uart_line::Start() == ESP_OK && g_host_uart.installed && g_host_task.made == 1, "the start installs the driver and one reader task");
    Check(uart_line::Start() == ESP_OK && g_host_task.made == 1, "a second start changes nothing");
}

void TestDispatch()
{
    std::printf("lines to their readers\n");
    Play("@deck 497c309c {\"type\":\"ping\"}\nmock\r\nhelp\n\n\r\n");
    Check(s_link.size() == 1 && s_link[0] == "497c309c {\"type\":\"ping\"}", "a line with the link's prefix goes to it, prefix stripped");
    Check(s_mock.size() == 1 && s_mock[0].empty(), "the console's mock to its reader, \\r\\n ending it");
    Check(s_rest.size() == 1 && s_rest[0] == "help", "anything else to the default reader; an empty line to none");
    Check(Wiped(), "the line's buffer is wiped once its reader returns");

    const std::string body(3000, 'b');
    Play("@deck " + body + "\n", 7);
    Check(s_link.size() == 1 && s_link[0] == body, "a line split across many reads arrives whole");

    Play("@de" "ck x\n", 2);
    Check(s_link.size() == 1 && s_link[0] == "x", "a prefix split across reads still matches");
    Play("@deck", 64);
    Check(s_link.empty() && s_rest.empty(), "a line with no end yet goes nowhere");
}

void TestOverlong()
{
    std::printf("a line too long\n");
    Play("@deck " + std::string(uart_line::kMaxLine, 'z') + "\n@deck after\n", 128);
    Check(s_link.size() == 1 && s_link[0] == "after", "a line past kMaxLine is dropped whole, and the next one kept");
    Check(Wiped(), "and nothing of it is left in the buffer");
}

void TestTable()
{
    std::printf("the reader table\n");
    Check(uart_line::AddReader("a", OnRest) == ESP_OK && uart_line::AddReader("b", OnRest) == ESP_OK, "four readers fit");
    Check(uart_line::AddReader("c", OnRest) == ESP_ERR_NO_MEM, "a fifth is refused");
}

void TestWrite()
{
    std::printf("a line written\n");
    char path[] = "/tmp/uart_line_testXXXXXX";
    const int file = mkstemp(path);
    const int saved = dup(fileno(stdout));
    std::fflush(stdout);
    dup2(file, fileno(stdout));
    const bool wrote = uart_line::WriteLine("@deck 12345678 {}", 17);
    const bool too_long = uart_line::WriteLine(std::string(uart_line::kMaxWrite + 1, 'w').c_str(), uart_line::kMaxWrite + 1);
    std::fflush(stdout);
    dup2(saved, fileno(stdout));
    close(saved);
    char got[64] = {};
    const ssize_t n = pread(file, got, sizeof(got) - 1, 0);
    close(file);
    unlink(path);
    Check(wrote && n == 18 && std::string(got, 18) == "@deck 12345678 {}\n", "a line goes out with its line end");
    Check(!too_long, "a line past kMaxWrite is refused, nothing written");
}

}  // namespace

int main()
{
    TestStart();
    TestDispatch();
    TestOverlong();
    TestTable();
    TestWrite();
    std::printf("%d checks, %d failed\n", s_checks, s_failures);
    return s_failures == 0 ? 0 : 1;
}
