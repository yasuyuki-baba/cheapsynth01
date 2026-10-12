#pragma once
#include "ProgramManager.h"
#include <memory>

namespace cs01 {
// Stable, per-instance names/indices. Saved user patches publish immutable sound
// revisions; retain old revisions until instance teardown. Only request/apply are
// real-time paths: no disk, XML, locks, allocation or reclamation.
class HostProgramBank {
   public:
    explicit HostProgramBank(std::vector<PreparedProgram> programs)
        : catalogue(std::move(programs)),
          sounds(std::make_unique<std::atomic<const PreparedProgram*>[]>(catalogue.size())) {
        for (size_t i = 0; i < catalogue.size(); ++i)
            sounds[i].store(&catalogue[i], std::memory_order_relaxed);
    }
    const std::vector<PreparedProgram>& programs() const {
        return catalogue;
    }
    // Main-thread save path only. Metadata and host program numbers stay fixed.
    bool updateUserProgram(const PreparedProgram& saved) {
        if (!saved.valid || saved.identity.type != PresetType::User || saved.mask >> parameterCount)
            return false;
        for (int i = 0; i < parameterCount; ++i)
            if ((saved.mask & (1u << i)) && !std::isfinite(saved.values[i]))
                return false;
        for (size_t i = 0; i < catalogue.size(); ++i) {
            if (catalogue[i].identity.type != PresetType::User ||
                catalogue[i].identity.filename != saved.identity.filename)
                continue;
            auto revision = std::make_unique<PreparedProgram>(saved);
            revision->identity = catalogue[i].identity;
            const auto* published = revision.get();
            revisions.push_back(std::move(revision));
            sounds[i].store(published, std::memory_order_release);
            // A queued selection of an older revision must not undo this save.
            cancel();
            return true;
        }
        return false;
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
        if (index < 0 || index >= static_cast<int>(catalogue.size()) ||
            !sounds[index].load(std::memory_order_acquire)->valid)
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
        const auto* program = sounds[index].load(std::memory_order_acquire);
        if (!parameters.trySetPatchIf(
                program->values, program->mask, [this, token, index, program] {
                    return requested.load(std::memory_order_acquire) == token &&
                           sounds[index].load(std::memory_order_acquire) == program;
                }))
            return -1;
        applied.store(token, std::memory_order_release);
        return index;
    }
    const PreparedProgram* selected() const noexcept {
        const auto token = requested.load(std::memory_order_acquire);
        return !token ? nullptr
                      : sounds[static_cast<unsigned>(token) - 1].load(std::memory_order_acquire);
    }
    struct Selection {
        const PreparedProgram* program;
        bool pending;
        uint64_t token = 0;
    };
    Selection selection() const noexcept {
        const auto token = requested.load(std::memory_order_acquire);
        return {!token ? nullptr
                       : sounds[static_cast<unsigned>(token) - 1].load(std::memory_order_acquire),
                token && token != applied.load(std::memory_order_acquire), token};
    }
    // Serialization is non-real-time. Retry if a request/cancellation changed
    // while taking the manager's locked XML/identity snapshot.
    std::string serializeSession(const ProgramManager& manager) const {
        for (;;) {
            const auto selected = selection();
            auto xml = manager.getStateInformation(selected.program, selected.pending);
            if (selection().token == selected.token)
                return xml;
            std::this_thread::yield();
        }
    }
    bool pending() const noexcept {
        const auto token = requested.load(std::memory_order_acquire);
        return token && token != applied.load(std::memory_order_acquire);
    }
    // Cancel before publishing an explicit patch. Requests already holding the
    // parameter writer finish first; acquired-but-uncommitted tokens recheck.
    void cancel() noexcept {
        requested.store(0, std::memory_order_release);
        applied.store(0, std::memory_order_release);
    }

   private:
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    static_assert(std::atomic<const PreparedProgram*>::is_always_lock_free);
    const std::vector<PreparedProgram> catalogue;
    std::unique_ptr<std::atomic<const PreparedProgram*>[]> sounds;
    std::vector<std::unique_ptr<PreparedProgram>> revisions;
    std::atomic<uint64_t> requested{0}, applied{0};
    std::atomic<unsigned> sequence{0};
};
}  // namespace cs01
