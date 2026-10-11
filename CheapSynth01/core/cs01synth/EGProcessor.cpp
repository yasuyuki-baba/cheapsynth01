#include "cs01synth/EGProcessor.h"

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
    if (noteGateRemainingSamples > 0) {
        noteGateLevel += noteGateIncrement;
        if (--noteGateRemainingSamples == 0)
            noteGateLevel = noteGateTarget;
    }
    return lastOutput;
}

// A fresh stage keeps the existing k=2 curve. Attack uses a fixed target, so
// retriggering retains charge rather than choosing a new target to fill a whole
// duration. Targets and curvature remain software approximations, not IC voltages.
void EGProcessor::beginStage(Stage next, double endpoint, double seconds) {
    stage = next;
    stageEndpoint = endpoint;
    const double reference = next == Stage::attack ? 0.0 : level;
    stageTarget = reference + (endpoint - reference) / (-std::expm1(-2.0));
    remainingSamples = 0;
    updateStageTiming(seconds);
}

void EGProcessor::updateStageTiming(double seconds) {
    seconds = std::max(seconds, 1.0e-6);
    const double previousSeconds = stageReferenceSeconds;
    stageReferenceSeconds = seconds;
    stageCoefficient = -std::expm1(-2.0 / (seconds * envelopeSampleRate));
    const double endpointDistance = std::abs(stageTarget - stageEndpoint);
    const double currentDistance = std::abs(stageTarget - level);
    // Remaining threshold time, not a restarted full stage. A zero-distance
    // branch retains a clock for the independent non-EG gate, even at sustain
    // zero. Speed edits rescale that clock rather than dropping its release.
    const double remainingSeconds =
        endpointDistance == 0.0
            ? (remainingSamples > 0 && previousSeconds > 0.0
                   ? remainingSamples * seconds / (envelopeSampleRate * previousSeconds)
                   : seconds)
        : currentDistance > endpointDistance
            ? seconds * 0.5 * std::log(currentDistance / endpointDistance)
            : 0.0;
    remainingSamples = std::max<int64_t>(
        1, static_cast<int64_t>(std::ceil(remainingSeconds * envelopeSampleRate)));
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
    // A short implementation gate only for the non-EG part of VCA gain.
    // The user-selected EG attack remains unchanged.
    beginNoteGate(1.0, 0.001);
}

void EGProcessor::releaseEnvelope() {
    updateADSR();
    if (stage != Stage::idle) {
        beginStage(Stage::release, 0.0, settings.release);
        beginNoteGate(0.0, settings.release);
    }
}

void EGProcessor::beginNoteGate(double target, double seconds) {
    noteGateTarget = target;
    noteGateRemainingSamples =
        std::max<int64_t>(1, static_cast<int64_t>(std::ceil(seconds * envelopeSampleRate)));
    noteGateIncrement = (target - noteGateLevel) / noteGateRemainingSamples;
}

void EGProcessor::stopEnvelopeImmediately() {
    stage = Stage::idle;
    level = 0.0;
    lastOutput = 0.0f;
    remainingSamples = 0;
    noteGateLevel = noteGateTarget = noteGateIncrement = 0.0;
    noteGateRemainingSamples = 0;
}

void EGProcessor::updateADSR() {
    cs01::EnvelopeSettings next;
    next.attack = parameters.get(ParameterIds::attack);
    next.decay = parameters.get(ParameterIds::decay);
    next.sustain = parameters.get(ParameterIds::sustain);
    next.release = parameters.get(ParameterIds::release);
    const auto previous = settings;
    settings = next;
    // Time edits change the branch speed without changing its stored level or
    // target. Sustain edits select a new provisional branch. Unrelated edits
    // never overwrite a running release.
    if (stage == Stage::attack && next.attack != previous.attack)
        updateStageTiming(next.attack);
    else if (stage == Stage::release && next.release != previous.release) {
        updateStageTiming(next.release);
        beginNoteGate(0.0, remainingSamples / envelopeSampleRate);
    } else if ((stage == Stage::decay || stage == Stage::sustain) &&
               next.sustain != previous.sustain)
        beginStage(Stage::decay, next.sustain, next.decay);
    else if (stage == Stage::decay && next.decay != previous.decay)
        updateStageTiming(next.decay);
}
