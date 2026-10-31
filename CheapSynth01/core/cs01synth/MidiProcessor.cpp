#include "cs01synth/MidiProcessor.h"

#include "cs01synth/EGProcessor.h"
#include "cs01synth/ISoundGenerator.h"
#include "Parameters.h"

MidiProcessor::MidiProcessor(cs01::ParameterState& p) : parameters(p) {}
MidiProcessor::~MidiProcessor() = default;
void MidiProcessor::updateParameter(Control control, float normalizedValue) {
    constexpr std::array ids{
        ParameterIds::pitchBend,  ParameterIds::modDepth,   ParameterIds::breathInput,
        ParameterIds::volume,     ParameterIds::glissando,  ParameterIds::sustain,
        ParameterIds::resonance,  ParameterIds::attack,     ParameterIds::cutoff,
        ParameterIds::decay,      ParameterIds::lfoSpeed,   ParameterIds::release,
        ParameterIds::vcaEgDepth, ParameterIds::waveType,   ParameterIds::feet,
        ParameterIds::pitch,      ParameterIds::vcfEgDepth, ParameterIds::pwmSpeed,
        ParameterIds::lfoTarget,  ParameterIds::breathVcf,  ParameterIds::breathVca,
        ParameterIds::filterType};
    parameters.setNormalized(ids[static_cast<size_t>(control)], normalizedValue, true);
}

void MidiProcessor::prepare(double) {
    reset();
}
void MidiProcessor::reset() {
    activeNotes.reset();
    heldNotes.reset();
    sustainPedalDown = false;
    if (soundGenerator != nullptr)
        soundGenerator->stopNote(false);
    if (egProcessor != nullptr)
        egProcessor->stopEnvelopeImmediately();
}

void MidiProcessor::handleMidiEvent(const cs01::MidiMessage& midiMessage) {
    if (midiMessage.isAllSoundOff()) {
        reset();
    } else if (midiMessage.isAllNotesOff()) {
        heldNotes.reset();
        if (!sustainPedalDown)
            releaseUnheldNotes();
    } else if (midiMessage.isNoteOn()) {
        handleNoteOn(midiMessage);
    } else if (midiMessage.isNoteOff()) {
        handleNoteOff(midiMessage);
    } else if (midiMessage.isPitchBend()) {
        handlePitchBend(midiMessage);
    } else if (midiMessage.isChannelPressure()) {
        handleChannelPressure(midiMessage);
    } else if (midiMessage.isController()) {
        handleControllerMessage(midiMessage);
    }
    // Ignore other MIDI messages
}

void MidiProcessor::handleNoteOn(const cs01::MidiMessage& midiMessage) {
    bool wasEmpty = activeNotes.none();
    heldNotes.set(static_cast<size_t>(midiMessage.getNoteNumber()));
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
    if (!heldNotes.test(static_cast<size_t>(midiMessage.getNoteNumber())))
        return;
    heldNotes.reset(static_cast<size_t>(midiMessage.getNoteNumber()));
    if (!sustainPedalDown)
        releaseUnheldNotes();
}

void MidiProcessor::setSustainPedal(bool down) {
    if (sustainPedalDown == down)
        return;
    sustainPedalDown = down;
    if (!down)
        releaseUnheldNotes();
}

void MidiProcessor::releaseUnheldNotes() {
    const auto previous = activeNotes;
    activeNotes &= heldNotes;
    if (activeNotes == previous)
        return;
    if (activeNotes.none()) {
        if (soundGenerator != nullptr)
            soundGenerator->stopNote(true);
        if (egProcessor != nullptr)
            egProcessor->releaseEnvelope();
    } else if (soundGenerator != nullptr) {
        soundGenerator->changeNote(currentNote());
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

void MidiProcessor::handleChannelPressure(const cs01::MidiMessage& midiMessage) {
    channelPressure = midiMessage.getChannelPressureValue();
    updateBreathParameter();
}

void MidiProcessor::updateModulationParameter() {
    int value14bit = (modulationMSB << 7) | modulationLSB;
    float normalizedValue = value14bit / 16383.0f;
    updateParameter(Control::Modulation, normalizedValue);
}

void MidiProcessor::updateBreathParameter() {
    int value14bit = (breathMSB << 7) | breathLSB;
    float normalizedValue = value14bit / 16383.0f;
    updateParameter(Control::Breath, std::max(normalizedValue, channelPressure / 127.f));
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
        channelPressure = 0;
        updateModulationParameter();
        updateBreathParameter();
        handlePitchBend(cs01::MidiMessage::pitchBend(midiMessage.getChannel(), 8192));
        setSustainPedal(false);
        return;
    }
    if (controller == 64) {
        setSustainPedal(value >= 64);
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
    else {
        struct SevenBitCcMapping {
            int cc;
            Control control;
        };
        constexpr std::array<SevenBitCcMapping, 18> kSevenBitMappings{{
            {9, Control::VcaEgDepth},
            {14, Control::WaveType},
            {15, Control::Feet},
            {16, Control::Pitch},
            {70, Control::Sustain},
            {71, Control::Resonance},
            {72, Control::Release},
            {73, Control::Attack},
            {74, Control::Cutoff},
            {75, Control::Decay},
            {76, Control::LfoSpeed},
            {79, Control::Release},
            {82, Control::VcfEgDepth},
            {85, Control::PwmSpeed},
            {88, Control::LfoTarget},
            {89, Control::BreathVcf},
            {90, Control::BreathVca},
            {91, Control::FilterType},
        }};
        for (const auto& mapping : kSevenBitMappings) {
            if (mapping.cc == controller) {
                updateParameter(mapping.control, value / 127.0f);
                break;
            }
        }
    }
}
