#pragma once
#include <JuceHeader.h>
#include "ToneGenerator.h"
#include "NoiseGenerator.h"
#include "ISoundGenerator.h"
#include "../Parameters.h"

/**
 * VCOProcessor - Processor responsible for sound generation and LFO processing
 * 
 * This class can hold different sound generators (ToneGenerator, NoiseGenerator)
 * and processes LFO input to generate sound.
 * MIDI processing is delegated to the MidiProcessor class.
 */
class VCOProcessor : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener
{
public:
    VCOProcessor(juce::AudioProcessorValueTreeState& vts, bool isNoiseMode = false);
    ~VCOProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    const juce::String getName() const override { return "VCOProcessor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
    
    // AudioProcessorValueTreeState::Listener implementation
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Accessor for active ISoundGenerator
    ISoundGenerator* getActiveGenerator() { return activeGenerator; }
    
    // Accessor for note handler interface
    ISoundGenerator* getNoteHandler() { return activeGenerator; }
    
    // Method to notify when generator type changes (for external use)
    std::function<void()> onGeneratorTypeChanged;
    
    // Check if noise generator is active
    bool isNoiseMode() const { return activeGenerator == noiseGenerator.get(); }

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::unique_ptr<ToneGenerator> toneGenerator;
    std::unique_ptr<NoiseGenerator> noiseGenerator;
    ISoundGenerator* activeGenerator; // Pointer to the currently active generator
    juce::dsp::ProcessSpec lastSpec;
    bool isPrepared = false;
    float lfoValue = 0.0f;
};
