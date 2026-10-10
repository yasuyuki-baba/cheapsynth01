#include "CS01Synth/VCAProcessor.h"

#include <cmath>

VCAProcessor::VCAProcessor(cs01::ParameterState& parameters) : parameters(parameters) {}

VCAProcessor::~VCAProcessor() {}

//==============================================================================
void VCAProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    egDepthControl.reset(sampleRate, 0.005);
    egDepthControl.setCurrentAndTargetValue(parameters.get(ParameterIds::vcaEgDepth));
    // Preserve the existing 44.1 kHz time constants; these are not hardware-calibrated.
    bufferCouplingPole =
        static_cast<float>(std::pow(static_cast<double>(0.997f), 44100.0 / sampleRate));
    outputCouplingPole =
        static_cast<float>(std::pow(static_cast<double>(0.9995f), 44100.0 / sampleRate));
    // Empirical second-order 40 Hz high-pass; not derived from the schematic's
    // 1 uF / 50 V coupling capacitor and 82 kohm series resistor.
    inputHighPass.highPass(sampleRate, 40.0);
    inputHighPass.reset();

    // Initialize DC blocker
    dcBlocker.highPass(sampleRate, 20.0);
    dcBlocker.reset();

    // Initialize high frequency rolloff filter
    float cutoffFreq = std::min(15000.0f, static_cast<float>(sampleRate * 0.45f));
    highFreqRolloff.lowPass(sampleRate, cutoffFreq);
    highFreqRolloff.reset();

    // Reset state variables
    capacitorState = 0.0f;
    prevOutput = 0.0f;
    outCapacitorState = 0.0f;
}

void VCAProcessor::releaseResources() {
    capacitorState = prevOutput = outCapacitorState = 0.0f;
    inputHighPass.reset();
    dcBlocker.reset();
    highFreqRolloff.reset();
}

float VCAProcessor::processSample(float audio, float eg) {
    egDepthControl.setTargetValue(parameters.get(ParameterIds::vcaEgDepth));
    audio = inputHighPass.processSample(audio);
    audio = dcBlocker.processSample(audio);
    audio = vcaModel.processSample(audio, eg, egDepthControl.getNextValue(),
                                   parameters.get(ParameterIds::breathInput),
                                   parameters.get(ParameterIds::breathVca),
                                   std::pow(parameters.get(ParameterIds::volume), 2.5f));
    return highFreqRolloff.processSample(processOutputCoupling(processTr7Buffer(audio)));
}

// Tr7 transistor buffer emulation
float VCAProcessor::processTr7Buffer(float input) {
    // Output coupling capacitor (1/50) - high-pass characteristic
    const float rc1 = bufferCouplingPole;
    capacitorState = capacitorState * rc1 + input * (1.0f - rc1);
    float hpOutput = input - capacitorState;

    // Tr7 transistor non-linear characteristic
    float transistorOutput;
    if (hpOutput > 0) {
        // NPN transistor is more linear for positive signals
        transistorOutput = hpOutput * 0.95f;
    } else {
        // Slight asymmetry for negative signals
        transistorOutput = hpOutput * 0.92f;
    }

    // Transistor high-frequency response (slight treble boost)
    const float rc2 = 0.998f;
    float highFreqComponent = (transistorOutput - prevOutput) * (1.0f - rc2) * 2.0f;
    prevOutput = transistorOutput;

    return transistorOutput + highFreqComponent;
}

// Output coupling capacitor emulation (4.7/25)
float VCAProcessor::processOutputCoupling(float input) {
    // Empirical output coupling time constant, preserved from the 44.1 kHz model.
    const float rc3 = outputCouplingPole;
    outCapacitorState = outCapacitorState * rc3 + input * (1.0f - rc3);

    return input - outCapacitorState;
}
