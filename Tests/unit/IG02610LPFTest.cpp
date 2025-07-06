#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "../../Source/CS01Synth/IG02610LPF.h"

// Test fixture for IG02610LPF tests
class IG02610LPFTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Set up test environment
        filter = std::make_unique<IG02610LPF>();
    }
    
    void TearDown() override
    {
        filter.reset();
    }
    
    std::unique_ptr<IG02610LPF> filter;
};

TEST_F(IG02610LPFTest, Initialization)
{
    // Just check that the filter can be created without crashing
    EXPECT_TRUE(true);
    
    // Check that the filter doesn't crash when used before preparation
    float sample = filter->processSample(0, 0.5f);
    EXPECT_TRUE(std::isfinite(sample));
}
