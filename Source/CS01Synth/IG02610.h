#pragma once
#include <JuceHeader.h>

// Provisional IC-only model. Control units and nonlinearities are empirical,
// not an established reconstruction of the IG02610 internal circuit.
class IG02610 {
   public:
    void reset();
    void prepare(double newSampleRate);
    void setCutoffFrequency(float value);
    void setResonance(float value);
    float processSample(float sample);

   private:
    float cutoff = 1000.0f, resonance = 0.1f, sampleRate = 0.0f;
    double a1 = 0, a2 = 0, b0 = 0, b1 = 0, b2 = 0;
    double z1 = 0, z2 = 0;
    float inputLevelSmoothed = 0;
    static constexpr float LEVEL_SMOOTHING = 0.99f;
    static constexpr float INPUT_LEVEL_INFLUENCE = 0.02f;
    float levelSmoothing = LEVEL_SMOOTHING;
    float accurateTanh(float x);
    void updateCoefficients();
};
