// A stand-in for FreeRTOS's queue.h, for the host tests: no scheduler, so a queue is a plain FIFO of copied items, never waited on —
// a send to a full queue fails at once and a receive from an empty one too, as with a wait of 0. A test that runs a feature's kReady
// step drives the queue it makes from one thread.
#pragma once
#include <cstring>
#include <deque>
#include <vector>

#include "FreeRTOS.h"

struct HostQueue {
    uint32_t depth;
    uint32_t item;
    std::deque<std::vector<unsigned char>> items;
};
typedef void *QueueHandle_t;
inline QueueHandle_t xQueueCreate(uint32_t depth, uint32_t item) { return new HostQueue{depth, item, {}}; }
inline BaseType_t xQueueSend(QueueHandle_t handle, const void *value, TickType_t)
{
    auto *q = static_cast<HostQueue *>(handle);
    if (q == nullptr || q->items.size() >= q->depth) {
        return pdFAIL;
    }
    const auto *bytes = static_cast<const unsigned char *>(value);
    q->items.emplace_back(bytes, bytes + q->item);
    return pdTRUE;
}
inline BaseType_t xQueueReceive(QueueHandle_t handle, void *out, TickType_t)
{
    auto *q = static_cast<HostQueue *>(handle);
    if (q == nullptr || q->items.empty()) {
        return pdFAIL;
    }
    std::memcpy(out, q->items.front().data(), q->item);
    q->items.pop_front();
    return pdTRUE;
}
