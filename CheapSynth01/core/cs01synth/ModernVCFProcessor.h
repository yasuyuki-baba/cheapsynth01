#pragma once

#include "dsp/Primitives.h"

#include "cs01synth/CS01IIVCFCircuit.h"
#include "cs01synth/IFilter.h"
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

   private:
    //==============================================================================
    CS01IIVCFCircuit filter;
    double processingSampleRate = 44100.0;

    // Resonance calculation function
    float calculateResonance(float resonanceParam) {
        return std::clamp(resonanceParam, 0.0f, 1.0f);
    }
};
