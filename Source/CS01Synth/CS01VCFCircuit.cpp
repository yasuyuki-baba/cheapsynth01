#include "CS01Synth/CS01VCFCircuit.h"

CS01VCFCircuit::CS01VCFCircuit()
    : cutoff(1000.0f),
      resonance(0.1f),
      sampleRate(0.0f)  // Unset state, will be set by prepare()
{
    reset();
}

CS01VCFCircuit::CS01VCFCircuit(double sampleRate)
    : cutoff(1000.0f), resonance(0.1f), sampleRate(static_cast<float>(sampleRate)) {
    reset();
}

void CS01VCFCircuit::reset() {
    model.reset();

    // Reset input and output stages
    inputStage.reset();
    outputStage.reset();
}

void CS01VCFCircuit::prepare(double newSampleRate) {
    sampleRate = static_cast<float>(newSampleRate);
    model.prepare(newSampleRate);

    // Prepare input and output stages
    inputStage.prepare(newSampleRate);
    outputStage.prepare(newSampleRate);
}

// Input stage processing - Clean DC blocking based on circuit diagram
float CS01VCFCircuit::processInputStage(float sample) {
    // Empirical 20 Hz, second-order DC blocker; not derived from the audio-input RC network.
    sample = inputStage.dcBlocker.processSample(sample);

    return sample;
}

// Output stage processing - Clean DC blocking based on circuit diagram
float CS01VCFCircuit::processOutputStage(float sample) {
    // Empirical coupling approximation. The schematic's 1/50 means 1 uF / 50 V,
    // not 0.02 uF. Its effective load has not been established here.
    const float cutoffFreq = 8.0f;  // Uncalibrated model value, not an RC-derived target.
    const float alpha =
        1.0f / (1.0f + 2.0f * juce::MathConstants<float>::pi * cutoffFreq / outputStage.sampleRate);

    // Clean DC blocking filter
    outputStage.prevOutput = alpha * (outputStage.prevOutput + sample - outputStage.prevInput);
    outputStage.prevInput = sample;

    return outputStage.prevOutput;
}

void CS01VCFCircuit::setCutoffFrequency(float newCutoff) {
    cutoff = juce::jlimit(20.0f, 20000.0f, newCutoff);
}

void CS01VCFCircuit::setResonance(float newResonance) {
    resonance = juce::jlimit(0.0f, 1.0f, newResonance);
}

float CS01VCFCircuit::processSample(int channel, float sample) {
    const float coupledInput = processInputStage(sample);
    const float filtered = model.processSample(coupledInput, cutoff, resonance);
    return processOutputStage(filtered);
}

void CS01VCFCircuit::processBlock(float* samples, int numSamples) {
    // Process a block of mono samples
    for (int i = 0; i < numSamples; ++i) {
        samples[i] = processSample(0, samples[i]);
    }
}

void CS01VCFCircuit::processBlock(float** channelData, int numChannels, int numSamples) {
    // Process each channel separately
    // Note: For true stereo processing, we would need separate state variables per channel
    for (int ch = 0; ch < numChannels; ++ch) {
        float* channelSamples = channelData[ch];

        // Process this channel's samples
        for (int i = 0; i < numSamples; ++i) {
            channelSamples[i] = processSample(ch, channelSamples[i]);
        }
    }
}

void CS01VCFCircuit::processBlock(float* samples, int numSamples, const float* cutoffModulation,
                                  float baseResonance) {
    const float originalCutoff = cutoff;
    const float originalResonance = resonance;
    const float boundedResonance = juce::jlimit(0.0f, 1.0f, baseResonance);
    for (int i = 0; i < numSamples; ++i) {
        const float boundedCutoff = juce::jlimit(20.0f, 20000.0f, cutoffModulation[i]);
        samples[i] = processOutputStage(model.processSample(
            processInputStage(samples[i]), boundedCutoff, boundedResonance));
    }
    cutoff = originalCutoff;
    resonance = originalResonance;
}
