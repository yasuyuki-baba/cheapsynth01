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
        
        // Create processor with unique_ptr to ensure proper cleanup
        std::unique_ptr<CS01AudioProcessor> processor = std::make_unique<CS01AudioProcessor>();
        
        // Check that processor was created successfully
        expect(processor != nullptr);
        
        // Check that processor has expected properties
        expectEquals(processor->getName(), juce::String("CS01 Emulator"));
        expect(processor->acceptsMidi());
        expect(!processor->producesMidi());
        expect(processor->isMidiEffect() == false);
        
        // Explicitly reset the processor to trigger cleanup
        processor.reset();
        
        // Allow a short delay for any async cleanup
        juce::Thread::sleep(10);
    }
};

static AudioGraphTest audioGraphTest;
