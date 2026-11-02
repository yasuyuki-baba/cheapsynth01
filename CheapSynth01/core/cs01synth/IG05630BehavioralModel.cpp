#include "cs01synth/IG05630BehavioralModel.h"

#include <algorithm>
#include <cmath>

void IG05630BehavioralModel::prepare(double newSampleRate) {
    sampleRate = std::max(1.0, newSampleRate);
    reset();
}

void IG05630BehavioralModel::reset() {
    integrators.fill(0.0);
}

void IG05630BehavioralModel::setCutoffFrequency(float frequency) {
    cutoff = std::isfinite(frequency) ? frequency : 1000.0f;
    cutoff = std::clamp(cutoff, 20.0f, static_cast<float>(sampleRate * 0.45));
}

void IG05630BehavioralModel::setResonance(float amount) {
    const float resonance = std::isfinite(amount) ? std::clamp(amount, 0.0f, 1.0f) : 0.0f;
    resonanceFeedbackGain =
        EmpiricalParameters::maximumFeedbackGain * static_cast<double>(resonance);
}

float IG05630BehavioralModel::processSample(float sample) {
    if (!std::isfinite(sample))
        sample = 0.0f;

    constexpr double pi = 3.14159265358979323846;
    const double g = std::tan(pi * cutoff / sampleRate);
    constexpr double a = EmpiricalParameters::oddStateFeedback;
    constexpr double b = EmpiricalParameters::evenStateFeedback;
    const double c = 1.0 + resonanceFeedbackGain;
    const double excessNegative =
        std::min(0.0, static_cast<double>(sample) + EmpiricalParameters::negativeKneeThreshold);
    const double driven = sample - excessNegative * EmpiricalParameters::negativeClipAmount;
    const double input =
        std::tanh(driven * EmpiricalParameters::inputDrive) / EmpiricalParameters::inputDrive;

    // Solve all four trapezoidal integrators and their weighted feedback together.
    const double t1 = integrators[0];
    const double t2 = g * t1 + integrators[1];
    const double t3 = g * t2 + integrators[2];
    const double t4 = g * t3 + integrators[3];
    double state =
        (input - a * t1 - b * t2 - a * t3 - c * t4) / (1.0 + g * (a + g * (b + g * (a + g * c))));
    for (double& memory : integrators) {
        const double output = g * state + memory;
        memory = 2.0 * output - memory;
        state = output;
    }

    if (!std::isfinite(state)) {
        reset();
        return 0.0f;
    }
    return static_cast<float>(std::clamp(state,
                                         -static_cast<double>(EmpiricalParameters::maximumOutput),
                                         static_cast<double>(EmpiricalParameters::maximumOutput)));
}
