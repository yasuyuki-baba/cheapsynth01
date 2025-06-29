#pragma once

#include <JuceHeader.h>
#include "../../Source/CS01AudioProcessor.h"

class AudioGraphTest : public juce::UnitTest
{
public:
    AudioGraphTest() : juce::UnitTest("Audio Graph Integration Tests") {}
    
    void runTest() override
    {
        testProcessorCreation();
    }
    
private:
    void testProcessorCreation()
    {
        beginTest("Processor Creation Test");
        
        // Smart pointers to prevent memory leaks
        std::unique_ptr<juce::AudioProcessor> processorToDelete;
        
        // Create processor
        processorToDelete.reset(new CS01AudioProcessor());
        CS01AudioProcessor* processor = static_cast<CS01AudioProcessor*>(processorToDelete.get());
        
        // Check that processor was created successfully
        expect(processor != nullptr);
        
        // Check that processor has expected properties
        expectEquals(processor->getName(), juce::String("CS01 Emulator"));
        expect(processor->acceptsMidi());
        expect(!processor->producesMidi());
        expect(processor->isMidiEffect() == false);
    }
};

static AudioGraphTest audioGraphTest;
