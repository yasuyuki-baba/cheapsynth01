#pragma once

#include "dsp/Primitives.h"

#include "cs01synth/CS01VCFCircuit.h"  // Include the CS01VCFCircuit filter
#include "cs01synth/IFilter.h"
#include "Parameters.h"

//==============================================================================
class OriginalVCFProcessor : public IFilter {
   public:
    //==============================================================================
    OriginalVCFProcessor(cs01::ParameterState& parameters);
    ~OriginalVCFProcessor();

    //==============================================================================
    void prepare(double sampleRate);
    void reset();

    float processSample(float audio, float eg, float lfo);

   private:
    //==============================================================================
    CS01VCFCircuit filter;  // Using CS01VCFCircuit instead of StateVariableTPTFilter

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
