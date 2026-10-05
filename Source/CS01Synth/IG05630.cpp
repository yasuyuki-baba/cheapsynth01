#include "IG05630.h"
#include <algorithm>
#include <cmath>

void IG05630::LowpassStage::update(float g) {
    normalization = static_cast<float>(1.0 / (1.0 + damping * g + g * g));
}

float IG05630::LowpassStage::processSample(float input, float g) {
    // Trapezoidal integrators, solved together to avoid a feedback delay.
    const float highpass = normalization * (input - integrator1 * (g + damping) - integrator2);
    const float bandpass = highpass * g + integrator1;
    integrator1 = highpass * g + bandpass;
    const float lowpass = bandpass * g + integrator2;
    integrator2 = bandpass * g + lowpass;
    return lowpass;
}

void IG05630::LowpassStage::reset() {
    integrator1 = 0.0f;
    integrator2 = 0.0f;
}

void IG05630::prepare(double sampleRate) {
    this->sampleRate = sampleRate;
    levelSmoothing = static_cast<float>(std::pow(0.99, 44100.0 / sampleRate));
    reset();
    updateCoefficients();
}

void IG05630::reset() {
    inputLevelSmoothed = 0.0f;
    firstStage.reset();
    secondStage.reset();
}

void IG05630::setCutoffFrequency(float frequency) {
    cutoff = frequency;
    updateCoefficients();
}

void IG05630::setResonance(float resonance) {
    // Butterworth alignment at zero; empirical resonance, not global feedback.
    resonanceAmount = std::clamp(resonance, 0.0f, 1.0f);
    const float q = 0.5411961f * (1.0f + 3.0f * resonanceAmount);
    firstStage.damping = static_cast<float>(1.0 / q);
    secondStage.damping = static_cast<float>(1.0 / 1.3065630f);
    firstStage.update(g);
    secondStage.update(g);
}

float IG05630::processSample(float sample) {
    // Empirical coloration inspired by Original, not measured IC behavior.
    // Keep the linear poles unchanged and avoid duplicating distortion per stage.
    const float level = std::min(std::abs(sample), 1.0f);
    inputLevelSmoothed = inputLevelSmoothed * levelSmoothing + level * (1.0f - levelSmoothing);
    const float output = secondStage.processSample(firstStage.processSample(sample, g), g);
    const float drive = 0.5f + 0.5f * resonanceAmount + 0.25f * inputLevelSmoothed;
    const float blend = 0.08f + 0.22f * resonanceAmount;
    const float saturated = std::tanh(output * drive) / drive;
    return output + blend * (saturated - output);
}

void IG05630::updateCoefficients() {
    constexpr double pi = 3.1415926535897932384626433832795;
    g = static_cast<float>(std::tan(pi * cutoff / sampleRate));
    firstStage.update(g);
    secondStage.update(g);
}