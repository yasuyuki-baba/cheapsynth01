#include "MidiProcessor.h"
#include "../Parameters.h"

MidiProcessor::MidiProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor(BusesProperties()),  // No audio buses
      apvts(apvts) {}

MidiProcessor::~MidiProcessor() = default;

void MidiProcessor::prepareToPlay(double, int) {}
void MidiProcessor::releaseResources() {}

void MidiProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    // This processor does not process audio, so we must clear the buffer
    // to prevent any leftover data from passing through.
    buffer.clear();

    // Process MIDI messages but don't generate output buffer
    for (const auto metadata : midiMessages) {
        auto message = metadata.getMessage();
        handleMidiEvent(message, midiMessages);
    }

    // Clear MIDI buffer as we don't generate output MIDI messages
    midiMessages.clear();
}

void MidiProcessor::handleMidiEvent(const juce::MidiMessage& midiMessage, juce::MidiBuffer&) {
    if (midiMessage.isNoteOn()) {
        handleNoteOn(midiMessage);
    } else if (midiMessage.isNoteOff()) {
        handleNoteOff(midiMessage);
    } else if (midiMessage.isPitchWheel()) {
        handlePitchWheel(midiMessage);
    } else if (midiMessage.isController()) {
        handleControllerMessage(midiMessage);
    }
    // Ignore other MIDI messages
}

void MidiProcessor::handleNoteOn(const juce::MidiMessage& midiMessage) {
    bool wasEmpty = activeNotes.isEmpty();
    activeNotes.addIfNotAlreadyThere(midiMessage.getNoteNumber());
    activeNotes.sort();

    int highestNote = activeNotes.getLast();
    float velocity = midiMessage.getVelocity() / 127.0f;

    if (soundGenerator != nullptr) {
        if (wasEmpty) {
            soundGenerator->startNote(highestNote, velocity, lastPitchWheelValue);

            // Call EG note-on only for the first note
            if (egProcessor != nullptr) {
                egProcessor->startEnvelope();
            }
        } else {
            soundGenerator->changeNote(highestNote);
        }
    }
}

void MidiProcessor::handleNoteOff(const juce::MidiMessage& midiMessage) {
    activeNotes.removeFirstMatchingValue(midiMessage.getNoteNumber());

    if (soundGenerator != nullptr) {
        if (activeNotes.isEmpty()) {
            // Always set allowTailOff = true to make sound fade gradually
            soundGenerator->stopNote(true);

            // Start EG release only when all notes are off
            if (egProcessor != nullptr) {
                egProcessor->releaseEnvelope();
            }
        } else {
            activeNotes.sort();
            int highestNote = activeNotes.getLast();
            soundGenerator->changeNote(highestNote);
        }
    }
}

void MidiProcessor::handlePitchWheel(const juce::MidiMessage& midiMessage) {
    lastPitchWheelValue = midiMessage.getPitchWheelValue();

    // Set pitch wheel value to sound generator
    if (soundGenerator != nullptr) {
        soundGenerator->pitchWheelMoved(lastPitchWheelValue);
    }

    // Also set pitch bend value to parameter
    const float bend = (lastPitchWheelValue - 8192) / 8192.0f;
    if (auto* param = apvts.getParameter(ParameterIds::pitchBend))
        param->setValueNotifyingHost(bend);
}

void MidiProcessor::updateModulationParameter() {
    int value14bit = (modulationMSB << 7) | modulationLSB;
    float normalizedValue = value14bit / 16383.0f;
    if (auto* param = apvts.getParameter(ParameterIds::modDepth))
        param->setValueNotifyingHost(normalizedValue);
}

void MidiProcessor::updateBreathParameter() {
    int value14bit = (breathMSB << 7) | breathLSB;
    float normalizedValue = value14bit / 16383.0f;
    if (auto* param = apvts.getParameter(ParameterIds::breathInput))
        param->setValueNotifyingHost(normalizedValue);
}

void MidiProcessor::updateVolumeParameter() {
    int value14bit = (volumeMSB << 7) | volumeLSB;
    float normalizedValue = value14bit / 16383.0f;
    if (auto* param = apvts.getParameter(ParameterIds::volume))
        param->setValueNotifyingHost(normalizedValue);
}

void MidiProcessor::updateGlissandoParameter() {
    int value14bit = (glissandoMSB << 7) | glissandoLSB;
    float normalizedValue = value14bit / 16383.0f;
    if (auto* param = apvts.getParameter(ParameterIds::glissando))
        param->setValueNotifyingHost(normalizedValue);
}

void MidiProcessor::handleControllerMessage(const juce::MidiMessage& midiMessage) {
    const int controller = midiMessage.getControllerNumber();
    const int value = midiMessage.getControllerValue();

    // 14bit CC MSB processing
    if (controller == 1) {          // CC #1: Modulation MSB
        modulationMSB = value;
        updateModulationParameter();
    }
    else if (controller == 2) {     // CC #2: Breath MSB
        breathMSB = value;
        updateBreathParameter();
    }
    else if (controller == 7) {     // CC #7: Volume MSB
        volumeMSB = value;
        updateVolumeParameter();
    }
    else if (controller == 35) {    // CC #35: Glissando MSB
        glissandoMSB = value;
        updateGlissandoParameter();
    }
    // 14bit CC LSB processing
    else if (controller == 33) {    // CC #33: Modulation LSB
        modulationLSB = value;
        updateModulationParameter();
    }
    else if (controller == 34) {    // CC #34: Breath LSB
        breathLSB = value;
        updateBreathParameter();
    }
    else if (controller == 37) {    // CC #37: Glissando LSB
        glissandoLSB = value;
        updateGlissandoParameter();
    }
    else if (controller == 39) {    // CC #39: Volume LSB
        volumeLSB = value;
        updateVolumeParameter();
    }
    // 7bit CC processing
    else if (controller == 11) {    // CC #11: PWM Speed
        float floatValue = value / 127.0f;
        if (auto* param = apvts.getParameter(ParameterIds::pwmSpeed))
            param->setValueNotifyingHost(floatValue);
    }
    else if (controller == 70) {    // CC #70: Sustain Level (Sound Variation)
        float floatValue = value / 127.0f;
        if (auto* param = apvts.getParameter(ParameterIds::sustain))
            param->setValueNotifyingHost(floatValue);
    }
    else if (controller == 71) {    // CC #71: Filter Resonance
        float floatValue = value / 127.0f;
        if (auto* param = apvts.getParameter(ParameterIds::resonance))
            param->setValueNotifyingHost(floatValue);
    }
    else if (controller == 73) {    // CC #73: Attack Time
        float floatValue = value / 127.0f;
        if (auto* param = apvts.getParameter(ParameterIds::attack))
            param->setValueNotifyingHost(floatValue);
    }
    else if (controller == 74) {    // CC #74: Filter Cutoff
        float floatValue = value / 127.0f;
        if (auto* param = apvts.getParameter(ParameterIds::cutoff))
            param->setValueNotifyingHost(floatValue);
    }
    else if (controller == 75) {    // CC #75: Decay Time
        float floatValue = value / 127.0f;
        if (auto* param = apvts.getParameter(ParameterIds::decay))
            param->setValueNotifyingHost(floatValue);
    }
    else if (controller == 76) {    // CC #76: LFO Speed (Vibrato Rate)
        float floatValue = value / 127.0f;
        if (auto* param = apvts.getParameter(ParameterIds::lfoSpeed))
            param->setValueNotifyingHost(floatValue);
    }
    else if (controller == 79) {    // CC #79: Release Time
        float floatValue = value / 127.0f;
        if (auto* param = apvts.getParameter(ParameterIds::release))
            param->setValueNotifyingHost(floatValue);
    }
}
