#include "CS01Synth/CS01IIVCFCircuit.h"

#include <JuceHeader.h>

#include <cmath>

void CS01IIVCFCircuit::prepare(double sampleRate) {
    maximumCutoff = juce::jmin(20000.0f, static_cast<float>(sampleRate) * 0.49f);
    model.prepare(sampleRate);
    experimentalModel.prepare(sampleRate);
}

void CS01IIVCFCircuit::reset() {
    model.reset();
    experimentalModel.reset();
}

void CS01IIVCFCircuit::setCutoffFrequency(float frequency) {
    if (!std::isfinite(frequency))
        frequency = 1000.0f;
    model.setCutoffFrequency(juce::jlimit(20.0f, maximumCutoff, frequency));
    experimentalModel.setCutoffFrequency(juce::jlimit(20.0f, maximumCutoff, frequency));
}

void CS01IIVCFCircuit::setResonance(float resonance) {
    model.setResonance(resonance);
    experimentalModel.setResonance(resonance);
}

float CS01IIVCFCircuit::processSample(int channel, float sample) {
    jassert(channel == 0);
    if (selectedModel.load(std::memory_order_relaxed) == Model::Experimental)
        return experimentalModel.processSample(sample);
    return model.processSample(sample);
}
