#pragma once

#include "CS01Synth/IG05630BehavioralModel.h"

// Circuit boundary for Modern, analogous to CS01VCFCircuit in Original.
// External coupling is currently unity: no uncalibrated stages are added.
class CS01IIVCFCircuit {
   public:
    void prepare(double sampleRate);
    void reset();
    void setCutoffFrequency(float frequency);
    void setResonance(float resonance);
    float processSample(int channel, float sample);

   private:
    IG05630BehavioralModel model;
    float maximumCutoff = 20000.0f;
};
