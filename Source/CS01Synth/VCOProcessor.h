#pragma once

#include <JuceHeader.h>

#include "CS01Synth/ISoundGenerator.h"
#include "CS01Synth/NoiseGenerator.h"
#include "CS01Synth/ToneGenerator.h"
#include "Parameters.h"

#include <atomic>
#include <functional>
#include <memory>

/**
 * VCOProcessor - Processor responsible for sound generation and LFO processing
 *
 * This class can hold different sound generators (ToneGenerator, NoiseGenerator)
 * and processes LFO input to generate sound.
 * MIDI processing is delegated to the MidiProcessor class.
 */
class VCOProcessor : public juce::AudioProcessor,
                     public juce::AudioProcessorValueTreeState::Listener {
   public:
    VCOProcessor(juce::AudioProcessorValueTreeState& vts, bool isNoiseMode = false);
    ~VCOProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void setExternalOversampling(bool enabled) {
        toneGenerator->setExternalOversampling(enabled);
    }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override {
        return nullptr;
    }
    bool hasEditor() const override {
        return false;
    }

    const juce::String getName() const override {
        return "VCOProcessor";
    }
    bool acceptsMidi() const override {
        return false;
    }
    bool producesMidi() const override {
        return false;
    }
    double getTailLengthSeconds() const override {
        return 0.0;
    }

    int getNumPrograms() override {
        return 1;
    }
    int getCurrentProgram() override {
        return 0;
    }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {
        return {};
    }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    // AudioProcessorValueTreeState::Listener implementation
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Call on the audio thread before MIDI events or rendering; never from the UI.
    void applyPendingGeneratorChange();

    // Audio-thread access only while processing is active.
    ISoundGenerator* getSoundGenerator() {
        return currentGenerator;
    }

    // Method to notify when generator type changes (for external use)
    std::function<void()> onGeneratorTypeChanged;

    // Check if noise generator is active
    bool isNoiseMode() const {
        return currentGenerator == noiseGenerator.get();
    }

   private:
    juce::AudioProcessorValueTreeState& apvts;
    std::unique_ptr<ToneGenerator> toneGenerator;
    std::unique_ptr<NoiseGenerator> noiseGenerator;
    ISoundGenerator* currentGenerator;  // Pointer to the currently selected generator
    std::atomic<bool> requestedNoiseMode{false};
    int observedFeet = -1;  // audio-thread snapshot, including silent program application
};
