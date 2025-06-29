#pragma once

#include <JuceHeader.h>
#include "../../Source/CS01Synth/LFOProcessor.h"

class LFOProcessorTest : public juce::UnitTest
{
public:
    LFOProcessorTest() : juce::UnitTest("LFOProcessor Tests") {}
    
    void runTest() override
    {
        // Tests will be implemented in the future
    }
};

static LFOProcessorTest lfoProcessorTest;
