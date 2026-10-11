#include "CS01Synth/LFOProcessor.h"

#include "MidiParameterValue.h"

//==============================================================================
LFOProcessor::LFOProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(apvts),
      lfo() {
    // JUCE supplies phase in [-pi, pi], not a normalized [0, 1) phase.
    lfo.initialise([](double phase) -> double {
        return 1.0 - 2.0 * std::abs(phase) / juce::MathConstants<double>::pi;
    });
}

LFOProcessor::~LFOProcessor() {}

//==============================================================================
void LFOProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    lfo.prepare(spec);
    updateParameters();
}

void LFOProcessor::releaseResources() {}

bool LFOProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    return true;
}

void LFOProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    updateParameters();

    // CS01 is a mono synth, so only generate mono output
    buffer.clear();

    // Keep phase accumulation in double precision, including at the internal
    // oversampled rate; convert only the control output to float.
    auto* output = buffer.getWritePointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        output[i] = static_cast<float>(lfo.processSample(0.0));

    // LFO output is written directly to the mono buffer
}

void LFOProcessor::updateParameters() {
    auto lfoSpeed = getMidiParameterValue(apvts, ParameterIds::lfoSpeed);
    lfo.setFrequency(lfoSpeed);
}
