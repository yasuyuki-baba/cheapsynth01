#pragma once

#include "dsp/Primitives.h"

#include <cmath>

// Provisional IG02610-inspired model; topology, Q and input limiting are
// behavioral choices, not a reconstruction of the IC.
class IG02610BehavioralModel {
   public:
    struct EmpiricalParameters {
        static constexpr float lowResonance = 0.2f;
        static constexpr float highResonance = 0.7f;
        static constexpr double lowQ = 0.5;
        static constexpr double highQ = 1.6;
        // Provisional normalized drives; neither is calibrated to an IC.
        // Negative-knee soft compression models pre-VCF asymmetric clipping
        // (preserving linear small-signal response for |u| <= 0.2 while generating
        // even harmonics on driven VCO waveforms).
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
            std::clamp(std::isfinite(cutoffHz) ? static_cast<double>(cutoffHz) : 1000.0, 20.0,
                       sampleRate * 0.45);
        const double g = std::tan(std::numbers::pi_v<double> * boundedCutoff / sampleRate);
        const double position =
            std::clamp(((std::isfinite(resonance) ? static_cast<double>(resonance)
                                                  : EmpiricalParameters::lowResonance) -
                        EmpiricalParameters::lowResonance) /
                           (static_cast<double>(EmpiricalParameters::highResonance) -
                            EmpiricalParameters::lowResonance),
                       0.0, 1.0);
        const double damping =
            1.0 / EmpiricalParameters::lowQ +
            position * (1.0 / EmpiricalParameters::highQ - 1.0 / EmpiricalParameters::lowQ);

        const double excessNegative =
            std::min(0.0, static_cast<double>(input) +
                              static_cast<double>(EmpiricalParameters::negativeKneeThreshold));
        const double asymmetricDriven =
            input - excessNegative * static_cast<double>(EmpiricalParameters::negativeClipAmount);
        const double integratorInput =
            std::tanh(asymmetricDriven * EmpiricalParameters::integratorInputDrive) /
            EmpiricalParameters::integratorInputDrive;
        const double v1 = (ic1eq + g * (integratorInput - ic2eq)) / (1.0 + g * (g + damping));
        const double v2 = ic2eq + g * v1;
        ic1eq = 2.0 * v1 - ic1eq;
        ic2eq = 2.0 * v2 - ic2eq;

        if (!std::isfinite(v2)) {
            reset();
            return 0.0f;
        }
        return static_cast<float>(
            std::clamp(v2, -static_cast<double>(EmpiricalParameters::maximumOutput),
                       static_cast<double>(EmpiricalParameters::maximumOutput)));
    }

   private:
    double sampleRate = 44100.0;
    double ic1eq = 0.0, ic2eq = 0.0;
};
