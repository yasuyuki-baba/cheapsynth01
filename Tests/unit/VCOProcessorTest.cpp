#pragma once

#include <JuceHeader.h>
#include "../../Source/CS01Synth/VCOProcessor.h"

class VCOProcessorTest : public juce::UnitTest
{
public:
    VCOProcessorTest() : juce::UnitTest("VCOProcessor Tests") {}
    
    void runTest() override
    {
        // Tests will be implemented in the future
    }
};

static VCOProcessorTest vcoProcessorTest;
