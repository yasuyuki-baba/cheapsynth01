#include "CS01Synth/ModernVCFProcessor.h"

#include <cmath>

//==============================================================================
ModernVCFProcessor::ModernVCFProcessor(cs01::ParameterState& parameters) : parameters(parameters) {}

ModernVCFProcessor::~ModernVCFProcessor() {}

//==============================================================================
void ModernVCFProcessor::prepareToPlay(double sampleRate, int) {
    filter.reset();
    filter.prepare(sampleRate);
    processingSampleRate = sampleRate;
}

void ModernVCFProcessor::releaseResources() {
    // Reset filter
    filter.reset();
}

float ModernVCFProcessor::processSample(float audio, float eg, float lfo) {
    const float depth = parameters.get(ParameterIds::vcfEgDepth);
    const float base = calculateCutoffFrequency(parameters.get(ParameterIds::cutoff));
    const float semitones =
        eg * depth * 36.f +
        std::clamp(lfo, -1.f, 1.f) * parameters.get(ParameterIds::modDepth) * 24.f +
        parameters.get(ParameterIds::breathInput) * parameters.get(ParameterIds::breathVcf) * 24.f;
    float cutoff = base * std::exp2(semitones / 12.f);
    if (!std::isfinite(cutoff))
        cutoff = base;
    filter.setCutoffFrequency(std::clamp(
        cutoff, 20.f, std::min(20000.0f, static_cast<float>(processingSampleRate * .49))));
    filter.setResonance(calculateResonance(parameters.get(ParameterIds::resonance)));
    return filter.processSample(0, audio);
}
