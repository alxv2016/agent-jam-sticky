// A stand-in for ESP-IDF's esp_heap_caps.h, for the host tests: the router logs the internal heap on each push, and the
// computer has none to report; the hardware test's screenshot copy and lvgl_display::InPsram take the computer's own heap.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#define MALLOC_CAP_8BIT (1 << 2)
#define MALLOC_CAP_SPIRAM (1 << 10)
#define MALLOC_CAP_INTERNAL (1 << 11)
inline size_t heap_caps_get_free_size(uint32_t) { return 0; }
inline size_t heap_caps_get_largest_free_block(uint32_t) { return 0; }
inline void *heap_caps_malloc(size_t size, uint32_t) { return std::malloc(size); }
inline void heap_caps_free(void *p) { std::free(p); }
inline void *heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t)
{
    void *p = nullptr;
    return posix_memalign(&p, alignment < sizeof(void *) ? sizeof(void *) : alignment, size) == 0 ? p : nullptr;
}
