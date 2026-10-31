#pragma once

#include "dsp/Primitives.h"

#include <cmath>

// IG02610-inspired behavioral model. The TPT state-variable topology and
// nonlinear feedback are numerical/behavioral choices, not an IC reconstruction.
class IG02610BehavioralModel {
   public:
    struct EmpiricalParameters {
        // Provisional damping map; replace with measured resonance response.
        static constexpr float minimumDamping = 0.12f;
        static constexpr float maximumDamping = 1.25f;
        // Provisional normalized drives; neither is calibrated to an IC.
        // Negative-knee soft compression models pre-VCF asymmetric clipping
        // (preserving linear small-signal response for |u| <= 0.2 while generating
        // even harmonics on driven VCO waveforms).
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
            std::clamp(static_cast<double>(cutoffHz), 20.0, sampleRate * 0.45);
        const double g = std::tan(std::numbers::pi_v<double> * boundedCutoff / sampleRate);
        const double damping = EmpiricalParameters::maximumDamping +
                               static_cast<double>(std::clamp(resonance, 0.f, 1.f)) *
                                   (static_cast<double>(EmpiricalParameters::minimumDamping) -
                                    static_cast<double>(EmpiricalParameters::maximumDamping));

        // Behavioral hypothesis based on a two-integrator SVF with asymmetric soft
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
            std::clamp(output, -static_cast<double>(EmpiricalParameters::maximumOutput),
                       static_cast<double>(EmpiricalParameters::maximumOutput)));
    }

   private:
    double sampleRate = 44100.0;
    double ic1eq = 0.0, ic2eq = 0.0;
};
