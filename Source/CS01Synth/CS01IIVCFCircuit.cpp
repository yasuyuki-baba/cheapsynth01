#include "CS01Synth/CS01IIVCFCircuit.h"

#include "DSP/Primitives.h"

#include <cmath>

void CS01IIVCFCircuit::prepare(double sampleRate) {
    maximumCutoff = std::min(20000.0f, static_cast<float>(sampleRate) * 0.49f);
    model.prepare(sampleRate);
}

void CS01IIVCFCircuit::reset() {
    model.reset();
}

void CS01IIVCFCircuit::setCutoffFrequency(float frequency) {
    if (!std::isfinite(frequency))
        frequency = 1000.0f;
    model.setCutoffFrequency(std::clamp(frequency, 20.0f, maximumCutoff));
}

void CS01IIVCFCircuit::setResonance(float resonance) {
    model.setResonance(resonance);
}

float CS01IIVCFCircuit::processSample(int channel, float sample) {
    (void)channel;  // The circuit is monophonic.
    return model.processSample(sample);
}
