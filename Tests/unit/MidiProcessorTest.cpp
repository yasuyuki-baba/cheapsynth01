#pragma once

#include <JuceHeader.h>
#include "../../Source/CS01Synth/MidiProcessor.h"

class MidiProcessorTest : public juce::UnitTest
{
public:
    MidiProcessorTest() : juce::UnitTest("MidiProcessor Tests") {}
    
    void runTest() override
    {
        // Tests will be implemented in the future
    }
};

static MidiProcessorTest midiProcessorTest;
