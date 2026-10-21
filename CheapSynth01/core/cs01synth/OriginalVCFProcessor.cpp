#include "cs01synth/OriginalVCFProcessor.h"

//==============================================================================
OriginalVCFProcessor::OriginalVCFProcessor(cs01::ParameterState& parameters)
    : IFilter(parameters) {}

OriginalVCFProcessor::~OriginalVCFProcessor() {}

//==============================================================================
void OriginalVCFProcessor::prepare(double sampleRate) {
    filter.reset();
    filter.prepare(sampleRate);
    prepareCutoffModulation(sampleRate);
}

void OriginalVCFProcessor::reset() {
    // Reset filter
    filter.reset();
}

float OriginalVCFProcessor::processSample(float audio, float eg, float lfo) {
    filter.setCutoffFrequency(nextModulatedCutoff(eg, lfo, 20000.0f));
    filter.setResonance(calculateResonance(parameters.get(ParameterIds::resonance)));
    return filter.processSample(0, audio);
}
