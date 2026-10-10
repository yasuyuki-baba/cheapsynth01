#pragma once

#include "DSP/Primitives.h"

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
    void beginStage(Stage next, double endpoint, double seconds);
    float nextEnvelopeSample();
};
