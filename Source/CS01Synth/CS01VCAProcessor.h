#pragma once

#include <JuceHeader.h>
#include "../Parameters.h"

//==============================================================================
class CS01VCAProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    CS01VCAProcessor(juce::AudioProcessorValueTreeState& apvts);
    ~CS01VCAProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    //==============================================================================
    const juce::String getName() const override { return "VCA"; }

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
    juce::AudioProcessorValueTreeState& apvts;
    
    // Input stage high-pass filter (82K resistor and 1/50 capacitor)
    juce::dsp::IIR::Filter<float> inputHighPass;
    
    // Simple DC blocking filter
    juce::dsp::IIR::Filter<float> dcBlocker;
    
    // Simple high frequency rolloff filter
    juce::dsp::IIR::Filter<float> highFreqRolloff;
    
    // IG02600 VCA chip emulation
    float processVCA(float input, float controlVoltage, float volumeParam);
    
    // Tr7 transistor buffer emulation
    float processTr7Buffer(float input);
    
    // Output coupling capacitor emulation
    float processOutputCoupling(float input);
    
    // State variables for analog circuit emulation
    float capacitorState = 0.0f;
    float prevOutput = 0.0f;
    float outCapacitorState = 0.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CS01VCAProcessor)
};
