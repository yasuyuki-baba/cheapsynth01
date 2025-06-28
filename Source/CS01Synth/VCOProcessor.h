#pragma once
#include <JuceHeader.h>
#include "SynthVoice.h"
#include "INoteHandler.h"
#include "../Parameters.h"

/**
 * VCOProcessor - Processor responsible for sound generation and LFO processing
 * 
 * This class holds SynthVoice and processes LFO input to generate sound.
 * MIDI processing is delegated to the MidiProcessor class.
 */
class VCOProcessor : public juce::AudioProcessor
{
public:
    VCOProcessor(juce::AudioProcessorValueTreeState& vts);
    ~VCOProcessor() override = default;

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

    // Accessor for sound generation
    SynthVoice* getSynthVoice() { return &voice; }
    
    // Accessor for INoteHandler interface
    INoteHandler* getNoteHandler() { return &voice; }

private:
    juce::AudioProcessorValueTreeState& apvts;
    SynthVoice voice;
};
