#pragma once
#include <cstddef>

namespace realtimeAudit {
extern thread_local bool enabled;
extern thread_local std::size_t allocations, deallocations, locks;
inline void begin() {
    allocations = deallocations = locks = 0;
    enabled = true;
}
inline void end() {
    enabled = false;
}
}  // namespace realtimeAudit
