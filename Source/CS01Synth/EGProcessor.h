#pragma once

#include <JuceHeader.h>

#include "Parameters.h"

//==============================================================================
class EGProcessor : public juce::AudioProcessor {
   public:
    //==============================================================================
    EGProcessor(juce::AudioProcessorValueTreeState& apvts);
    ~EGProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isActive() const {
        return stage != Stage::idle;
    }
    int getReleaseSamplesRemaining() {
        updateADSR();
        return stage == Stage::release ? static_cast<int>(remainingSamples) : (isActive() ? -1 : 0);
    }
    // Same-thread observation only; does not advance the envelope.
    float getLastOutputForTesting() const {
        return lastOutput;
    }
    // Parallel sample-wise note gate for the VCA's non-EG gain. The existing
    // EG audio connection orders this producer before the VCA in the graph.
    float getNoteGateForSample(int sample) const {
        return noteGateBuffer.getSample(0, sample);
    }

    // Methods to control ADSR from outside
    void startEnvelope();
    void stopEnvelopeImmediately();
    void releaseEnvelope();

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override {
        return nullptr;
    }
    bool hasEditor() const override {
        return false;
    }

    //==============================================================================
    const juce::String getName() const override {
        return "EG";
    }

    bool acceptsMidi() const override {
        return false;
    }
    bool producesMidi() const override {
        return false;
    }
    bool isMidiEffect() const override {
        return false;
    }
    double getTailLengthSeconds() const override {
        return 0.0;
    }

    //==============================================================================
    int getNumPrograms() override {
        return 1;
    }
    int getCurrentProgram() override {
        return 0;
    }
    void setCurrentProgram(int index) override {}
    const juce::String getProgramName(int index) override {
        return {};
    }
    void changeProgramName(int index, const juce::String& newName) override {}

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override {}
    void setStateInformation(const void* data, int sizeInBytes) override {}

   private:
    //==============================================================================
    void updateADSR();

    juce::AudioProcessorValueTreeState& apvts;
    enum class Stage { idle, attack, decay, sustain, release };
    Stage stage = Stage::idle;
    juce::ADSR::Parameters settings;
    double envelopeSampleRate = 44100.0;
    double level = 0.0, stageTarget = 0.0, stageEndpoint = 0.0;
    double stageCoefficient = 0.0;
    double stageReferenceSeconds = 0.0;
    int64_t remainingSamples = 0;
    float lastOutput = 0.0f;
    void beginStage(Stage next, double endpoint, double seconds);
    void updateStageTiming(double seconds);
    float nextEnvelopeSample();
    void beginNoteGate(double target, double seconds);
    juce::AudioBuffer<float> noteGateBuffer;
    double noteGateLevel = 0.0, noteGateTarget = 0.0, noteGateIncrement = 0.0;
    int64_t noteGateRemainingSamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EGProcessor)
};
