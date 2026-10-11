#pragma once

#include "dsp/Primitives.h"

#include "Parameters.h"

//==============================================================================
class EGProcessor {
   public:
    //==============================================================================
    EGProcessor(cs01::ParameterState& parameters);
    ~EGProcessor();

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();

    float processSample();

    bool isActive() const {
        return stage != Stage::idle;
    }
    // Same-thread observation only; does not advance the envelope.
    float getLastOutputForTesting() const {
        return lastOutput;
    }

    float getNoteGate() const {
        return static_cast<float>(noteGateLevel);
    }
    int64_t getReleaseSamplesRemaining() {
        updateADSR();
        return stage == Stage::release ? remainingSamples : 0;
    }

    // Methods to control ADSR from outside
    void startEnvelope();
    void stopEnvelopeImmediately();
    void releaseEnvelope();

    //==============================================================================
   private:
    //==============================================================================
    void updateADSR();

    cs01::ParameterState& parameters;
    enum class Stage { idle, attack, decay, sustain, release };
    Stage stage = Stage::idle;
    cs01::EnvelopeSettings settings;
    double envelopeSampleRate = 44100.0;
    double level = 0.0, stageTarget = 0.0, stageEndpoint = 0.0;
    double stageCoefficient = 0.0;
    int64_t remainingSamples = 0;
    float lastOutput = 0.0f;
    double stageReferenceSeconds = 0.0;
    double noteGateLevel = 0.0, noteGateTarget = 0.0, noteGateIncrement = 0.0;
    int64_t noteGateRemainingSamples = 0;
    void beginStage(Stage next, double endpoint, double seconds);
    void updateStageTiming(double seconds);
    void beginNoteGate(double target, double seconds);
    float nextEnvelopeSample();
};
