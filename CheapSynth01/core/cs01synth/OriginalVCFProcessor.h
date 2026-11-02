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
        if (resonanceParam >= 0.5f) {
            return IG02610BehavioralModel::EmpiricalParameters::highResonance;
        } else {
            return IG02610BehavioralModel::EmpiricalParameters::lowResonance;
        }
    }
};
