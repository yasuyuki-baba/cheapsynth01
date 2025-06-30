#pragma once

#include <JuceHeader.h>
#include "../../Source/CS01Synth/NoiseProcessor.h"

class NoiseProcessorTest : public juce::UnitTest
{
public:
    NoiseProcessorTest() : juce::UnitTest("NoiseProcessor Tests") {}
    
    void runTest() override
    {
        // Tests will be implemented in the future
    }
};

static NoiseProcessorTest noiseProcessorTest;
