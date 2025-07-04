#include <JuceHeader.h>
#include "../../Source/CS01Synth/IG02610LPF.h"

class IG02610LPFTest : public juce::UnitTest
{
public:
    IG02610LPFTest() : juce::UnitTest("IG02610LPF Tests") {}
    
    void runTest() override
    {
        testInitialization();
    }
    
private:
    void testInitialization()
    {
        beginTest("Initialization Test");
        
        IG02610LPF filter;
        
        // Just check that the filter can be created without crashing
        expect(true);
        
        // Check that the filter doesn't crash when used before preparation
        float sample = filter.processSample(0, 0.5f);
        expect(std::isfinite(sample));
    }
};

static IG02610LPFTest ig02610LPFTest;
