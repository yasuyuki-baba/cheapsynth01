#include "cs01synth/MidiProcessor.h"

#include "cs01synth/EGProcessor.h"
#include "cs01synth/ISoundGenerator.h"
#include "Parameters.h"

#include <typeinfo>

MidiProcessor::MidiProcessor(cs01::ParameterState& p) : parameters(p) {}
MidiProcessor::~MidiProcessor() = default;
void MidiProcessor::updateParameter(Control control, float normalizedValue) {
    constexpr std::array ids{
        ParameterIds::pitchBend, ParameterIds::modDepth,  ParameterIds::breathInput,
        ParameterIds::volume,    ParameterIds::glissando, ParameterIds::sustain,
        ParameterIds::resonance, ParameterIds::attack,    ParameterIds::cutoff,
        ParameterIds::decay,     ParameterIds::lfoSpeed,  ParameterIds::release};
    parameters.setNormalized(ids[static_cast<size_t>(control)], normalizedValue, true);
}

void MidiProcessor::prepare(double) {
    reset();
}
void MidiProcessor::reset() {
    activeNotes.reset();
    if (soundGenerator != nullptr)
        soundGenerator->stopNote(false);
    if (egProcessor != nullptr)
        egProcessor->stopEnvelopeImmediately();
}

void MidiProcessor::handleMidiEvent(const cs01::MidiMessage& midiMessage) {
    if (midiMessage.isAllSoundOff()) {
        reset();
    } else if (midiMessage.isAllNotesOff()) {
        if (activeNotes.any()) {
            activeNotes.reset();
            if (soundGenerator != nullptr)
                soundGenerator->stopNote(true);
            if (egProcessor != nullptr)
                egProcessor->releaseEnvelope();
        }
    } else if (midiMessage.isNoteOn()) {
        handleNoteOn(midiMessage);
    } else if (midiMessage.isNoteOff()) {
        handleNoteOff(midiMessage);
    } else if (midiMessage.isPitchBend()) {
        handlePitchBend(midiMessage);
    } else if (midiMessage.isController()) {
        handleControllerMessage(midiMessage);
    }
    // Ignore other MIDI messages
}

void MidiProcessor::handleNoteOn(const cs01::MidiMessage& midiMessage) {
    bool wasEmpty = activeNotes.none();
    activeNotes.set(static_cast<size_t>(midiMessage.getNoteNumber()));

    int highestNote = currentNote();
    float velocity = midiMessage.getVelocity() / 127.0f;

    if (soundGenerator != nullptr) {
        if (wasEmpty) {
            soundGenerator->startNote(highestNote, velocity, lastPitchBend);

            // Call EG note-on only for the first note
            if (egProcessor != nullptr) {
                egProcessor->startEnvelope();
            }
        } else {
            soundGenerator->changeNote(highestNote);
        }
    }
}

void MidiProcessor::handleNoteOff(const cs01::MidiMessage& midiMessage) {
    // An unmatched key release must not restart an already running release.
    if (!activeNotes.test(static_cast<size_t>(midiMessage.getNoteNumber())))
        return;
    activeNotes.reset(static_cast<size_t>(midiMessage.getNoteNumber()));

    if (soundGenerator != nullptr) {
        if (activeNotes.none()) {
            // Always set allowReleaseTail = true to make sound fade gradually
            soundGenerator->stopNote(true);

            // Start EG release only when all notes are off
            if (egProcessor != nullptr) {
                egProcessor->releaseEnvelope();
            }
        } else {
            int highestNote = currentNote();
            soundGenerator->changeNote(highestNote);
        }
    }
}

void MidiProcessor::handlePitchBend(const cs01::MidiMessage& midiMessage) {
    lastPitchBend = midiMessage.getPitchBendValue();

    // Set pitch bend value to sound generator
    if (soundGenerator != nullptr) {
        soundGenerator->setPitchBend(lastPitchBend);
    }

    const float displacement = static_cast<float>(lastPitchBend - 8192);
    const float position = displacement / (displacement >= 0.0f ? 8191.0f : 8192.0f);
    updateParameter(Control::PitchBend, cs01::toNormalized(ParameterIds::pitchBend, position));
}

void MidiProcessor::updateModulationParameter() {
    int value14bit = (modulationMSB << 7) | modulationLSB;
    float normalizedValue = value14bit / 16383.0f;
    updateParameter(Control::Modulation, normalizedValue);
}

void MidiProcessor::updateBreathParameter() {
    int value14bit = (breathMSB << 7) | breathLSB;
    float normalizedValue = value14bit / 16383.0f;
    updateParameter(Control::Breath, normalizedValue);
}

void MidiProcessor::updateVolumeParameter() {
    int value14bit = (volumeMSB << 7) | volumeLSB;
    float normalizedValue = value14bit / 16383.0f;
    updateParameter(Control::Volume, normalizedValue);
}

void MidiProcessor::updateGlissandoParameter() {
    int value14bit = (glissandoMSB << 7) | glissandoLSB;
    float normalizedValue = value14bit / 16383.0f;
    updateParameter(Control::Glissando, normalizedValue);
}

void MidiProcessor::handleControllerMessage(const cs01::MidiMessage& midiMessage) {
    const int controller = midiMessage.getControllerNumber();
    const int value = midiMessage.getControllerValue();

    if (controller == 121) {
        // Reset performance controls, not the patch, volume or held keys.
        modulationMSB = modulationLSB = 0;
        breathMSB = breathLSB = 0;
        updateModulationParameter();
        updateBreathParameter();
        handlePitchBend(cs01::MidiMessage::pitchBend(midiMessage.getChannel(), 8192));
        return;
    }

    // 14bit CC MSB processing
    if (controller == 1) {  // CC #1: Modulation MSB
        modulationMSB = value;
        updateModulationParameter();
    } else if (controller == 2) {  // CC #2: Breath MSB
        breathMSB = value;
        updateBreathParameter();
    } else if (controller == 7) {  // CC #7: Volume MSB
        volumeMSB = value;
        updateVolumeParameter();
    } else if (controller == 5) {  // CC #5: Portamento Time MSB (discrete glissando)
        glissandoMSB = value;
        updateGlissandoParameter();
    }
    // 14bit CC LSB processing
    else if (controller == 33) {  // CC #33: Modulation LSB
        modulationLSB = value;
        updateModulationParameter();
    } else if (controller == 34) {  // CC #34: Breath LSB
        breathLSB = value;
        updateBreathParameter();
    } else if (controller == 37) {  // CC #37: Glissando LSB
        glissandoLSB = value;
        updateGlissandoParameter();
    } else if (controller == 39) {  // CC #39: Volume LSB
        volumeLSB = value;
        updateVolumeParameter();
    }
    // 7bit CC processing
    else if (controller == 70) {  // CC #70: Sustain Level (Sound Variation)
        float floatValue = value / 127.0f;
        updateParameter(Control::Sustain, floatValue);
    } else if (controller == 71) {  // CC #71: Filter Resonance
        float floatValue = value / 127.0f;
        updateParameter(Control::Resonance, floatValue);
    } else if (controller == 73) {  // CC #73: Attack Time
        float floatValue = value / 127.0f;
        updateParameter(Control::Attack, floatValue);
    } else if (controller == 74) {  // CC #74: Filter Cutoff
        float floatValue = value / 127.0f;
        updateParameter(Control::Cutoff, floatValue);
    } else if (controller == 75) {  // CC #75: Decay Time
        float floatValue = value / 127.0f;
        updateParameter(Control::Decay, floatValue);
    } else if (controller == 76) {  // CC #76: LFO Speed (Vibrato Rate)
        float floatValue = value / 127.0f;
        updateParameter(Control::LfoSpeed, floatValue);
    } else if (controller == 79) {  // CC #79: Release Time
        float floatValue = value / 127.0f;
        updateParameter(Control::Release, floatValue);
    }
}
