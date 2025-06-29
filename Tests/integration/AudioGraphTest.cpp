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
        
        {
            // Create processor in a local scope to ensure proper cleanup
            std::unique_ptr<CS01AudioProcessor> processor = std::make_unique<CS01AudioProcessor>();
            
            // Check that processor was created successfully
            expect(processor != nullptr);
            
            // Check that processor has expected properties
            expectEquals(processor->getName(), juce::String("CS01 Emulator"));
            expect(processor->acceptsMidi());
            expect(!processor->producesMidi());
            expect(processor->isMidiEffect() == false);
            
            // Processor will be automatically destroyed when leaving this scope
        }
        
        // Force garbage collection to clean up any remaining objects
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    }
};

static AudioGraphTest audioGraphTest;
