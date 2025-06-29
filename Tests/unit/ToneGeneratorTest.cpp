#pragma once

#include <JuceHeader.h>
#include "../../Source/CS01Synth/ToneGenerator.h"
#include "../mocks/MockToneGenerator.h"

// Helper function to create a dummy APVTS for testing
juce::AudioProcessorValueTreeState* createDummyAPVTS()
{
    // Create a dummy processor
    auto* dummyProcessor = new juce::AudioProcessorGraph();
    
    // Create parameter layout
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("vco_waveform", "Waveform", 0.0f, 4.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("vco_octave", "Octave", -2.0f, 2.0f, 0.0f));
    
    // Create APVTS
    return new juce::AudioProcessorValueTreeState(*dummyProcessor, nullptr, "Parameters", std::move(layout));
}

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
        
        // Create APVTS with smart pointer to manage memory
        std::unique_ptr<juce::AudioProcessorValueTreeState> apvts(createDummyAPVTS());
        
        // Create mock tone generator
        testing::MockToneGenerator toneGenerator(*apvts);
        
        // Check initial state
        expect(!toneGenerator.isActive());
        expectEquals(toneGenerator.getCurrentlyPlayingNote(), 0);
    }
};

static ToneGeneratorTest toneGeneratorTest;
