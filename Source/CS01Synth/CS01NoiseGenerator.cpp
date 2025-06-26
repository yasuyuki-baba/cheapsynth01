#include "CS01NoiseGenerator.h"

CS01NoiseGenerator::CS01NoiseGenerator()
{
}

void CS01NoiseGenerator::prepare(const juce::dsp::ProcessSpec& spec)
{
    noiseFilter.prepare(spec);
    
    // Limit frequency to not exceed Nyquist frequency
    float cutoffFreq = std::min(12000.0f, static_cast<float>(spec.sampleRate * 0.45f));
    
    *noiseFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(spec.sampleRate, cutoffFreq);
}

float CS01NoiseGenerator::getNextSample()
{
    float whiteNoise = random.nextFloat() * 2.0f - 1.0f;
    return noiseFilter.processSample(whiteNoise);
}
