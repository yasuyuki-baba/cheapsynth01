#pragma once

#include <JuceHeader.h>
#include "../Parameters.h"

//==============================================================================
// ModernVCFProcessor - Filter using JUCE's StateVariableTPTFilter (with less distortion)
class ModernVCFProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    ModernVCFProcessor(juce::AudioProcessorValueTreeState& apvts);
    ~ModernVCFProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    //==============================================================================
    const juce::String getName() const override { return "Modern VCF"; }

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
    std::vector<juce::dsp::StateVariableTPTFilter<float>> filters; // Filter for each channel
    
    // Cutoff frequency mapping function
    float mapCutoffFrequency(float cutoffParam)
    {
        // Range covering the entire audible spectrum
        const float minFreq = 20.0f;  // Minimum cutoff frequency
        const float maxFreq = 20000.0f; // Maximum cutoff frequency
        
        // Verify input range (actual frequency value)
        float cutoffFreq = juce::jlimit(minFreq, maxFreq, cutoffParam);
        
        return cutoffFreq;
    }
    
    // Resonance mapping function
    float mapResonance(bool isHighResonance)
    {
        // Simple resonance value based on switch state
        if (isHighResonance)
        {
            // High resonance setting
            return 0.8f;
        }
        else
        {
            // Low resonance setting
            return 0.2f;
        }
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModernVCFProcessor)
};
