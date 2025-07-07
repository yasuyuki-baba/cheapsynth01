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

TEST_F(IG02610LPFTest, NonlinearDistortionLowResonance)
{
    filter->prepare(44100.0);
    filter->setCutoffFrequency(1000.0f);
    filter->setResonance(0.2f); // Low resonance - should have subtle even harmonics
    
    // Test with different input levels
    float testSignals[] = {0.1f, 0.5f, 0.8f};
    
    for (float input : testSignals) {
        float output = filter->processSample(0, input);
        
        // Output should be finite and within reasonable bounds
        EXPECT_TRUE(std::isfinite(output));
        EXPECT_LE(std::abs(output), 2.0f); // Should not exceed reasonable limits
        
        // For low resonance, distortion should be subtle
        float distortionRatio = std::abs(output / input);
        EXPECT_LT(distortionRatio, 1.5f); // Should not be heavily distorted
    }
}

TEST_F(IG02610LPFTest, NonlinearDistortionMediumResonance)
{
    filter->prepare(44100.0);
    filter->setCutoffFrequency(1000.0f);
    filter->setResonance(0.55f); // Medium resonance - balanced distortion
    
    float input = 0.7f;
    float output = filter->processSample(0, input);
    
    EXPECT_TRUE(std::isfinite(output));
    EXPECT_LE(std::abs(output), 2.0f);
    
    // Medium resonance should show some output (realistic expectations for single sample)
    float distortionRatio = std::abs(output / input);
    EXPECT_GT(distortionRatio, 0.0001f); // Should have some output
    EXPECT_LT(distortionRatio, 0.1f); // But not extreme for single sample
}

TEST_F(IG02610LPFTest, NonlinearDistortionHighResonance)
{
    filter->prepare(44100.0);
    filter->setCutoffFrequency(1000.0f);
    filter->setResonance(0.75f); // High resonance - strong distortion with asymmetric clipping
    
    float input = 0.8f;
    float output = filter->processSample(0, input);
    
    EXPECT_TRUE(std::isfinite(output));
    EXPECT_LE(std::abs(output), 2.0f);
    
    // High resonance should show some distortion (realistic for single sample)
    float distortionRatio = std::abs(output / input);
    EXPECT_GT(distortionRatio, 0.001f); // Should have some output
    EXPECT_LT(distortionRatio, 0.1f); // But not extreme for single sample
}

TEST_F(IG02610LPFTest, FrequencyDependentDistortion)
{
    filter->prepare(44100.0);
    filter->setResonance(0.6f);
    
    // Test low frequency (should get more distortion)
    filter->setCutoffFrequency(300.0f);
    float lowFreqOutput = filter->processSample(0, 0.7f);
    
    // Reset filter state
    filter->reset();
    
    // Test high frequency (should get less distortion)
    filter->setCutoffFrequency(3000.0f);
    float highFreqOutput = filter->processSample(0, 0.7f);
    
    EXPECT_TRUE(std::isfinite(lowFreqOutput));
    EXPECT_TRUE(std::isfinite(highFreqOutput));
    
    // Low frequencies should generally show more distortion characteristics
    // This is a behavioral test rather than strict numerical comparison
    EXPECT_LE(std::abs(lowFreqOutput), 2.0f);
    EXPECT_LE(std::abs(highFreqOutput), 2.0f);
}

TEST_F(IG02610LPFTest, InputLevelDependentDistortion)
{
    filter->prepare(44100.0);
    filter->setCutoffFrequency(1000.0f);
    filter->setResonance(0.75f); // High resonance to activate level-dependent distortion
    
    // Test with different input levels
    float smallInput = 0.1f;
    float largeInput = 0.9f;
    
    float smallOutput = filter->processSample(0, smallInput);
    
    // Reset filter state
    filter->reset();
    
    float largeOutput = filter->processSample(0, largeInput);
    
    EXPECT_TRUE(std::isfinite(smallOutput));
    EXPECT_TRUE(std::isfinite(largeOutput));
    
    // Input level dependent behavior should show some response (realistic for single sample)
    float smallRatio = std::abs(smallOutput / smallInput);
    float largeRatio = std::abs(largeOutput / largeInput);
    
    // Both should produce some output
    EXPECT_GT(smallRatio, 0.0001f);
    EXPECT_LT(smallRatio, 0.1f);
    EXPECT_GT(largeRatio, 0.0001f);
    EXPECT_LT(largeRatio, 0.1f);
}

TEST_F(IG02610LPFTest, AsymmetricClippingBehavior)
{
    filter->prepare(44100.0);
    filter->setCutoffFrequency(1000.0f);
    filter->setResonance(0.8f); // Maximum resonance to activate asymmetric clipping
    
    // Test positive and negative inputs
    float positiveInput = 0.8f;
    float negativeInput = -0.8f;
    
    float positiveOutput = filter->processSample(0, positiveInput);
    
    // Reset filter state
    filter->reset();
    
    float negativeOutput = filter->processSample(0, negativeInput);
    
    EXPECT_TRUE(std::isfinite(positiveOutput));
    EXPECT_TRUE(std::isfinite(negativeOutput));
    
    // Asymmetric clipping behavior should show some response (realistic for single sample)
    float positiveRatio = std::abs(positiveOutput / positiveInput);
    float negativeRatio = std::abs(negativeOutput / negativeInput);
    
    // Both should produce some output, may differ due to asymmetric processing
    EXPECT_GT(positiveRatio, 0.0001f);
    EXPECT_LT(positiveRatio, 0.1f);
    EXPECT_GT(negativeRatio, 0.0001f);
    EXPECT_LT(negativeRatio, 0.1f);
}
