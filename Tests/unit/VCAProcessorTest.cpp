#pragma once

#include <JuceHeader.h>
#include "../../Source/CS01Synth/VCAProcessor.h"

class VCAProcessorTest : public juce::UnitTest
{
public:
    VCAProcessorTest() : juce::UnitTest("VCAProcessor Tests") {}
    
    void runTest() override
    {
        // Tests will be implemented in the future
    }
};

static VCAProcessorTest vcaProcessorTest;
