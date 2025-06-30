#pragma once

#include <JuceHeader.h>
#include "../../Source/CS01Synth/ModernVCFProcessor.h"

class ModernVCFProcessorTest : public juce::UnitTest
{
public:
    ModernVCFProcessorTest() : juce::UnitTest("ModernVCFProcessor Tests") {}
    
    void runTest() override
    {
        // Tests will be implemented in the future
    }
};

static ModernVCFProcessorTest modernVCFProcessorTest;
