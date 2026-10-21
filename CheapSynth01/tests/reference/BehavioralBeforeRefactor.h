#pragma once

#include <algorithm>
#include <numbers>

#include <cmath>

template <class T>
T frozenClamp(T low, T high, T value) {
    return std::clamp(value, low, high);
}
template <class T>
T frozenMap(T value, T low, T high) {
    return low + value * (high - low);
}

// Captured from repository commit 1cbbdd7 (before this refactor).
// Frozen production DSP before the naming/stage refactor. Test oracle only.
// Keep arithmetic unchanged; do not update this alongside production DSP.

// IG02610-inspired behavioral model. The TPT state-variable topology and
// nonlinear feedback are numerical/behavioral choices, not an IC reconstruction.
class BeforeIG02610 {
   public:
    struct EmpiricalParameters {
        // Provisional damping map; replace with measured resonance response.
        static constexpr float minimumDamping = 0.12f;
        static constexpr float maximumDamping = 1.25f;
        // Provisional normalized drives; neither is calibrated to an IC.
        static constexpr float feedbackDrive = 1.0f;
        static constexpr float integratorInputDrive = 2.0f;
        static constexpr float negativeKneeThreshold = 0.2f;
        static constexpr float negativeClipAmount = 0.35f;
        static constexpr float maximumOutput = 1.5f;
    };

    void prepare(double rate) {
        sampleRate = std::max(1.0, rate);
        reset();
    }
    void reset() {
        ic1eq = ic2eq = 0.0;
    }

    float processSample(float input, float cutoffHz, float resonance) {
        if (!std::isfinite(input))
            input = 0.0f;
        const double boundedCutoff =
            frozenClamp(20.0, sampleRate * 0.45, static_cast<double>(cutoffHz));
        const double g = std::tan(std::numbers::pi * boundedCutoff / sampleRate);
        const double damping = frozenMap(static_cast<double>(frozenClamp(0.0f, 1.0f, resonance)),
                                         static_cast<double>(EmpiricalParameters::maximumDamping),
                                         static_cast<double>(EmpiricalParameters::minimumDamping));

        // Behavioral hypothesis based on a two-integrator SVF with soft
        // limiting around the summing/integrator input and resonant feedback.
        // These placements do not claim to reproduce the IC's internal circuit.
        const double feedback = damping * std::tanh(ic2eq * EmpiricalParameters::feedbackDrive);
        const double driven = static_cast<double>(input) - feedback;
        const double excessNegative =
            std::min(0.0, driven + static_cast<double>(EmpiricalParameters::negativeKneeThreshold));
        const double asymmetricDriven =
            driven - excessNegative * static_cast<double>(EmpiricalParameters::negativeClipAmount);
        const double integratorInput =
            std::tanh(asymmetricDriven * EmpiricalParameters::integratorInputDrive) /
            EmpiricalParameters::integratorInputDrive;
        const double v1 = (ic1eq + g * (integratorInput - ic2eq)) / (1.0 + g * (g + damping));
        const double v2 = ic2eq + g * v1;
        ic1eq = 2.0 * v1 - ic1eq;
        ic2eq = 2.0 * v2 - ic2eq;

        const double output = std::isfinite(v2) ? v2 : 0.0;
        return static_cast<float>(
            frozenClamp(-static_cast<double>(EmpiricalParameters::maximumOutput),
                        static_cast<double>(EmpiricalParameters::maximumOutput), output));
    }

   private:
    double sampleRate = 44100.0;
    double ic1eq = 0.0, ic2eq = 0.0;
};

// CS-01II / IG05630-inspired behavioral hypothesis.
// Four poles, continuous resonant feedback, and bounded nonlinear stages are
// model choices based on the supplied report, not an internal IC reconstruction.
class BeforeIG05630 {
   public:
    struct EmpiricalParameters {
        // Butterworth-aligned section Q values; used as the zero-resonance target.
        static constexpr float firstSectionQ = 0.5411961f;
        static constexpr float secondSectionQ = 1.3065630f;
        // Provisional normalized saturation and resonance ranges. Not calibrated.
        static constexpr float inputDrive = 2.0f;
        static constexpr float negativeKneeThreshold = 0.2f;
        static constexpr float negativeClipAmount = 0.35f;
        static constexpr float feedbackDrive = 1.0f;
        // Provisional feedback map kept below the model's self-oscillation
        // threshold. Non-oscillating behavior is a conservative hypothesis,
        // not a verified property of the hardware.
        static constexpr float maximumFeedbackGain = 1.2f;
        static constexpr float resonanceCurve = 4.0f;
        static constexpr float maximumOutput = 1.5f;
    };

    void prepare(double sampleRate);
    void reset();
    void setCutoffFrequency(float frequency);
    void setResonance(float resonance);
    float processSample(float sample);

   private:
    struct StateVariableLowpass {
        float integrator1 = 0.0f;
        float integrator2 = 0.0f;

        float processSample(float input, float g, float damping);
        void reset();
    };

    double sampleRate = 44100.0;
    float cutoff = 1000.0f;
    float resonance = 0.0f;
    float resonanceFeedbackGain = 0.0f;
    float feedbackOutput = 0.0f;
    StateVariableLowpass firstSection;
    StateVariableLowpass secondSection;
};
