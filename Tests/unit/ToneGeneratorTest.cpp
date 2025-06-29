#pragma once

#include <JuceHeader.h>
#include "../../Source/CS01Synth/ToneGenerator.h"
#include "../mocks/MockToneGenerator.h"

// Helper class to manage APVTS and processor lifecycle
class APVTSHolder
{
public:
    APVTSHolder()
    {
        // Create a dummy processor
        dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();
        
        // Create parameter layout
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        
        layout.add(std::make_unique<juce::AudioParameterFloat>("vco_waveform", "Waveform", 0.0f, 4.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>("vco_octave", "Octave", -2.0f, 2.0f, 0.0f));
        
        // Create APVTS
        apvts = std::make_unique<juce::AudioProcessorValueTreeState>(*dummyProcessor, nullptr, "Parameters", std::move(layout));
    }
    
    ~APVTSHolder() = default;
    
    juce::AudioProcessorValueTreeState& getAPVTS() { return *apvts; }
    
private:
    std::unique_ptr<juce::AudioProcessorGraph> dummyProcessor;
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
};

class ToneGeneratorTest : public juce::UnitTest
{
public:
    ToneGeneratorTest() : juce::UnitTest("ToneGenerator Tests") {}
    
    void runTest() override
    {
        testInitialization();
    }
    
private:
    void testInitialization()
    {
        beginTest("Initialization Test");
        
        // Create APVTS holder to manage memory
        APVTSHolder apvtsHolder;
        
        // Create mock tone generator
        testing::MockToneGenerator toneGenerator(apvtsHolder.getAPVTS());
        
        // Check initial state
        expect(!toneGenerator.isActive());
        expectEquals(toneGenerator.getCurrentlyPlayingNote(), 0);
    }
};

static ToneGeneratorTest toneGeneratorTest;
