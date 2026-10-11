#pragma once

#include "dsp/Primitives.h"

#include "cs01synth/IG02600BehavioralModel.h"
#include "cs01synth/VCAEmpiricalStages.h"
#include "Parameters.h"

//==============================================================================
class VCAProcessor {
   public:
    //==============================================================================
    VCAProcessor(cs01::ParameterState& parameters);
    ~VCAProcessor();

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();

    float processSample(float audio, float eg, float noteGate = 1.0f);

    //==============================================================================
   private:
    //==============================================================================
    cs01::ParameterState& parameters;

    // Input stage high-pass filter (82K resistor and 1/50 capacitor)
    cs01::Biquad inputHighPass;

    // Simple DC blocking filter
    cs01::Biquad dcBlocker;

    // Simple high frequency rolloff filter
    cs01::Biquad highFreqRolloff;

    // IG02600BehavioralModel VCA chip emulation
    IG02600BehavioralModel vcaModel;
    cs01::LinearRamp egDepthControl;

    EmpiricalVCACoupling bufferCoupling{0.997f};
    Tr7EmpiricalBuffer tr7Buffer;
    EmpiricalVCACoupling outputCoupling{0.9995f};
};
