#pragma once

#include <JuceHeader.h>

#include "CS01Synth/IG02610BehavioralModel.h"
#include "CS01Synth/VCFCouplingStages.h"

//==============================================================================
// CS-01 VCF signal path: IG02610-inspired behavioral core plus external coupling.
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

    // Process a block of samples with multiple channels
    void processBlock(float** channelData, int numChannels, int numSamples);

    // Process a block with per-sample cutoff modulation
    void processBlock(float* samples, int numSamples, const float* cutoffModulation,
                      float baseResonance);

   private:
    float cutoff, resonance, sampleRate;
    IG02610BehavioralModel model;

    EmpiricalVCFInputCoupling inputCoupling;
    EmpiricalVCFOutputCoupling outputCoupling;
};
