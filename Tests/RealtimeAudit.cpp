#include "RealtimeAudit.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <new>
#include <pthread.h>

namespace realtimeAudit {
thread_local bool enabled = false;
thread_local std::size_t allocations = 0, deallocations = 0, locks = 0, contendedLocks = 0;
thread_local std::uint64_t waitNanoseconds = 0, maximumWaitNanoseconds = 0;
std::atomic<std::size_t> observedContentions{0};
}  // namespace realtimeAudit
extern "C" {
void* __real_malloc(std::size_t);
void* __real_calloc(std::size_t, std::size_t);
void* __real_realloc(void*, std::size_t);
void __real_free(void*);
int __real_pthread_mutex_lock(pthread_mutex_t*);
void* __wrap_malloc(std::size_t size) {
    if (realtimeAudit::enabled) {
        ++realtimeAudit::allocations;
    }
    return __real_malloc(size);
}
void* __wrap_calloc(std::size_t count, std::size_t size) {
    if (realtimeAudit::enabled)
        ++realtimeAudit::allocations;
    return __real_calloc(count, size);
}
void* __wrap_realloc(void* pointer, std::size_t size) {
    if (realtimeAudit::enabled)
        ++realtimeAudit::allocations;
    return __real_realloc(pointer, size);
}
void __wrap_free(void* pointer) {
    if (realtimeAudit::enabled && pointer)
        ++realtimeAudit::deallocations;
    __real_free(pointer);
}
int __wrap_pthread_mutex_lock(pthread_mutex_t* mutex) {
    if (!realtimeAudit::enabled)
        return __real_pthread_mutex_lock(mutex);
    ++realtimeAudit::locks;
    // Test-only interposition: trylock preserves recursive acquisition semantics.
    // Successful acquisition replaces lock; the original caller still unlocks once.
    const int attempt = pthread_mutex_trylock(mutex);
    if (attempt == 0 || attempt == EOWNERDEAD)
        return attempt;
    if (attempt != EBUSY)
        return __real_pthread_mutex_lock(mutex);
    ++realtimeAudit::contendedLocks;
    realtimeAudit::observedContentions.fetch_add(1);
    const auto start = std::chrono::steady_clock::now();
    const int result = __real_pthread_mutex_lock(mutex);
    const auto waited =
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                       std::chrono::steady_clock::now() - start)
                                       .count());
    realtimeAudit::waitNanoseconds += waited;
    realtimeAudit::maximumWaitNanoseconds = std::max(realtimeAudit::maximumWaitNanoseconds, waited);
    return result;
}
}
// Also catch STL allocations whose allocator lives in the shared C++ runtime.
void* operator new(std::size_t size) {
    if (void* pointer = __wrap_malloc(size ? size : 1))
        return pointer;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    return ::operator new(size);
}
void operator delete(void* pointer) noexcept {
    __wrap_free(pointer);
}
void operator delete[](void* pointer) noexcept {
    __wrap_free(pointer);
}
void operator delete(void* pointer, std::size_t) noexcept {
    __wrap_free(pointer);
}
void operator delete[](void* pointer, std::size_t) noexcept {
    __wrap_free(pointer);
}
