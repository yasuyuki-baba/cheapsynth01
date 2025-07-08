#pragma once

#include <JuceHeader.h>
#include <functional>
#include "EGProcessor.h"
#include "ISoundGenerator.h"

class MidiProcessor : public juce::AudioProcessor {
   public:
    MidiProcessor(juce::AudioProcessorValueTreeState& apvts);
    ~MidiProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    const juce::String getName() const override {
        return "MIDI Processor";
    }
    juce::AudioProcessorEditor* createEditor() override {
        return nullptr;
    }
    bool hasEditor() const override {
        return false;
    }
    bool acceptsMidi() const override {
        return true;
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

    // Set sound generator
    void setSoundGenerator(ISoundGenerator* generator) {
        soundGenerator = generator;
    }

    // Set EG processor
    void setEGProcessor(EGProcessor* processor) {
        egProcessor = processor;
    }

    // Get currently playing note
    int getCurrentlyPlayingNote() const {
        return activeNotes.isEmpty() ? 0 : activeNotes.getLast();
    }

    // Get sound generator
    ISoundGenerator* getSoundGenerator() const {
        return soundGenerator;
    }

    // Get array of active notes
    const juce::Array<int>& getActiveNotes() const {
        return activeNotes;
    }

   private:
    // MIDI processing methods
    void handleMidiEvent(const juce::MidiMessage& midiMessage, juce::MidiBuffer&);
    void handleNoteOn(const juce::MidiMessage& midiMessage);
    void handleNoteOff(const juce::MidiMessage& midiMessage);
    void handlePitchWheel(const juce::MidiMessage& midiMessage);
    void handleControllerMessage(const juce::MidiMessage& midiMessage);

    // 14bit CC parameter update methods
    void updateModulationParameter();
    void updateBreathParameter();
    void updateVolumeParameter();
    void updateGlissandoParameter();

    juce::AudioProcessorValueTreeState& apvts;
    ISoundGenerator* soundGenerator = nullptr;
    EGProcessor* egProcessor = nullptr;

    // For monophonic sound management
    juce::Array<int> activeNotes;
    int lastPitchWheelValue = 8192;  // Center value

    // 14bit CC values storage
    int modulationMSB = 0, modulationLSB = 0;     // CC #1/#33
    int breathMSB = 0, breathLSB = 0;             // CC #2/#34  
    int volumeMSB = 0, volumeLSB = 0;             // CC #7/#39
    int glissandoMSB = 0, glissandoLSB = 0;       // CC #35/#37

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiProcessor)
};
