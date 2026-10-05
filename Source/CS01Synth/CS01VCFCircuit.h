#pragma once

#include <JuceHeader.h>

#include "CS01Synth/IG02610.h"

//==============================================================================
// CS-01 VCF signal path: provisional IG02610 model plus external coupling.
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

    // Process a single sample (legacy method)
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
    IG02610 model;

    // Input stage model
    struct InputStage {
        float prevSample = 0.0f;
        juce::dsp::IIR::Filter<float> dcBlocker;

        void prepare(double sampleRate) {
            dcBlocker.reset();
            dcBlocker.coefficients =
                juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 20.0f);
        }

        void reset() {
            prevSample = 0.0f;
            dcBlocker.reset();
        }
    };

    // Output stage model
    struct OutputStage {
        float prevInput = 0.0f;
        float prevOutput = 0.0f;
        double sampleRate = 44100.0;

        void prepare(double newSampleRate) {
            sampleRate = newSampleRate;
        }

        void reset() {
            prevInput = 0.0f;
            prevOutput = 0.0f;
        }
    };

    InputStage inputStage;
    OutputStage outputStage;

    // Input stage processing
    float processInputStage(float sample);

    // Output stage processing
    float processOutputStage(float sample);
};
