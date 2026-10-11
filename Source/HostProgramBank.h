#pragma once
#include "ProgramManager.h"

namespace cs01 {
// Immutable, per-instance catalogue. Construct before exposing the plugin to the
// host; never resize it while requests/rendering can run. Only request/apply are
// real-time paths: no disk, XML, locks, allocation or reclamation.
class HostProgramBank {
   public:
    explicit HostProgramBank(std::vector<PreparedProgram> programs)
        : catalogue(std::move(programs)) {}
    const std::vector<PreparedProgram>& programs() const {
        return catalogue;
    }
    static std::array<char, 12> requestChunk(unsigned index) noexcept {
        std::array<char, 12> bytes{'C', 'S', 'H', 'P', 1, 0, 0, 0};
        for (unsigned i = 0; i < 4; ++i)
            bytes[8 + i] = static_cast<char>(index >> (8 * i));
        return bytes;
    }
    bool requestChunk(std::string_view bytes) noexcept {
        if (bytes.size() != 12 || bytes.substr(0, 8) != std::string_view("CSHP\1\0\0\0", 8))
            return false;
        unsigned index = 0;
        for (unsigned i = 0; i < 4; ++i)
            index |= static_cast<unsigned>(static_cast<unsigned char>(bytes[8 + i])) << (8 * i);
        if (index >= catalogue.size())
            return false;
        return request(static_cast<int>(index));
    }
    bool request(int index) noexcept {
        if (index < 0 || index >= static_cast<int>(catalogue.size()) || !catalogue[index].valid)
            return false;
        const uint64_t token = (static_cast<uint64_t>(sequence.fetch_add(1) + 1) << 32) |
                               static_cast<unsigned>(index + 1);
        requested.store(token, std::memory_order_release);
        return true;
    }
    int applyPending(ParameterState& parameters) noexcept {
        const uint64_t token = requested.load(std::memory_order_acquire);
        if (!token || token == applied.load(std::memory_order_relaxed))
            return -1;
        const int index = static_cast<int>(static_cast<unsigned>(token)) - 1;
        const auto& program = catalogue[index];
        for (int i = 0; i < parameterCount; ++i)
            if (program.mask & (1u << i))
                parameters.set(static_cast<Param>(i), program.values[i]);
        applied.store(token, std::memory_order_release);
        return index;
    }
    const PreparedProgram* selected() const noexcept {
        const auto token = requested.load(std::memory_order_acquire);
        return !token ? nullptr : &catalogue[static_cast<unsigned>(token) - 1];
    }
    struct Selection {
        const PreparedProgram* program;
        bool pending;
    };
    Selection selection() const noexcept {
        const auto token = requested.load(std::memory_order_acquire);
        return {!token ? nullptr : &catalogue[static_cast<unsigned>(token) - 1],
                token != applied.load(std::memory_order_acquire)};
    }
    bool pending() const noexcept {
        return requested.load(std::memory_order_acquire) != applied.load(std::memory_order_acquire);
    }
    // Supersede future application of a host request. An in-flight audio block
    // can finish applying its already acquired snapshot; no snapshot is freed.
    void cancel() noexcept {
        requested.store(0, std::memory_order_release);
        applied.store(0, std::memory_order_release);
    }

   private:
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    const std::vector<PreparedProgram> catalogue;
    std::atomic<uint64_t> requested{0}, applied{0};
    std::atomic<unsigned> sequence{0};
};
}  // namespace cs01
