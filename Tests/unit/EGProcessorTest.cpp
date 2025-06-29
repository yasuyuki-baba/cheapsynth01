#pragma once

#include <JuceHeader.h>
#include "../../Source/CS01Synth/EGProcessor.h"

class EGProcessorTest : public juce::UnitTest
{
public:
    EGProcessorTest() : juce::UnitTest("EGProcessor Tests") {}
    
    void runTest() override
    {
        // Tests will be implemented in the future
    }
};

static EGProcessorTest egProcessorTest;
