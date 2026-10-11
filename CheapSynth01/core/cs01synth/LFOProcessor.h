#pragma once

#include "dsp/Primitives.h"

#include "Parameters.h"

//==============================================================================
class LFOProcessor {
   public:
    //==============================================================================
    LFOProcessor(cs01::ParameterState& parameters);
    ~LFOProcessor();

    //==============================================================================
    void prepare(double sampleRate);
    void reset();

    float processSample();

    //==============================================================================
   private:
    //==============================================================================
    void updateParameters();

    cs01::ParameterState& parameters;
    cs01::Oscillator lfo;
};
