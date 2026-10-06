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
    model.setCutoffFrequency(cutoff);
    model.setResonance(resonance);
}

void CS01VCFCircuit::reset() {
    model.reset();
    experimentalModel.reset();

    // Reset input and output stages
    inputStage.reset();
    outputStage.reset();
}

void CS01VCFCircuit::prepare(double newSampleRate) {
    sampleRate = static_cast<float>(newSampleRate);
    model.prepare(newSampleRate);
    experimentalModel.prepare(newSampleRate);

    // Prepare input and output stages
    inputStage.prepare(newSampleRate);
    outputStage.prepare(newSampleRate);
}

// More accurate tanh approximation
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
    model.setCutoffFrequency(cutoff);
    model.setResonance(resonance);
}

void CS01VCFCircuit::setResonance(float newResonance) {
    // IG02610 resonance range (limit max to 0.8f)
    resonance = juce::jlimit(0.1f, 0.8f, newResonance);
    model.setCutoffFrequency(cutoff);
    model.setResonance(resonance);
}

float CS01VCFCircuit::processSample(int channel, float sample) {
    return processOutputStage(model.processSample(processInputStage(sample)));
}

float CS01VCFCircuit::processExperimentalSample(float sample, float cutoffHz, float resonanceValue) {
    const float coupledInput = processInputStage(sample);
    const float filtered = experimentalModel.processSample(coupledInput, cutoffHz, resonanceValue);
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
    // Store original cutoff and resonance to restore later
    const float originalCutoff = cutoff;
    const float originalResonance = resonance;

    // Set base resonance
    setResonance(baseResonance);

    // Process each sample with its own cutoff frequency
    for (int i = 0; i < numSamples; ++i) {
        // Update cutoff for this sample
        setCutoffFrequency(cutoffModulation[i]);

        // Process the sample
        samples[i] = processSample(0, samples[i]);
    }

    // Restore original parameters
    cutoff = originalCutoff;
    resonance = originalResonance;
    model.setCutoffFrequency(cutoff);
    model.setResonance(resonance);
}
