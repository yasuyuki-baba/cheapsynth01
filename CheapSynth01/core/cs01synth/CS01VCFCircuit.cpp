#include "cs01synth/CS01VCFCircuit.h"

CS01VCFCircuit::CS01VCFCircuit() : cutoff(1000.0f), resonance(0.1f) {
    reset();
}

CS01VCFCircuit::CS01VCFCircuit(double sampleRate) : CS01VCFCircuit() {
    prepare(sampleRate);
}

void CS01VCFCircuit::reset() {
    model.reset();
}

void CS01VCFCircuit::prepare(double sampleRate) {
    model.prepare(sampleRate);
}

void CS01VCFCircuit::setCutoffFrequency(float newCutoff) {
    cutoff = std::clamp(newCutoff, 20.0f, 20000.0f);
}

void CS01VCFCircuit::setResonance(float newResonance) {
    resonance = std::clamp(newResonance, 0.0f, 1.0f);
}

float CS01VCFCircuit::processSample(int channel, float sample) {
    if (!std::isfinite(sample)) {
        reset();
        return 0.0f;
    }
    model.setCutoffFrequency(cutoff);
    model.setFeedback(cs01::OriginalVCFCircuitSettings::feedback(resonance));
    const float output = model.processSample(sample);
    return output;
}

void CS01VCFCircuit::processBlock(float* samples, int numSamples) {
    // Process a block of mono samples
    for (int i = 0; i < numSamples; ++i) {
        samples[i] = processSample(0, samples[i]);
    }
}

void CS01VCFCircuit::processBlock(float* samples, int numSamples, const float* cutoffModulation,
                                  float baseResonance) {
    const float originalCutoff = cutoff;
    const float originalResonance = resonance;
    const float boundedResonance = std::clamp(baseResonance, 0.0f, 1.0f);
    for (int i = 0; i < numSamples; ++i) {
        const float boundedCutoff = std::clamp(cutoffModulation[i], 20.0f, 20000.0f);
        cutoff = std::isfinite(boundedCutoff) ? boundedCutoff : originalCutoff;
        resonance = std::isfinite(boundedResonance) ? boundedResonance : originalResonance;
        samples[i] = processSample(0, samples[i]);
    }
    cutoff = originalCutoff;
    resonance = originalResonance;
}
