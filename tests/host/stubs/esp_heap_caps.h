// A stand-in for ESP-IDF's esp_heap_caps.h, for the host tests: the router logs the internal heap on each push, and the
// computer has none to report; the hardware test's screenshot copy and lvgl_display::InPsram take the computer's own heap.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#define MALLOC_CAP_8BIT (1 << 2)
#define MALLOC_CAP_SPIRAM (1 << 10)
#define MALLOC_CAP_INTERNAL (1 << 11)
inline size_t heap_caps_get_free_size(uint32_t) { return 0; }
inline size_t heap_caps_get_largest_free_block(uint32_t) { return 0; }
// A test that must know a buffer was wiped before it was freed turns `on`: each heap_caps_malloc is then kept with its size, and
// each heap_caps_free of one counted, and counted as unwiped when a byte of it is not 0.
struct HostHeapWatch {
    bool on = false;
    std::unordered_map<void *, size_t> sizes;
    int freed = 0;
    int unwiped = 0;
};
inline HostHeapWatch g_host_heap;
inline void *heap_caps_malloc(size_t size, uint32_t)
{
    void *p = std::malloc(size);
    if (g_host_heap.on && p != nullptr) {
        g_host_heap.sizes[p] = size;
    }
    return p;
}
inline void *heap_caps_calloc(size_t n, size_t size, uint32_t caps)
{
    void *p = heap_caps_malloc(n * size, caps);
    if (p != nullptr) {
        std::memset(p, 0, n * size);
    }
    return p;
}
inline void heap_caps_free(void *p)
{
    const auto it = g_host_heap.sizes.find(p);
    if (it != g_host_heap.sizes.end()) {
        const auto *bytes = static_cast<const unsigned char *>(p);
        bool zero = true;
        for (size_t i = 0; i < it->second; ++i) {
            zero = zero && bytes[i] == 0;
        }
        ++g_host_heap.freed;
        g_host_heap.unwiped += zero ? 0 : 1;
        g_host_heap.sizes.erase(it);
    }
    std::free(p);
}
inline void *heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t)
{
    void *p = nullptr;
    return posix_memalign(&p, alignment < sizeof(void *) ? sizeof(void *) : alignment, size) == 0 ? p : nullptr;
}
