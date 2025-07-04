#pragma once

#include <JuceHeader.h>
#include "../Parameters.h"
#include "IG02610LPF.h" // Include the IG02610LPF filter

//==============================================================================
class CS01VCFProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    CS01VCFProcessor(juce::AudioProcessorValueTreeState& apvts);
    ~CS01VCFProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    //==============================================================================
    const juce::String getName() const override { return "VCF"; }

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
    IG02610LPF filter; // Using IG02610LPF instead of StateVariableTPTFilter
    juce::HeapBlock<float> modulationBuffer; // バッファを事前に確保して再利用
    
    // Cutoff frequency calculation function
    float calculateCutoffFrequency(float cutoffParam)
    {
        // Range covering the entire audible spectrum
        const float minFreq = 20.0f;  // Minimum cutoff frequency
        const float maxFreq = 20000.0f; // Maximum cutoff frequency
        
        // Verify input range (actual frequency value)
        float cutoffFreq = juce::jlimit(minFreq, maxFreq, cutoffParam);
        
        return cutoffFreq;
    }
    
    // Resonance calculation function
    float calculateResonance(bool isHighResonance)
    {
        // Simple resonance value based on switch state
        if (isHighResonance)
        {
            // High resonance setting - IG02610LPF has max resonance of 0.8f
            return 0.7f;
        }
        else
        {
            // Low resonance setting
            return 0.2f;
        }
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CS01VCFProcessor)
};
