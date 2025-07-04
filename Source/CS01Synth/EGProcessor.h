#pragma once

#include <JuceHeader.h>
#include "../Parameters.h"

//==============================================================================
class EGProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    EGProcessor(juce::AudioProcessorValueTreeState& apvts);
    ~EGProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isActive() const { return adsr.isActive(); }
    
    // Methods to control ADSR from outside
    void startEnvelope() { adsr.noteOn(); }
    void releaseEnvelope() { adsr.noteOff(); }

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    //==============================================================================
    const juce::String getName() const override { return "EG"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& newName) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override {}
    void setStateInformation (const void* data, int sizeInBytes) override {}

private:
    //==============================================================================
    void updateADSR();

    juce::AudioProcessorValueTreeState& apvts;
    juce::ADSR adsr;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EGProcessor)
};
