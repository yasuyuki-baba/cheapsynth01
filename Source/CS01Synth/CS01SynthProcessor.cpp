#include "CS01SynthProcessor.h"

CS01SynthProcessor::CS01SynthProcessor(juce::AudioProcessorValueTreeState& vts)
    : AudioProcessor(BusesProperties()
                        .withInput("LFOInput", juce::AudioChannelSet::mono(), false)
                        .withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(vts),
      voice(vts)
{
}

void CS01SynthProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumOutputChannels() };
    voice.prepare(spec);
}

bool CS01SynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getChannelSet(true, 0);
    const auto& mainOut = layouts.getChannelSet(false, 0);

    if (mainIn != juce::AudioChannelSet::mono()) return false;
    if (mainOut != juce::AudioChannelSet::mono()) return false;

    return true;
}

void CS01SynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // Process LFO input
    auto lfoInput = getBusBuffer(buffer, true, 0);
    float lfoValue = lfoInput.getNumSamples() > 0 ? lfoInput.getSample(0, 0) : 0.0f;

    auto modDepth = apvts.getRawParameterValue(ParameterIds::modDepth)->load();
    const float lfoModRangeSemitones = 1.0f;
    float finalLfoValue = lfoValue * modDepth * lfoModRangeSemitones;

    // Clear audio buffer
    buffer.clear();

    // Sound generation
    if (voice.isActive())
    {
        voice.setLfoValue(finalLfoValue);
        voice.renderNextBlock(buffer, 0, buffer.getNumSamples());
    }

    // Pass MIDI buffer through (processing is done in CS01MidiProcessor)
}
