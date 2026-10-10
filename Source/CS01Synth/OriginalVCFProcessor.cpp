#include "CS01Synth/OriginalVCFProcessor.h"

#include <cmath>

//==============================================================================
OriginalVCFProcessor::OriginalVCFProcessor(cs01::ParameterState& parameters)
    : parameters(parameters) {}

OriginalVCFProcessor::~OriginalVCFProcessor() {}

//==============================================================================
void OriginalVCFProcessor::prepareToPlay(double sampleRate, int) {
    filter.reset();
    filter.prepare(sampleRate);
    egDepthControl.reset(sampleRate, .005);
    egDepthControl.setCurrentAndTargetValue(parameters.get(ParameterIds::vcfEgDepth));
}

void OriginalVCFProcessor::releaseResources() {
    // Reset filter
    filter.reset();
}

float OriginalVCFProcessor::processSample(float audio, float eg, float lfo) {
    egDepthControl.setTargetValue(parameters.get(ParameterIds::vcfEgDepth));
    const float depth = egDepthControl.getNextValue();
    const float base = calculateCutoffFrequency(parameters.get(ParameterIds::cutoff));
    const float semitones =
        eg * depth * 36.f +
        std::clamp(lfo, -1.f, 1.f) * parameters.get(ParameterIds::modDepth) * 24.f +
        parameters.get(ParameterIds::breathInput) * parameters.get(ParameterIds::breathVcf) * 24.f;
    float cutoff = base * std::exp2(semitones / 12.f);
    if (!std::isfinite(cutoff))
        cutoff = base;
    filter.setCutoffFrequency(std::clamp(cutoff, 20.f, 20000.0f));
    filter.setResonance(calculateResonance(parameters.get(ParameterIds::resonance)));
    return filter.processSample(0, audio);
}
