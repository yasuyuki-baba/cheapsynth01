#pragma once
#include <cstdint>
namespace cs01 {
struct MidiMessage {
    uint8_t status = 0, data1 = 0, data2 = 0;
    bool isNoteOn() const {
        return (status & 0xf0) == 0x90 && data2 != 0;
    }
    bool isNoteOff() const {
        return (status & 0xf0) == 0x80 || ((status & 0xf0) == 0x90 && data2 == 0);
    }
    bool isController() const {
        return (status & 0xf0) == 0xb0;
    }
    bool isPitchWheel() const {
        return (status & 0xf0) == 0xe0;
    }
    bool isAllSoundOff() const {
        return isController() && data1 == 120;
    }
    bool isAllNotesOff() const {
        return isController() && data1 == 123;
    }
    int getNoteNumber() const {
        return data1 & 127;
    }
    int getVelocity() const {
        return data2 & 127;
    }
    int getChannel() const {
        return (status & 15) + 1;
    }
    int getPitchWheelValue() const {
        return (data1 & 127) | ((data2 & 127) << 7);
    }
    int getControllerNumber() const {
        return data1 & 127;
    }
    int getControllerValue() const {
        return data2 & 127;
    }
    static MidiMessage pitchWheel(int channel, int value) {
        return {static_cast<uint8_t>(0xe0 | ((channel - 1) & 15)),
                static_cast<uint8_t>(value & 127), static_cast<uint8_t>((value >> 7) & 127)};
    }
};
}  // namespace cs01
