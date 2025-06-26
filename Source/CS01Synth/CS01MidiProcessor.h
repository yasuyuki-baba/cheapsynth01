#pragma once

#include <JuceHeader.h>
#include <functional>
#include "CS01EGProcessor.h"

// Forward declaration
class CS01SynthVoice;

class CS01MidiProcessor : public juce::AudioProcessor
{
public:
    CS01MidiProcessor(juce::AudioProcessorValueTreeState& apvts);
    ~CS01MidiProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    const juce::String getName() const override { return "MIDI Processor"; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    // Set synth voice
    void setSynthVoice(CS01SynthVoice* voice) { synthVoice = voice; }
    
    // Set EG processor
    void setEGProcessor(CS01EGProcessor* processor) { egProcessor = processor; }

    // Get currently playing note
    int getCurrentlyPlayingNote() const { return activeNotes.isEmpty() ? 0 : activeNotes.getLast(); }
    
    // Get array of active notes
    const juce::Array<int>& getActiveNotes() const { return activeNotes; }

private:
    // MIDI processing methods
    void handleMidiEvent(const juce::MidiMessage& midiMessage, juce::MidiBuffer&);
    void handleNoteOn(const juce::MidiMessage& midiMessage);
    void handleNoteOff(const juce::MidiMessage& midiMessage);
    void handlePitchWheel(const juce::MidiMessage& midiMessage);
    void handleControllerMessage(const juce::MidiMessage& midiMessage);

    juce::AudioProcessorValueTreeState& apvts;
    CS01SynthVoice* synthVoice = nullptr;
    CS01EGProcessor* egProcessor = nullptr;
    
    // For monophonic sound management
    juce::Array<int> activeNotes;
    int lastPitchWheelValue = 8192; // Center value

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CS01MidiProcessor)
};
