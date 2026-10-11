#pragma once
#include "MidiMessage.h"
#include <bitset>

namespace cs01 {
// Audio-owned source tracking. The synth is omni and monophonic; ownership is
// therefore per pitch, matching MidiProcessor's held-key semantics.
class NoteOwnership {
   public:
    template <typename Send>
    void process(MidiMessage message, bool fromUI, Send send) {
        if (message.isAllSoundOff() || message.isAllNotesOff()) {
            reset();
            send(message);
            return;
        }
        auto& owner = fromUI ? ui : external;
        const auto note = static_cast<size_t>(message.getNoteNumber());
        if (message.isNoteOn()) {
            owner.set(note);
            send(message);
        } else if (message.isNoteOff()) {
            if (!owner.test(note))
                return;
            owner.reset(note);
            if (!ui.test(note) && !external.test(note))
                send(message);
        } else {
            send(message);
        }
    }
    void reset() {
        ui.reset();
        external.reset();
    }
    const std::bitset<128>& externalNotes() const {
        return external;
    }

   private:
    std::bitset<128> ui, external;
};
}  // namespace cs01
