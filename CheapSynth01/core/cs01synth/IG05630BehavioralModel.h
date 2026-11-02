#pragma once

#include <array>

// CS-01II / IG05630-inspired behavioral hypothesis.
// The matched-loading coefficients and resonance law are provisional;
// this is not a reconstruction of the IC's unknown Vin/VQ circuitry.
class IG05630BehavioralModel {
   public:
    struct EmpiricalParameters {
        // Conditional external-summer model with equal input-node conductance.
        static constexpr double oddStateFeedback = 82.0 / 22.0;
        static constexpr double evenStateFeedback = 82.0 / 15.0;
        // Provisional normalized saturation and resonance ranges. Not calibrated.
        static constexpr float inputDrive = 2.0f;
        static constexpr float negativeKneeThreshold = 0.2f;
        static constexpr float negativeClipAmount = 0.35f;
        // Linear feedback remains below b - 2 = 3.4667 in this assumed model.
        static constexpr double maximumFeedbackGain = 3.2;
        static constexpr float maximumOutput = 1.5f;
    };

    void prepare(double sampleRate);
    void reset();
    void setCutoffFrequency(float frequency);
    void setResonance(float resonance);
    float processSample(float sample);

   private:
    double sampleRate = 44100.0;
    float cutoff = 1000.0f;
    double resonanceFeedbackGain = 0.0;
    std::array<double, 4> integrators{};
};
