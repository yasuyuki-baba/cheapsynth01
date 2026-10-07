#pragma once

// CS-01II / IG05630-inspired behavioral hypothesis.
// Four poles, continuous resonant feedback, and bounded nonlinear stages are
// model choices based on the supplied report, not an internal IC reconstruction.
class ExperimentalIG05630 {
   public:
    struct EmpiricalParameters {
        // Butterworth-aligned section Q values; used as the zero-resonance target.
        static constexpr float firstSectionQ = 0.5411961f;
        static constexpr float secondSectionQ = 1.3065630f;
        // Provisional normalized saturation and resonance ranges. Not calibrated.
        static constexpr float inputDrive = 2.0f;
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
