#pragma once

#include <JuceHeader.h>
#include "../../Source/CS01Synth/CS01VCFProcessor.h"

class CS01VCFProcessorTest : public juce::UnitTest
{
public:
    CS01VCFProcessorTest() : juce::UnitTest("CS01VCFProcessor Tests") {}
    
    void runTest() override
    {
        // Tests will be implemented in the future
    }
};

static CS01VCFProcessorTest cs01VCFProcessorTest;
