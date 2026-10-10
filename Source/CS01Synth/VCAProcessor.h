#pragma once

#include "DSP/Primitives.h"

#include "CS01Synth/IG02600.h"
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

    float processSample(float audio, float eg);

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

    // IG02600 VCA chip emulation
    IG02600 vcaModel;
    cs01::LinearRamp egDepthControl;

    // Tr7 transistor buffer emulation
    float processTr7Buffer(float input);

    // Output coupling capacitor emulation
    float processOutputCoupling(float input);

    // State variables for analog circuit emulation
    float capacitorState = 0.0f;
    float prevOutput = 0.0f;
    float outCapacitorState = 0.0f;
    float bufferCouplingPole = 0.997f;
    float outputCouplingPole = 0.9995f;
};
