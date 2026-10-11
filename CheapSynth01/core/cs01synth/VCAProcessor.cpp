#include "cs01synth/VCAProcessor.h"

#include <cmath>

VCAProcessor::VCAProcessor(cs01::ParameterState& parameters) : parameters(parameters) {}

VCAProcessor::~VCAProcessor() {}

//==============================================================================
void VCAProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    egDepthControl.reset(sampleRate, 0.005);
    egDepthControl.setCurrentAndTargetValue(parameters.get(ParameterIds::vcaEgDepth));
    // Preserve the existing 44.1 kHz time constants; these are not hardware-calibrated.
    bufferCoupling.prepare(sampleRate, 0.997f);
    outputCoupling.prepare(sampleRate, 0.9995f);
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
    bufferCoupling.reset();
    tr7Buffer.reset();
    outputCoupling.reset();
}

void VCAProcessor::releaseResources() {
    bufferCoupling.reset();
    tr7Buffer.reset();
    outputCoupling.reset();
    inputHighPass.reset();
    dcBlocker.reset();
    highFreqRolloff.reset();
}

float VCAProcessor::processSample(float audio, float eg, float noteGate) {
    if (!std::isfinite(audio) || !std::isfinite(eg) || !std::isfinite(noteGate)) {
        releaseResources();
        return 0.0f;
    }
    egDepthControl.setTargetValue(parameters.get(ParameterIds::vcaEgDepth));
    audio = inputHighPass.processSample(audio);
    audio = dcBlocker.processSample(audio);
    audio = vcaModel.processSample(audio, eg, egDepthControl.getNextValue(),
                                   parameters.get(ParameterIds::breathInput),
                                   parameters.get(ParameterIds::breathVca),
                                   std::pow(parameters.get(ParameterIds::volume), 2.5f), noteGate);
    const float output = highFreqRolloff.processSample(
        outputCoupling.processSample(tr7Buffer.processSample(bufferCoupling.processSample(audio))));
    if (!std::isfinite(output)) {
        releaseResources();
        return 0.0f;
    }
    return output;
}
