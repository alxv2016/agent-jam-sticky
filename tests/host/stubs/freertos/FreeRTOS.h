// A stand-in for FreeRTOS.h, for the host tests: the types and macros the hardware test names; no scheduler.
#pragma once
#include <cstdint>
typedef uint32_t TickType_t;
typedef int BaseType_t;
#define pdPASS 1
#define pdFAIL 0
#define portMAX_DELAY 0xFFFFFFFFu
#define pdMS_TO_TICKS(ms) static_cast<TickType_t>(ms)
#define pdTRUE 1
#define pdFALSE 0
typedef unsigned UBaseType_t;
// One thread on the host: a critical section is nothing to enter.
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux) ((void)(mux))
#define portEXIT_CRITICAL(mux) ((void)(mux))
