#include "NoiseProcessor.h"

NoiseProcessor::NoiseProcessor(juce::AudioProcessorValueTreeState& vts)
    : AudioProcessor(BusesProperties()
                        .withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(vts)
{
}

void NoiseProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumOutputChannels() };
    noiseFilter.prepare(spec);
    
    // Limit frequency to not exceed Nyquist frequency
    float cutoffFreq = std::min(12000.0f, static_cast<float>(spec.sampleRate * 0.45f));
    
    *noiseFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(spec.sampleRate, cutoffFreq);
}

bool NoiseProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getChannelSet(false, 0);
    
    // Only support mono output
    if (mainOut != juce::AudioChannelSet::mono())
        return false;
    
    return true;
}

void NoiseProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    
    // Clear buffer first
    buffer.clear();
    
    // Generate white noise
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float whiteNoise = random.nextFloat() * 2.0f - 1.0f;
        float filteredNoise = noiseFilter.processSample(whiteNoise);
        
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            buffer.setSample(channel, sample, filteredNoise);
        }
    }
}
