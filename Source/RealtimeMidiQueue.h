#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <cmath>

// Bounded multi-producer, single-consumer short-MIDI queue. No payload allocation,
// lock or wait. A contended/full producer reports overflow instead of spinning.
// Overflow is a panic request: the consumer discards queued events and silences
// held voices. This deliberately sacrifices that block rather than lose Note Off.
class RealtimeMidiQueue {
   public:
    static constexpr size_t capacity = 2048;
    RealtimeMidiQueue() {
        for (size_t i = 0; i < capacity; ++i)
            slots[i].sequence.store(i);
        reset(44100.0);
    }
    // Preparation may overlap GUI producers or the mirror consumer. Discard
    // old generations without rewinding live ring slots or consumer indices.
    void reset(double rate) {
        sampleRate.store(rate);
        epoch.fetch_add(1, std::memory_order_acq_rel);
        previousTime.store(juce::Time::getMillisecondCounterHiRes() * 0.001);
    }
    void addMessageToQueue(const juce::MidiMessage& message) {
        if (message.getRawDataSize() > 3)
            return;
        const auto generation = epoch.load(std::memory_order_acquire);
        size_t position = writePosition.load(std::memory_order_relaxed);
        for (int attempt = 0; attempt < 8; ++attempt) {
            auto& slot = slots[position % capacity];
            const size_t sequence = slot.sequence.load(std::memory_order_acquire);
            if (sequence == position) {
                if (!writePosition.compare_exchange_weak(position, position + 1,
                                                         std::memory_order_relaxed))
                    continue;
                slot.epoch = generation;
                slot.size = message.getRawDataSize();
                std::copy_n(message.getRawData(), slot.size, slot.bytes.data());
                slot.time = message.getTimeStamp();
                slot.sequence.store(position + 1, std::memory_order_release);
                return;
            }
            if (sequence < position) {
                epoch.fetch_add(1, std::memory_order_acq_rel);
                overflow.store(true, std::memory_order_release);
                return;
            }
            position = writePosition.load(std::memory_order_relaxed);
        }
        epoch.fetch_add(1, std::memory_order_acq_rel);
        overflow.store(true, std::memory_order_release);
    }
    // Caller reserves capacity * 9 bytes in destination outside the audio callback.
    // Only current queue contents are drained, so producers cannot extend this loop.
    bool removeNextBlockOfMessages(juce::MidiBuffer& destination, int samples) {
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const double previous = previousTime.load();
        const double rate = sampleRate.load();
        const bool panic = overflow.exchange(false, std::memory_order_acq_rel);
        const size_t limit =
            std::min(writePosition.load(std::memory_order_acquire), readPosition + capacity);
        while (readPosition < limit) {
            auto& slot = slots[readPosition % capacity];
            if (slot.sequence.load(std::memory_order_acquire) != readPosition + 1)
                break;  // An unfinished producer never blocks the consumer.
            if (!panic && slot.epoch == epoch.load(std::memory_order_acquire)) {
                const double time = std::isfinite(slot.time) ? slot.time : previous;
                const int offset = juce::jlimit(
                    0, std::max(0, samples - 1),
                    static_cast<int>(std::clamp((time - previous) * rate, 0.0,
                                                static_cast<double>(std::max(0, samples - 1)))));
                destination.addEvent(slot.bytes.data(), slot.size, offset);
            }
            slot.sequence.store(readPosition + capacity, std::memory_order_release);
            ++readPosition;
        }
        previousTime = now;
        return panic;
    }

   private:
    struct Slot {
        std::atomic<size_t> sequence{0};
        std::array<juce::uint8, 3> bytes{};
        int size = 0;
        double time = 0;
        size_t epoch = 0;
    };
    std::array<Slot, capacity> slots;
    std::atomic<size_t> writePosition{0};
    size_t readPosition = 0;
    std::atomic<bool> overflow{false};
    std::atomic<size_t> epoch{0};
    std::atomic<double> sampleRate{44100.0}, previousTime{0.0};
    static_assert(std::atomic<double>::is_always_lock_free);
    static_assert(std::atomic<size_t>::is_always_lock_free);
};
