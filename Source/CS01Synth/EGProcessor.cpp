#include "CS01Synth/EGProcessor.h"

#include <cmath>

//==============================================================================
EGProcessor::EGProcessor(cs01::ParameterState& parameters) : parameters(parameters) {}

EGProcessor::~EGProcessor() {}

//==============================================================================
void EGProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    stopEnvelopeImmediately();
    envelopeSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    updateADSR();
}

void EGProcessor::releaseResources() {
    stopEnvelopeImmediately();
}

float EGProcessor::processSample() {
    updateADSR();
    lastOutput = nextEnvelopeSample();
    return lastOutput;
}

// Exponential envelope shaping retained from the existing model.
void EGProcessor::beginStage(Stage next, double endpoint, double seconds) {
    stage = next;
    stageEndpoint = endpoint;
    const double distance = endpoint - level;
    stageTarget = level + distance / (-std::expm1(-2.0));
    stageCoefficient = -std::expm1(-2.0 / (std::max(seconds, 1.0e-6) * envelopeSampleRate));
    remainingSamples = std::max<int64_t>(
        1, static_cast<int64_t>(std::ceil(std::max(seconds, 0.0) * envelopeSampleRate)));
}

float EGProcessor::nextEnvelopeSample() {
    if (stage == Stage::idle)
        return 0.0f;
    if (stage != Stage::sustain) {
        level += (stageTarget - level) * stageCoefficient;
        if (--remainingSamples <= 0) {
            level = stageEndpoint;
            if (stage == Stage::attack)
                beginStage(Stage::decay, settings.sustain, settings.decay);
            else if (stage == Stage::release)
                stage = Stage::idle;
            else
                stage = Stage::sustain;
        }
    }
    return static_cast<float>(std::clamp(level, 0.0, 1.0));
}

void EGProcessor::startEnvelope() {
    updateADSR();
    beginStage(Stage::attack, 1.0, settings.attack);
}

void EGProcessor::releaseEnvelope() {
    updateADSR();
    if (stage != Stage::idle)
        beginStage(Stage::release, 0.0, settings.release);
}

void EGProcessor::stopEnvelopeImmediately() {
    stage = Stage::idle;
    level = 0.0;
    lastOutput = 0.0f;
    remainingSamples = 0;
}

void EGProcessor::updateADSR() {
    cs01::EnvelopeSettings next;
    next.attack = parameters.get(ParameterIds::attack);
    next.decay = parameters.get(ParameterIds::decay);
    next.sustain = parameters.get(ParameterIds::sustain);
    next.release = parameters.get(ParameterIds::release);
    const auto previous = settings;
    settings = next;
    // Edits restart only the affected stage from its current level. Unrelated
    // edits never overwrite a running release; sustain changes slew via decay.
    if (stage == Stage::attack && next.attack != previous.attack)
        beginStage(Stage::attack, 1.0, next.attack);
    else if (stage == Stage::release && next.release != previous.release)
        beginStage(Stage::release, 0.0, next.release);
    else if ((stage == Stage::decay || stage == Stage::sustain) &&
             (next.sustain != previous.sustain ||
              (stage == Stage::decay && next.decay != previous.decay)))
        beginStage(Stage::decay, next.sustain, next.decay);
}
