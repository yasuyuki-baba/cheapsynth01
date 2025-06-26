#pragma once

#include <JuceHeader.h>
#include "../Parameters.h"

//==============================================================================
class CS01LFOProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    CS01LFOProcessor(juce::AudioProcessorValueTreeState& apvts);
    ~CS01LFOProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    //==============================================================================
    const juce::String getName() const override { return "LFO"; }

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
    void updateParameters();

    juce::AudioProcessorValueTreeState& apvts;
    juce::dsp::Oscillator<float> lfo;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CS01LFOProcessor)
};
