#pragma once
#include "DSP/MidiMessage.h"
#include <array>
#include <algorithm>
namespace cs01 {
// Audio-thread queue. Stable ordering for equal offsets; never allocates.
class MidiQueue {
   public:
    static constexpr int capacity = 2048;
    struct Event {
        int offset;
        MidiMessage message;
    };
    void add(int offset, MidiMessage message) {
        if (overflow)
            return;
        if (count == capacity) {
            overflow = true;
            count = front = 0;
            return;
        }
        int i = count++;
        while (i > front && events[i - 1].offset > offset) {
            events[i] = events[i - 1];
            --i;
        }
        events[i] = {std::max(0, offset), message};
    }
    bool empty() const {
        return front == count;
    }
    const Event& peek() const {
        return events[front];
    }
    void remove() {
        ++front;
    }
    void flush(int frames) {
        int kept = 0;
        for (int i = front; i < count; ++i) {
            events[kept] = events[i];
            events[kept++].offset -= frames;
        }
        count = kept;
        front = 0;
    }
    bool takeOverflow() {
        const bool result = overflow;
        overflow = false;
        return result;
    }
    void clear() {
        count = front = 0;
        overflow = false;
    }
    bool containsPitchBend(int frames) const {
        for (int i = front; i < count; ++i)
            if (events[i].offset < frames && events[i].message.isPitchWheel())
                return true;
        return false;
    }

   private:
    std::array<Event, capacity> events{};
    int count = 0, front = 0;
    bool overflow = false;
};
}  // namespace cs01
