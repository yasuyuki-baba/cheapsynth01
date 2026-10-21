#pragma once

#include "dsp/Primitives.h"

#include "cs01synth/CS01IIVCFCircuit.h"
#include "cs01synth/IFilter.h"  // Interface
#include "Parameters.h"

//==============================================================================
// ModernVCFProcessor - CS01II-inspired four-pole lowpass approximation.
class ModernVCFProcessor : public IFilter {
   public:
    //==============================================================================
    ModernVCFProcessor(cs01::ParameterState& parameters);
    ~ModernVCFProcessor();

    //==============================================================================
    void prepare(double sampleRate);
    void reset();

    float processSample(float audio, float eg, float lfo);

    //==============================================================================
    // Implementation of IFilter interface
    ResonanceMode getResonanceMode() const override {
        return ResonanceMode::Continuous;
    }

   private:
    //==============================================================================
    cs01::ParameterState& parameters;
    CS01IIVCFCircuit filter;
    cs01::LinearRamp egDepthControl;
    double processingSampleRate = 44100.0;

    // Cutoff frequency calculation function
    float calculateCutoffFrequency(float cutoffParam) {
        // Range covering the entire audible spectrum
        const float minFreq = 20.0f;     // Minimum cutoff frequency
        const float maxFreq = 20000.0f;  // Maximum cutoff frequency

        // Verify input range (actual frequency value)
        float cutoffFreq = std::clamp(cutoffParam, minFreq, maxFreq);

        return cutoffFreq;
    }

    // Resonance calculation function
    float calculateResonance(float resonanceParam) {
        return std::clamp(resonanceParam, 0.0f, 1.0f);
    }
};
