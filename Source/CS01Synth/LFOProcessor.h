#pragma once

#include "DSP/Primitives.h"

#include "Parameters.h"

//==============================================================================
class LFOProcessor {
   public:
    //==============================================================================
    LFOProcessor(cs01::ParameterState& parameters);
    ~LFOProcessor();

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();

    float processSample();

    //==============================================================================
   private:
    //==============================================================================
    void updateParameters();

    cs01::ParameterState& parameters;
    cs01::Oscillator lfo;
};
