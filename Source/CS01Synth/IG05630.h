#pragma once


// Provisional IC-only model for the CS01II's IG05630.
// Four-pole TPT approximation; not a reconstruction of the IC's internal circuit.
// Replace/calibrate this class when IC documentation or measurements are available.
class IG05630 {
   public:
    void prepare(double sampleRate);
    void reset();
    void setCutoffFrequency(float frequency);
    void setResonance(float resonance);
    float processSample(float sample);

   private:
    struct LowpassStage {
        float integrator1 = 0.0f;
        float integrator2 = 0.0f;
        float damping = 1.41421354f;
        float normalization = 1.0f;

        void update(float g);
        float processSample(float input, float g);
        void reset();
    };

    double sampleRate = 44100.0;
    float cutoff = 1000.0f;
    float g = 0.0f;
    float resonanceAmount = 0.0f;
    float inputLevelSmoothed = 0.0f;
    float levelSmoothing = 0.99f;
    LowpassStage firstStage;
    LowpassStage secondStage;
    void updateCoefficients();
};