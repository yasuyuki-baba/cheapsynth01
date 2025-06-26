#include "CS01LFOProcessor.h"

//==============================================================================
CS01LFOProcessor::CS01LFOProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::mono(), true)),
      apvts(apvts),
      // Modified to standard triangle wave function (corrected to exact -1.0 to 1.0 range)
      lfo([](float x) { 
          // Normalize to 0-1 range
          float phase = std::fmod(x, 1.0f);
          // Generate triangle wave in -1 to 1 range
          return 2.0f * (phase < 0.5f ? phase : 1.0f - phase) - 1.0f;
      })
{
}

CS01LFOProcessor::~CS01LFOProcessor()
{
}

//==============================================================================
void CS01LFOProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    lfo.prepare(spec);
    updateParameters();
}

void CS01LFOProcessor::releaseResources()
{
}

bool CS01LFOProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    return true;
}

void CS01LFOProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    updateParameters();

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    lfo.process(context);
}

void CS01LFOProcessor::updateParameters()
{
    auto lfoSpeed = apvts.getRawParameterValue(ParameterIds::lfoSpeed)->load();
    lfo.setFrequency(lfoSpeed);
    
    // Since juce::dsp::Oscillator<float> doesn't have a setAmplitude method,
    // we need to adjust the amplitude in the waveform function itself
    
    // We can add waveform selection later.
}
