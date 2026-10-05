#include "CS01Synth/EGProcessor.h"

//==============================================================================
EGProcessor::EGProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(apvts) {}

EGProcessor::~EGProcessor() {}

//==============================================================================
void EGProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    adsr.reset();
    lastOutput = 0.0f;
    releasing = false;
    adsr.setSampleRate(sampleRate);
    parametersInitialized = false;
    updateADSR();
}

void EGProcessor::releaseResources() {
    adsr.reset();
    lastOutput = 0.0f;
    releasing = false;
}

bool EGProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    return true;
}

void EGProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    updateADSR();

    // CS01 is a mono synth, so only generate mono output
    buffer.clear();

    // Skip MIDI message processing (already processed in MidiProcessor)
    // MIDI messages are processed by startEnvelope/releaseEnvelope methods

    // Process mono output (channel 0) only
    auto* channelData = buffer.getWritePointer(0);

    // Keep the control signal continuous. Circuit-calibrated shaping is a separate step.
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        channelData[sample] = adsr.getNextSample();
    }
    if (buffer.getNumSamples() > 0)
        lastOutput = channelData[buffer.getNumSamples() - 1];
}

void EGProcessor::updateADSR() {
    juce::ADSR::Parameters adsrParams;
    adsrParams.attack = apvts.getRawParameterValue(ParameterIds::attack)->load();
    adsrParams.decay = apvts.getRawParameterValue(ParameterIds::decay)->load();
    adsrParams.sustain = apvts.getRawParameterValue(ParameterIds::sustain)->load();
    adsrParams.release = apvts.getRawParameterValue(ParameterIds::release)->load();

    const auto& current = adsr.getParameters();
    // Defer controls unrelated to release until the next note. Updating JUCE's
    // complete parameter set would overwrite the running note-off rate.
    if (releasing && adsr.isActive() && adsrParams.release == current.release)
        return;
    if (releasing && adsr.isActive()) {
        // JUCE recalculates release from sustain and may immediately reset when
        // sustain is zero. Use the current level while changing release, then
        // let noteOff() calculate the new slope without changing that level.
        // The real sustain setting is restored by startEnvelope().
        adsrParams.sustain = lastOutput;
        adsr.setParameters(adsrParams);
        adsr.noteOff();
        parametersInitialized = true;
        return;
    }
    // Unchanged settings must not overwrite the rate calculated by noteOff()
    // from the actual envelope level (which may differ from sustain).
    if (!parametersInitialized || adsrParams.attack != current.attack ||
        adsrParams.decay != current.decay || adsrParams.sustain != current.sustain ||
        adsrParams.release != current.release) {
        adsr.setParameters(adsrParams);
        parametersInitialized = true;
    }
}
