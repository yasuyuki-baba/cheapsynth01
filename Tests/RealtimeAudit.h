#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace realtimeAudit {
extern thread_local bool enabled;
extern thread_local std::size_t allocations, deallocations, locks, contendedLocks;
extern thread_local std::uint64_t waitNanoseconds, maximumWaitNanoseconds;
extern std::atomic<std::size_t> observedContentions;
inline void begin() {
    allocations = deallocations = locks = 0;
    contendedLocks = 0;
    waitNanoseconds = maximumWaitNanoseconds = 0;
    enabled = true;
}
inline void end() {
    enabled = false;
}
}  // namespace realtimeAudit
