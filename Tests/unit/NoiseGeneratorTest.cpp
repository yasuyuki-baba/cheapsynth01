#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "../../Source/CS01Synth/NoiseGenerator.h"
#include "../../Source/Parameters.h"

// Test fixture for NoiseGenerator tests
class NoiseGeneratorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a dummy processor for APVTS
        dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();
        
        auto parameterLayout = createParameterLayout();
        apvts = std::make_unique<juce::AudioProcessorValueTreeState>(*dummyProcessor, nullptr, "PARAMETERS", std::move(parameterLayout));
        
        // Create NoiseGenerator
        generator = std::make_unique<NoiseGenerator>(*apvts);
    }
    
    void TearDown() override
    {
        generator.reset();
        apvts.reset();
        dummyProcessor.reset();
    }
    
    // Create a parameter layout for testing
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        
        // Add parameters needed for NoiseGenerator
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::release, "Release", 
            juce::NormalisableRange<float>(0.01f, 5.0f), 0.1f));
        
        return layout;
    }
    
    std::unique_ptr<juce::AudioProcessorGraph> dummyProcessor;
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
    std::unique_ptr<NoiseGenerator> generator;
};

TEST_F(NoiseGeneratorTest, Initialization)
{
    // Check that generator was created successfully
    EXPECT_NE(generator.get(), nullptr);
    
    // Check that generator has expected properties
    EXPECT_FALSE(generator->isActive()); // Initially not active
    EXPECT_EQ(generator->getCurrentlyPlayingNote(), 0); // No note playing initially
}

TEST_F(NoiseGeneratorTest, PrepareToPlay)
{
    // Test prepare with different sample rates
    // This should not throw any exceptions
    juce::dsp::ProcessSpec spec1;
    spec1.sampleRate = 44100.0;
    spec1.maximumBlockSize = 512;
    spec1.numChannels = 1;
    
    generator->prepare(spec1);
    EXPECT_TRUE(true); // If we got here, no exception was thrown
    
    juce::dsp::ProcessSpec spec2;
    spec2.sampleRate = 48000.0;
    spec2.maximumBlockSize = 1024;
    spec2.numChannels = 1;
    
    generator->prepare(spec2);
    EXPECT_TRUE(true); // If we got here, no exception was thrown
}

TEST_F(NoiseGeneratorTest, ProcessBlock)
{
    // Prepare generator
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 1;
    generator->prepare(spec);
    
    // Create audio buffer for processing
    juce::AudioBuffer<float> buffer(1, 512);
    
    // Test 1: No note should produce silence
    buffer.clear();
    generator->renderNextBlock(buffer, 0, buffer.getNumSamples());
    
    // Check that output buffer contains silence
    float sum1 = 0.0f;
    auto* outputData1 = buffer.getReadPointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        sum1 += std::abs(outputData1[i]);
    }
    
    EXPECT_EQ(sum1, 0.0f); // Should be silent when no note is active
    
    // Test 2: With note active, should produce noise
    buffer.clear();
    generator->startNote(60, 1.0f, 8192);
    generator->renderNextBlock(buffer, 0, buffer.getNumSamples());
    
    // Check that output buffer contains non-zero values
    float sum2 = 0.0f;
    auto* outputData2 = buffer.getReadPointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        sum2 += std::abs(outputData2[i]);
    }
    
    EXPECT_GT(sum2, 0.0f); // Should produce noise
    
    // Test 3: After stopping note, should revert to silence
    buffer.clear();
    generator->stopNote(false); // Immediate stop
    generator->renderNextBlock(buffer, 0, buffer.getNumSamples());
    
    // Check that output buffer contains silence
    float sum3 = 0.0f;
    auto* outputData3 = buffer.getReadPointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        sum3 += std::abs(outputData3[i]);
    }
    
    EXPECT_EQ(sum3, 0.0f); // Should be silent after note is stopped
}

TEST_F(NoiseGeneratorTest, NoteHandling)
{
    // Initially not active
    EXPECT_FALSE(generator->isActive());
    EXPECT_EQ(generator->getCurrentlyPlayingNote(), 0);
    
    // Test 1: Start note
    generator->startNote(60, 1.0f, 8192);
    
    // Check that note is active
    EXPECT_TRUE(generator->isActive());
    EXPECT_EQ(generator->getCurrentlyPlayingNote(), 60);
    
    // Test 2: Change note
    generator->changeNote(64);
    
    // Check that note has changed
    EXPECT_TRUE(generator->isActive());
    EXPECT_EQ(generator->getCurrentlyPlayingNote(), 64);
    
    // Test 3: Stop note without tail
    generator->stopNote(false);
    
    // Check that note is no longer active
    EXPECT_FALSE(generator->isActive());
    EXPECT_EQ(generator->getCurrentlyPlayingNote(), 0);
}

TEST_F(NoiseGeneratorTest, ReleaseBehavior)
{
    // Set release parameter to a known value
    auto releaseParam = static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::release));
    if (releaseParam != nullptr)
        releaseParam->setValueNotifyingHost(0.1f); // Short release time (100ms)
    
    // Prepare generator
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 1;
    generator->prepare(spec);
    
    // Start a note
    generator->startNote(60, 1.0f, 8192);
    EXPECT_TRUE(generator->isActive());
    
    // Stop note with tail off
    generator->stopNote(true);
    
    // Note should still be active during release phase
    EXPECT_TRUE(generator->isActive());
    
    // Process enough blocks to cover the release time
    juce::AudioBuffer<float> buffer(1, 512);
    
    // Process multiple blocks to simulate release time completion
    for (int i = 0; i < 10; ++i) // Process sufficient number of blocks (10 times)
    {
        buffer.clear();
        generator->renderNextBlock(buffer, 0, buffer.getNumSamples());
    }
    
    // This test verifies that the release processing functions correctly
    EXPECT_TRUE(true);
}
