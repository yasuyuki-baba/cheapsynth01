#pragma once

#include "DSP/Primitives.h"

#include "CS01Synth/CS01VCFCircuit.h"  // Include the CS01VCFCircuit filter
#include "CS01Synth/IFilter.h"         // Updated interface
#include "Parameters.h"

#include <atomic>

//==============================================================================
class OriginalVCFProcessor : public IFilter {
   public:
    //==============================================================================
    OriginalVCFProcessor(cs01::ParameterState& parameters);
    ~OriginalVCFProcessor();

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();

    float processSample(float audio, float eg, float lfo);

    //==============================================================================
    // Implementation of IFilter interface
    ResonanceMode getResonanceMode() const override {
        return ResonanceMode::Toggle;
    }

   private:
    //==============================================================================
    cs01::ParameterState& parameters;
    CS01VCFCircuit filter;  // Using CS01VCFCircuit instead of StateVariableTPTFilter
    cs01::LinearRamp egDepthControl;

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
        // For original filter, use threshold to binarize the value
        // Treat as High Resonance if value is 0.5 or higher
        if (resonanceParam >= 0.5f) {
            // High resonance setting - CS01VCFCircuit has max resonance of 0.8f
            return 0.7f;
        } else {
            // Low resonance setting
            return 0.2f;
        }
    }
};
