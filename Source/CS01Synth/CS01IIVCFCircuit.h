#pragma once

#include <JuceHeader.h>
#include "IG05630.h"

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
    IG05630 model;
    float maximumCutoff = 20000.0f;
};