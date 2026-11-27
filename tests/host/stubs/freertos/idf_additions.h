// A stand-in for ESP-IDF's FreeRTOS additions, for the host tests: a task made with caps is not run but kept, its function and
// argument, for the test to call on its own thread.
#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
struct HostTask {
    void (*fn)(void *) = nullptr;
    void *arg = nullptr;
    int made = 0;
};
inline HostTask g_host_task;
inline BaseType_t xTaskCreateWithCaps(void (*fn)(void *), const char *, uint32_t, void *arg, UBaseType_t, TaskHandle_t *, UBaseType_t)
{
    g_host_task.fn = fn;
    g_host_task.arg = arg;
    ++g_host_task.made;
    return pdPASS;
}
