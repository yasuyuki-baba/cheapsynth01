#pragma once

#include "dsp/MidiMessage.h"
#include "Parameters.h"

#include <array>
#include <atomic>
#include <bitset>

class EGProcessor;
class ISoundGenerator;

class MidiProcessor {
   public:
    MidiProcessor(cs01::ParameterState& parameters);
    ~MidiProcessor();

    void prepare(double sampleRate);
    void reset();
    void handleMidiEvent(const cs01::MidiMessage&);

    // Set sound generator
    void setSoundGenerator(ISoundGenerator* generator) {
        soundGenerator = generator;
    }

    // Set EG processor
    void setEGProcessor(EGProcessor* processor) {
        egProcessor = processor;
    }

    // Get currently playing note
    int currentNote() const {
        for (int note = 127; note >= 0; --note)
            if (activeNotes[static_cast<size_t>(note)])
                return note;
        return 0;
    }

    // Get sound generator
    ISoundGenerator* getSoundGenerator() const {
        return soundGenerator;
    }

    // Audio-thread inspection only; includes notes retained by the sustain pedal.
    const std::bitset<128>& getActiveNotes() const {
        return activeNotes;
    }

   private:
    enum class Control { PitchBend, Modulation, Breath, Volume, Glissando, Count };
    cs01::ParameterState& parameters;
    void updateParameter(Control control, float normalizedValue);

    // MIDI processing methods
    void handleNoteOn(const cs01::MidiMessage& midiMessage);
    void handleNoteOff(const cs01::MidiMessage& midiMessage);
    void handlePitchBend(const cs01::MidiMessage& midiMessage);
    void handleChannelPressure(const cs01::MidiMessage& midiMessage);
    void handleControllerMessage(const cs01::MidiMessage& midiMessage);
    void setSustainPedal(bool down);
    void releaseUnheldNotes();

    // 14bit CC parameter update methods
    void updateModulationParameter();
    void updateBreathParameter();
    void updateVolumeParameter();
    void updateGlissandoParameter();

    ISoundGenerator* soundGenerator = nullptr;
    EGProcessor* egProcessor = nullptr;

    // For monophonic sound management
    std::bitset<128> activeNotes;
    std::bitset<128> heldNotes;
    bool sustainPedalDown = false;
    int lastPitchBend = 8192;  // Center value
    int channelPressure = 0;

    // 14bit CC values storage
    int modulationMSB = 0, modulationLSB = 0;  // CC #1/#33
    int breathMSB = 0, breathLSB = 0;          // CC #2/#34
    int volumeMSB = 0, volumeLSB = 0;          // CC #7/#39
    int glissandoMSB = 0, glissandoLSB = 0;    // CC #5/#37
};
