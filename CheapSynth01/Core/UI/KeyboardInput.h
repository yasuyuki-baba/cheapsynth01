#pragma once
#include "DSP/MidiMessage.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cstdint>
#include <string_view>

namespace cs01 {
// UI-thread owned key map. Key releases use the pitch captured on key-down;
// octave switches release PC keys and do not affect mouse/external MIDI notes.
class QwertyKeyboard {
   public:
    QwertyKeyboard() {
        held.fill(-1);
    }
    template <typename Send>
    bool handle(int key, bool up, bool modified, Send send) {
        if (key >= 'a' && key <= 'z')
            key -= 'a' - 'A';
        constexpr std::string_view keys = "AWSEDFTGYHUJKOL";
        const auto index = keys.find(static_cast<char>(key));
        if (index != keys.npos && key >= 0 && key <= 127) {
            auto& note = held[index];
            if (up) {
                if (note >= 0)
                    send(note, 0);
                note = -1;
            } else if (!modified && note < 0) {
                note = base + static_cast<int>(index);
                send(note, 127);
            }
            return !modified || up;
        }
        if ((key == 'Z' || key == 'X') && !modified) {
            if (up)
                octaveKeyDown[key == 'X'] = false;
            else if (!octaveKeyDown[key == 'X']) {
                octaveKeyDown[key == 'X'] = true;
                releaseNotes(send);
                base = std::clamp(base + (key == 'X' ? 12 : -12), 24, 96);
            }
            return true;
        }
        return false;
    }
    template <typename Send>
    void release(Send send) {
        releaseNotes(send);
        octaveKeyDown.fill(false);
    }
    int octaveBase() const {
        return base;
    }

   private:
    template <typename Send>
    void releaseNotes(Send send) {
        for (auto& note : held) {
            if (note >= 0)
                send(note, 0);
            note = -1;
        }
    }
    std::array<int, 15> held{};
    std::array<bool, 2> octaveKeyDown{};
    int base = 48;
};

// One UI producer publishes desired PC/mouse ownership; audio consumes bounded
// per-note transitions. No queue can overflow or strand a lost note-off. UI
// events coalesce until the next block, like other live UI parameter updates.
class UiNoteBridge {
   public:
    enum Source : unsigned { PC = 1, Mouse = 2 };
    void set(int note, int velocity, Source source) {
        if (note < 0 || note > 127)
            return;
        auto value = desired[note].load(std::memory_order_relaxed);
        const auto owners = (value & 3u & ~source) | (velocity > 0 ? source : 0u);
        const auto level =
            velocity > 0 ? static_cast<unsigned>(std::clamp(velocity, 1, 127)) : value >> 8;
        desired[note].store(owners ? owners | (level << 8) : 0, std::memory_order_release);
    }
    void releaseAll() {
        for (auto& value : desired)
            value.store(0, std::memory_order_release);
    }
    template <typename Send>
    void consume(Send send) {
        for (unsigned note = 0; note < 128; ++note) {
            const auto value = desired[note].load(std::memory_order_acquire);
            if (value && !applied[note])
                send(MidiMessage{0x90, static_cast<uint8_t>(note),
                                 static_cast<uint8_t>(value >> 8)});
            else if (!value && applied[note])
                send(MidiMessage{0x80, static_cast<uint8_t>(note), 0});
            applied[note] = value;
        }
    }
    void publishExternal(const std::bitset<128>& notes) {
        for (unsigned word = 0; word < 4; ++word) {
            uint32_t bits = 0;
            for (unsigned bit = 0; bit < 32; ++bit)
                bits |= static_cast<uint32_t>(notes[word * 32 + bit]) << bit;
            external[word].store(bits, std::memory_order_release);
        }
    }
    bool displayed(int note) const {
        return desired[note].load(std::memory_order_acquire) != 0 ||
               (external[note / 32].load(std::memory_order_acquire) & (1u << (note % 32)));
    }
    // Audio reset deliberately forgets previously applied UI notes. UI desired
    // notes will be re-established at a subsequent consume, without cross-thread writes.
    void resetAudio() {
        applied.fill(0);
    }

   private:
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    std::array<std::atomic<unsigned>, 128> desired{};
    std::array<unsigned, 128> applied{};
    std::array<std::atomic<uint32_t>, 4> external{};
};
}  // namespace cs01
