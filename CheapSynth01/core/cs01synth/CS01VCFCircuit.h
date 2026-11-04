#pragma once

#include "dsp/Primitives.h"

#include "cs01synth/IG02610BehavioralModel.h"
#include "cs01synth/VCFCircuitSettings.h"

//==============================================================================
// CS-01 VCF signal path: IG02610-inspired core and external feedback network.
// External input/output coupling is unity, as in CS01IIVCFCircuit.
// Includes uncalibrated approximations; not a complete component-level reconstruction.
class CS01VCFCircuit {
   public:
    CS01VCFCircuit();                   // Default constructor (safe initial values)
    CS01VCFCircuit(double sampleRate);  // Constructor with sample rate specification
    ~CS01VCFCircuit() = default;

    void reset();
    void prepare(double sampleRate);
    void setCutoffFrequency(float newCutoff);
    void setResonance(float newResonance);

    float processSample(int channel, float sample);

    // Process a block of samples (more efficient)
    void processBlock(float* samples, int numSamples);

    // Process a block with per-sample cutoff modulation
    void processBlock(float* samples, int numSamples, const float* cutoffModulation,
                      float baseResonance);

   private:
    float cutoff, resonance;
    IG02610BehavioralModel model{cs01::OriginalVCFCircuitSettings::feedback(0.1f)};
};
