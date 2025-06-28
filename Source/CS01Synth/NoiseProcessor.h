#pragma once

#include <JuceHeader.h>
#include "../Parameters.h"
#include "INoteHandler.h"

/**
 * NoiseProcessor - Processor responsible for noise generation
 * 
 * This class generates white noise and processes it through a filter.
 * It is designed to be used as a node in the AudioProcessorGraph.
 */
class NoiseProcessor : public juce::AudioProcessor, public INoteHandler
{
public:
    NoiseProcessor(juce::AudioProcessorValueTreeState& vts);
    ~NoiseProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    const juce::String getName() const override { return "NoiseProcessor"; }
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

    // INoteHandler implementation
    void startNote(int midiNoteNumber, float velocity, int currentPitchWheelPosition) override;
    void stopNote(bool allowTailOff) override;
    void changeNote(int midiNoteNumber) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    bool isActive() const override;
    int getCurrentlyPlayingNote() const override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::Random random;
    juce::dsp::IIR::Filter<float> noiseFilter;
    
    // Note state
    bool noteOn = false;
    bool tailOff = false;
    int tailOffCounter = 0;
    int tailOffDuration = 0;
    int currentlyPlayingNote = 0;
    double sampleRate = 44100.0;
    int pitchWheelValue = 8192; // Center value
};
