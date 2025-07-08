#include "LFOProcessor.h"

//==============================================================================
LFOProcessor::LFOProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(apvts),
      lfo() {
    // Initialize triangle wave (standard implementation based on JUCE tutorial)
    lfo.initialise(
        [](float x) -> float {
            // x is in [0, 1), normalized by the Oscillator class
            // Standard triangle wave implementation
            return 1.0f - 4.0f * std::abs(std::round(x - 0.25f) - (x - 0.25f));
        },
        128);
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

    // Use JUCE DSP module to generate LFO signal (mono output)
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    lfo.process(context);

    // LFO output is written directly to the mono buffer
}

void LFOProcessor::updateParameters() {
    auto lfoSpeed = apvts.getRawParameterValue(ParameterIds::lfoSpeed)->load();
    lfo.setFrequency(lfoSpeed);
}
