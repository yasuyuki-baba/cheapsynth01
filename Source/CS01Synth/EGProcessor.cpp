#include "EGProcessor.h"

//==============================================================================
EGProcessor::EGProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(apvts) {}

EGProcessor::~EGProcessor() {}

//==============================================================================
void EGProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    adsr.setSampleRate(sampleRate);
    updateADSR();
}

void EGProcessor::releaseResources() {}

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
}

void EGProcessor::updateADSR() {
    juce::ADSR::Parameters adsrParams;
    adsrParams.attack = apvts.getRawParameterValue(ParameterIds::attack)->load();
    adsrParams.decay = apvts.getRawParameterValue(ParameterIds::decay)->load();
    adsrParams.sustain = apvts.getRawParameterValue(ParameterIds::sustain)->load();
    adsrParams.release = apvts.getRawParameterValue(ParameterIds::release)->load();

    adsr.setParameters(adsrParams);
}
