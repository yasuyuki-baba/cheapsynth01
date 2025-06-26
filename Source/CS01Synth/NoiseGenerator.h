#pragma once

#include <JuceHeader.h>

class NoiseGenerator
{
public:
    NoiseGenerator();

    void prepare(const juce::dsp::ProcessSpec& spec);
    float getNextSample();

private:
    juce::Random random;
    juce::dsp::IIR::Filter<float> noiseFilter;
};
