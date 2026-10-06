#include "CS01Synth/ExperimentalIG05630.h"

#include <algorithm>
#include <cmath>

float ExperimentalIG05630::StateVariableLowpass::processSample(float input, float g,
                                                                float damping) {
    // TPT SVF with coupled trapezoidal integrators.
    const float normalization = 1.0f / (1.0f + damping * g + g * g);
    const float highpass = normalization * (input - integrator1 * (g + damping) - integrator2);
    const float bandpass = highpass * g + integrator1;
    integrator1 = highpass * g + bandpass;
    const float lowpass = bandpass * g + integrator2;
    integrator2 = bandpass * g + lowpass;
    return lowpass;
}

void ExperimentalIG05630::StateVariableLowpass::reset() {
    integrator1 = 0.0f;
    integrator2 = 0.0f;
}

void ExperimentalIG05630::prepare(double newSampleRate) {
    sampleRate = std::max(1.0, newSampleRate);
    reset();
}

void ExperimentalIG05630::reset() {
    feedbackOutput = 0.0f;
    firstSection.reset();
    secondSection.reset();
}

void ExperimentalIG05630::setCutoffFrequency(float frequency) {
    cutoff = std::isfinite(frequency) ? frequency : 1000.0f;
    cutoff = std::clamp(cutoff, 20.0f, static_cast<float>(sampleRate * 0.45));
}

void ExperimentalIG05630::setResonance(float amount) {
    resonance = std::isfinite(amount) ? std::clamp(amount, 0.0f, 1.0f) : 0.0f;
}

float ExperimentalIG05630::processSample(float sample) {
    if (!std::isfinite(sample))
        sample = 0.0f;

    constexpr float pi = 3.14159265358979323846f;
    const float g = std::tan(pi * cutoff / static_cast<float>(sampleRate));
    const float firstDamping = 1.0f / EmpiricalParameters::firstSectionQ;
    const float secondDamping = 1.0f / EmpiricalParameters::secondSectionQ;

    // The one-sample feedback state keeps this behavioral loop deterministic.
    // This is an explicit-delay TPT cascade, not a ZDF solution of an IC loop.
    const float feedbackGain = EmpiricalParameters::maximumFeedbackGain * resonance;
    const float feedback = feedbackGain *
                           std::tanh(feedbackOutput * EmpiricalParameters::feedbackDrive);
    const float integratorInput =
        std::tanh((sample - feedback) * EmpiricalParameters::inputDrive) /
        EmpiricalParameters::inputDrive;

    const float firstOutput = firstSection.processSample(integratorInput, g, firstDamping);
    const float output = secondSection.processSample(firstOutput, g, secondDamping);
    feedbackOutput = output;

    if (!std::isfinite(output)) {
        reset();
        return 0.0f;
    }
    return std::clamp(output, -EmpiricalParameters::maximumOutput,
                      EmpiricalParameters::maximumOutput);
}
