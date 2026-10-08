#include "CS01Synth/MidiProcessor.h"

#include "CS01Synth/EGProcessor.h"
#include "CS01Synth/ISoundGenerator.h"
#include "Parameters.h"

#include <typeinfo>

MidiProcessor::MidiProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor(BusesProperties()) {  // No audio buses
    static_assert(std::atomic<float>::is_always_lock_free);
    static_assert(std::atomic<bool>::is_always_lock_free);
    const std::array ids{
        ParameterIds::pitchBend, ParameterIds::modDepth,  ParameterIds::breathInput,
        ParameterIds::volume,    ParameterIds::glissando, ParameterIds::sustain,
        ParameterIds::resonance, ParameterIds::attack,    ParameterIds::cutoff,
        ParameterIds::decay,     ParameterIds::lfoSpeed,  ParameterIds::release};
    for (size_t i = 0; i < controls.size(); ++i) {
        auto* parameter = apvts.getParameter(ids[i]);
        // Only the standard float class has the empty valueChanged hook we
        // rely on. A future custom parameter must be audited before MIDI use.
        if (parameter != nullptr && typeid(*parameter) == typeid(juce::AudioParameterFloat))
            controls[i].parameter = parameter;
        else
            jassert(parameter == nullptr);
    }
    // Polling avoids AsyncUpdater's potentially blocking message post on the audio thread.
    startTimerHz(60);
}

MidiProcessor::~MidiProcessor() {
    stopTimer();
}

void MidiProcessor::updateParameter(Control control, float normalizedValue) {
    auto& state = controls[static_cast<size_t>(control)];
    if (state.parameter == nullptr)
        return;
    // Standard AudioParameterFloat::setValue only stores its atomic value and
    // calls the empty valueChanged hook. DSP reads that value directly; APVTS's
    // listener-maintained raw cache is deliberately deferred to the timer.
    state.parameter->setValue(normalizedValue);
    state.pending.store(true, std::memory_order_release);
}

void MidiProcessor::timerCallback() {
    for (auto& state : controls) {
        if (state.pending.exchange(false, std::memory_order_acquire)) {
            // Notify the current value without writing a snapshot back. A MIDI
            // update or host edit during dispatch cannot be rolled back here.
            state.parameter->sendValueChangedMessageToListeners(state.parameter->getValue());
        }
    }
}

void MidiProcessor::prepareToPlay(double, int) {
    releaseResources();
}
void MidiProcessor::releaseResources() {
    activeNotes.reset();
    if (soundGenerator != nullptr)
        soundGenerator->stopNote(false);
    if (egProcessor != nullptr)
        egProcessor->stopEnvelopeImmediately();
}

void MidiProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    // This processor does not process audio, so we must clear the buffer
    // to prevent any leftover data from passing through.
    buffer.clear();

    // Process MIDI messages but don't generate output buffer
    for (const auto metadata : midiMessages) {
        // Supported channel events fit MidiMessage inline storage; never copy SysEx.
        if (metadata.numBytes <= 3)
            handleMidiEvent(metadata.getMessage());
    }

    // Clear MIDI buffer as we don't generate output MIDI messages
    midiMessages.clear();
}

void MidiProcessor::processShortEvent(const juce::MidiMessage& message) {
    handleMidiEvent(message);
}

void MidiProcessor::handleMidiEvent(const juce::MidiMessage& midiMessage) {
    if (midiMessage.isAllSoundOff()) {
        releaseResources();
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
    } else if (midiMessage.isPitchWheel()) {
        handlePitchWheel(midiMessage);
    } else if (midiMessage.isController()) {
        handleControllerMessage(midiMessage);
    }
    // Ignore other MIDI messages
}

void MidiProcessor::handleNoteOn(const juce::MidiMessage& midiMessage) {
    bool wasEmpty = activeNotes.none();
    activeNotes.set(static_cast<size_t>(midiMessage.getNoteNumber()));

    int highestNote = getCurrentlyPlayingNote();
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
    // An unmatched key release must not restart an already running release.
    if (!activeNotes.test(static_cast<size_t>(midiMessage.getNoteNumber())))
        return;
    activeNotes.reset(static_cast<size_t>(midiMessage.getNoteNumber()));

    if (soundGenerator != nullptr) {
        if (activeNotes.none()) {
            // Always set allowTailOff = true to make sound fade gradually
            soundGenerator->stopNote(true);

            // Start EG release only when all notes are off
            if (egProcessor != nullptr) {
                egProcessor->releaseEnvelope();
            }
        } else {
            int highestNote = getCurrentlyPlayingNote();
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

    const float displacement = static_cast<float>(lastPitchWheelValue - 8192);
    const float position = displacement / (displacement >= 0.0f ? 8191.0f : 8192.0f);
    if (auto* parameter = controls[static_cast<size_t>(Control::PitchBend)].parameter)
        updateParameter(Control::PitchBend, parameter->convertTo0to1(position));
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

void MidiProcessor::handleControllerMessage(const juce::MidiMessage& midiMessage) {
    const int controller = midiMessage.getControllerNumber();
    const int value = midiMessage.getControllerValue();

    if (controller == 121) {
        // Reset performance controls, not the patch, volume or held keys.
        modulationMSB = modulationLSB = 0;
        breathMSB = breathLSB = 0;
        updateModulationParameter();
        updateBreathParameter();
        handlePitchWheel(juce::MidiMessage::pitchWheel(midiMessage.getChannel(), 8192));
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
