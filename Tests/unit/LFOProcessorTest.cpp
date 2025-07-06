#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "../../Source/CS01Synth/LFOProcessor.h"
#include "../../Source/Parameters.h"

// Test fixture for LFOProcessor tests
class LFOProcessorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a dummy processor for APVTS
        dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();
        
        auto parameterLayout = createParameterLayout();
        apvts = std::make_unique<juce::AudioProcessorValueTreeState>(*dummyProcessor, nullptr, "PARAMETERS", std::move(parameterLayout));
        
        // Create LFOProcessor
        processor = std::make_unique<LFOProcessor>(*apvts);
    }
    
    void TearDown() override
    {
        processor.reset();
        apvts.reset();
        dummyProcessor.reset();
    }
    
    // Create a parameter layout for testing
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        
        // Add parameters needed for LFOProcessor
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::lfoSpeed, "LFO Speed", 
            juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f), 1.0f));
        
        return layout;
    }
    
    std::unique_ptr<juce::AudioProcessorGraph> dummyProcessor;
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
    std::unique_ptr<LFOProcessor> processor;
};

TEST_F(LFOProcessorTest, Initialization)
{
    // Check that processor was created successfully
    EXPECT_NE(processor.get(), nullptr);
    
    // Check that processor has expected properties
    EXPECT_EQ(processor->getName(), juce::String("LFO"));
    EXPECT_FALSE(processor->acceptsMidi());
    EXPECT_FALSE(processor->producesMidi());
    EXPECT_FALSE(processor->isMidiEffect());
    
    // Check bus configuration
    EXPECT_EQ(processor->getBusCount(true), 0); // No input buses
    EXPECT_EQ(processor->getBusCount(false), 1); // 1 output bus
    
    EXPECT_EQ(processor->getBus(false, 0)->getName(), juce::String("Output"));
}

TEST_F(LFOProcessorTest, ParameterSettings)
{
    // Test LFO speed parameter
    auto* lfoSpeedParam = apvts->getParameter(ParameterIds::lfoSpeed);
    EXPECT_NE(lfoSpeedParam, nullptr);
    
    // Set LFO speed to different values
    lfoSpeedParam->setValueNotifyingHost(lfoSpeedParam->convertTo0to1(0.5f)); // 0.5 Hz
    EXPECT_NEAR(apvts->getRawParameterValue(ParameterIds::lfoSpeed)->load(), 0.5f, 0.001f);
    
    lfoSpeedParam->setValueNotifyingHost(lfoSpeedParam->convertTo0to1(5.0f)); // 5.0 Hz
    EXPECT_NEAR(apvts->getRawParameterValue(ParameterIds::lfoSpeed)->load(), 5.0f, 0.001f);
}

TEST_F(LFOProcessorTest, BusesLayout)
{
    // Test supported buses layout
    juce::AudioProcessor::BusesLayout supportedLayout;
    supportedLayout.inputBuses.clear(); // No input buses
    supportedLayout.outputBuses.add(juce::AudioChannelSet::mono()); // Mono output
    
    EXPECT_TRUE(processor->isBusesLayoutSupported(supportedLayout));
    
    // Test supported buses layout with input (LFOProcessor only checks output)
    juce::AudioProcessor::BusesLayout supportedLayout2;
    supportedLayout2.inputBuses.add(juce::AudioChannelSet::mono()); // Input bus (supported)
    supportedLayout2.outputBuses.add(juce::AudioChannelSet::mono()); // Mono output
    
    EXPECT_TRUE(processor->isBusesLayoutSupported(supportedLayout2));
    
    // Test unsupported buses layout (stereo output)
    juce::AudioProcessor::BusesLayout unsupportedLayout;
    unsupportedLayout.inputBuses.clear(); // No input buses
    unsupportedLayout.outputBuses.add(juce::AudioChannelSet::stereo()); // Stereo output (not supported)
    
    EXPECT_FALSE(processor->isBusesLayoutSupported(unsupportedLayout));
}

TEST_F(LFOProcessorTest, WaveformGeneration)
{
    
    // Prepare processor
    processor->prepareToPlay(44100.0, 512);
    
    // Create audio buffer
    juce::AudioBuffer<float> buffer(1, 512); // 1 channel for output
    juce::MidiBuffer midiBuffer;
    
    // Set LFO speed to 1 Hz
    apvts->getParameter(ParameterIds::lfoSpeed)->setValueNotifyingHost(
        apvts->getParameter(ParameterIds::lfoSpeed)->convertTo0to1(1.0f)); // 1.0 Hz
    
    // Process block with 1 Hz LFO
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);
    
    // Check that output buffer has non-zero values
    float sum = 0.0f;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        sum += std::abs(buffer.getSample(0, i));
    }
    EXPECT_GT(sum, 0.0001f);
    
    // Store the output for comparison
    juce::AudioBuffer<float> slowLfoBuffer;
    slowLfoBuffer.makeCopyOf(buffer);
    
    // Set LFO speed to 5 Hz
    apvts->getParameter(ParameterIds::lfoSpeed)->setValueNotifyingHost(
        apvts->getParameter(ParameterIds::lfoSpeed)->convertTo0to1(5.0f)); // 5.0 Hz
    
    // Process block with 5 Hz LFO
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);
    
    // Compare the outputs - they should be different
    bool isDifferent = false;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        if (std::abs(buffer.getSample(0, i) - slowLfoBuffer.getSample(0, i)) > 0.0001f)
        {
            isDifferent = true;
            break;
        }
    }
    
    EXPECT_TRUE(isDifferent) << "Different LFO speeds should produce different outputs";
    
    // Check that the LFO output is approximately a triangle wave
    // Triangle wave characteristics:
    // 1. Monotonic sections (increasing or decreasing)
    // 2. Direction changes at peaks and troughs
    // 3. Roughly linear slopes between direction changes
    
    // Count direction changes (peaks and troughs)
    int directionChanges = 0;
    bool wasIncreasing = false;
    bool directionEstablished = false;
    
    for (int i = 1; i < slowLfoBuffer.getNumSamples() - 1; ++i)
    {
        float prev = slowLfoBuffer.getSample(0, i - 1);
        float curr = slowLfoBuffer.getSample(0, i);
        float next = slowLfoBuffer.getSample(0, i + 1);
        
        bool isIncreasing = (next > curr) && (curr > prev);
        bool isDecreasing = (next < curr) && (curr < prev);
        
        if (isIncreasing || isDecreasing)
        {
            if (!directionEstablished)
            {
                wasIncreasing = isIncreasing;
                directionEstablished = true;
            }
            else if (wasIncreasing != isIncreasing)
            {
                directionChanges++;
                wasIncreasing = isIncreasing;
            }
        }
    }
    
    // For a 1 Hz triangle wave over 512 samples at 44100 Hz sample rate,
    // we expect at least one direction change (peak or trough)
    EXPECT_GE(directionChanges, 1) << "Triangle wave should have direction changes (peaks/troughs)";
    
    // Also verify that there is significant variation in the output
    float minVal = slowLfoBuffer.getSample(0, 0);
    float maxVal = slowLfoBuffer.getSample(0, 0);
    
    for (int i = 0; i < slowLfoBuffer.getNumSamples(); ++i)
    {
        float sample = slowLfoBuffer.getSample(0, i);
        minVal = std::min(minVal, sample);
        maxVal = std::max(maxVal, sample);
    }
    
    float range = maxVal - minVal;
    EXPECT_GT(range, 0.1f) << "LFO should produce significant amplitude variation";
}
