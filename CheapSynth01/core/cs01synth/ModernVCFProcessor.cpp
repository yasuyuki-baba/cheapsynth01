#include "cs01synth/ModernVCFProcessor.h"

#include <algorithm>

//==============================================================================
ModernVCFProcessor::ModernVCFProcessor(cs01::ParameterState& parameters) : IFilter(parameters) {}

ModernVCFProcessor::~ModernVCFProcessor() {}

//==============================================================================
void ModernVCFProcessor::prepare(double sampleRate) {
    filter.reset();
    filter.prepare(sampleRate);
    processingSampleRate = sampleRate;
    prepareCutoffModulation(sampleRate);
}

void ModernVCFProcessor::reset() {
    // Reset filter
    filter.reset();
}

float ModernVCFProcessor::processSample(float audio, float eg, float lfo) {
    filter.setCutoffFrequency(nextModulatedCutoff(
        eg, lfo, std::min(20000.0f, static_cast<float>(processingSampleRate * .49))));
    filter.setResonance(calculateResonance(parameters.get(ParameterIds::resonance)));
    return filter.processSample(0, audio);
}
