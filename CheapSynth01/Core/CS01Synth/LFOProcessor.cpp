#include "CS01Synth/LFOProcessor.h"

//==============================================================================
LFOProcessor::LFOProcessor(cs01::ParameterState& parameters) : parameters(parameters) {
    lfo.initialise([](double phase) { return 1.0 - 2.0 * std::abs(phase) / std::numbers::pi; });
}

LFOProcessor::~LFOProcessor() {}

//==============================================================================
void LFOProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    lfo.prepare(sampleRate);
    lfo.setFrequency(parameters.get(ParameterIds::lfoSpeed), true);
}

void LFOProcessor::releaseResources() {}

float LFOProcessor::processSample() {
    updateParameters();
    return static_cast<float>(lfo.processSample());
}

void LFOProcessor::updateParameters() {
    auto lfoSpeed = parameters.get(ParameterIds::lfoSpeed);
    lfo.setFrequency(lfoSpeed);
}
