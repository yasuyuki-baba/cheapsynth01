#pragma once

#include <JuceHeader.h>

#include <cmath>

// IG02610-inspired behavioral model. The TPT state-variable topology and
// nonlinear feedback are numerical/behavioral choices, not an IC reconstruction.
class ExperimentalOriginalVCF {
   public:
    struct EmpiricalParameters {
        // Provisional damping map; replace with measured resonance response.
        static constexpr float minimumDamping = 0.12f;
        static constexpr float maximumDamping = 1.25f;
        // Provisional normalized feedback drive and output safety limit.
        static constexpr float feedbackDrive = 1.0f;
        static constexpr float maximumOutput = 1.5f;
    };

    void prepare(double rate) { sampleRate = juce::jmax(1.0, rate); reset(); }
    void reset() { ic1eq = ic2eq = 0.0; }

    float processSample(float input, float cutoffHz, float resonance) {
        if (!std::isfinite(input)) input = 0.0f;
        const double boundedCutoff = juce::jlimit(20.0, sampleRate * 0.45,
                                                   static_cast<double>(cutoffHz));
        const double g = std::tan(juce::MathConstants<double>::pi * boundedCutoff / sampleRate);
        const double damping = juce::jmap(
            static_cast<double>(juce::jlimit(0.0f, 1.0f, resonance)),
            static_cast<double>(EmpiricalParameters::maximumDamping),
            static_cast<double>(EmpiricalParameters::minimumDamping));

        // The nonlinear feedback is an explicit behavioral hypothesis. tanh
        // bounds feedback while retaining a smooth, deterministic transfer.
        const double feedback = damping * std::tanh(ic2eq * EmpiricalParameters::feedbackDrive);
        const double v1 = (ic1eq + g * (static_cast<double>(input) - feedback - ic2eq)) /
                          (1.0 + g * (g + damping));
        const double v2 = ic2eq + g * v1;
        ic1eq = 2.0 * v1 - ic1eq;
        ic2eq = 2.0 * v2 - ic2eq;

        const double output = std::isfinite(v2) ? v2 : 0.0;
        return static_cast<float>(juce::jlimit(-static_cast<double>(EmpiricalParameters::maximumOutput),
                                                static_cast<double>(EmpiricalParameters::maximumOutput), output));
    }

   private:
    double sampleRate = 44100.0;
    double ic1eq = 0.0, ic2eq = 0.0;
};
