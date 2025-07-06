#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "../../Source/CS01Synth/EGProcessor.h"
#include "../../Source/Parameters.h"

// Test fixture for EGProcessor tests
class EGProcessorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a dummy processor for APVTS
        dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();
        
        auto parameterLayout = createParameterLayout();
        apvts = std::make_unique<juce::AudioProcessorValueTreeState>(*dummyProcessor, nullptr, "PARAMETERS", std::move(parameterLayout));
        
        // Create EGProcessor
        processor = std::make_unique<EGProcessor>(*apvts);
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
        
        // Add parameters needed for EGProcessor
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::attack, "Attack", 
            juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.5f), 0.1f));
        
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::decay, "Decay", 
            juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.5f), 0.3f));
        
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::sustain, "Sustain", 
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::release, "Release", 
            juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.5f), 0.5f));
        
        return layout;
    }
    
    std::unique_ptr<juce::AudioProcessorGraph> dummyProcessor;
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
    std::unique_ptr<EGProcessor> processor;
};

TEST_F(EGProcessorTest, Initialization)
{
    // Check that processor was created successfully
    EXPECT_NE(processor.get(), nullptr);
    
    // Check that processor has expected properties
    EXPECT_EQ(processor->getName(), juce::String("EG"));
    EXPECT_FALSE(processor->acceptsMidi());
    EXPECT_FALSE(processor->producesMidi());
    EXPECT_FALSE(processor->isMidiEffect());
    
    // Check bus configuration
    EXPECT_EQ(processor->getBusCount(true), 0); // No input buses
    EXPECT_EQ(processor->getBusCount(false), 1); // 1 output bus
    
    EXPECT_EQ(processor->getBus(false, 0)->getName(), juce::String("Output"));
    
    // Check initial state
    EXPECT_FALSE(processor->isActive());
}

TEST_F(EGProcessorTest, ParameterSettings)
{
    // Test attack parameter
    auto* attackParam = apvts->getParameter(ParameterIds::attack);
    EXPECT_NE(attackParam, nullptr);
    
    // Set attack to different values
    attackParam->setValueNotifyingHost(attackParam->convertTo0to1(0.05f)); // 50ms
    EXPECT_EQ(apvts->getRawParameterValue(ParameterIds::attack)->load(), 0.05f);
    
    attackParam->setValueNotifyingHost(attackParam->convertTo0to1(0.5f)); // 500ms
    EXPECT_EQ(apvts->getRawParameterValue(ParameterIds::attack)->load(), 0.5f);
    
    // Test decay parameter
    auto* decayParam = apvts->getParameter(ParameterIds::decay);
    EXPECT_NE(decayParam, nullptr);
    
    // Set decay to different values
    decayParam->setValueNotifyingHost(decayParam->convertTo0to1(0.1f)); // 100ms
    EXPECT_EQ(apvts->getRawParameterValue(ParameterIds::decay)->load(), 0.1f);
    
    decayParam->setValueNotifyingHost(decayParam->convertTo0to1(1.0f)); // 1s
    EXPECT_EQ(apvts->getRawParameterValue(ParameterIds::decay)->load(), 1.0f);
    
    // Test sustain parameter
    auto* sustainParam = apvts->getParameter(ParameterIds::sustain);
    EXPECT_NE(sustainParam, nullptr);
    
    // Set sustain to different values
    sustainParam->setValueNotifyingHost(0.25f); // 25%
    EXPECT_EQ(apvts->getRawParameterValue(ParameterIds::sustain)->load(), 0.25f);
    
    sustainParam->setValueNotifyingHost(0.75f); // 75%
    EXPECT_EQ(apvts->getRawParameterValue(ParameterIds::sustain)->load(), 0.75f);
    
    // Test release parameter
    auto* releaseParam = apvts->getParameter(ParameterIds::release);
    EXPECT_NE(releaseParam, nullptr);
    
    // Set release to different values
    releaseParam->setValueNotifyingHost(releaseParam->convertTo0to1(0.2f)); // 200ms
    EXPECT_EQ(apvts->getRawParameterValue(ParameterIds::release)->load(), 0.2f);
    
    releaseParam->setValueNotifyingHost(releaseParam->convertTo0to1(2.0f)); // 2s
    EXPECT_EQ(apvts->getRawParameterValue(ParameterIds::release)->load(), 2.0f);
}

TEST_F(EGProcessorTest, BusesLayout)
{
    // Test supported buses layout
    juce::AudioProcessor::BusesLayout supportedLayout;
    supportedLayout.inputBuses.clear(); // No input buses
    supportedLayout.outputBuses.add(juce::AudioChannelSet::mono()); // Mono output
    
    EXPECT_TRUE(processor->isBusesLayoutSupported(supportedLayout));
    
    // Test unsupported buses layout (with input)
    juce::AudioProcessor::BusesLayout unsupportedLayout1;
    unsupportedLayout1.inputBuses.add(juce::AudioChannelSet::mono()); // Input bus (not supported)
    unsupportedLayout1.outputBuses.add(juce::AudioChannelSet::mono()); // Mono output
    
    EXPECT_FALSE(processor->isBusesLayoutSupported(unsupportedLayout1));
    
    // Test unsupported buses layout (stereo output)
    juce::AudioProcessor::BusesLayout unsupportedLayout2;
    unsupportedLayout2.inputBuses.clear(); // No input buses
    unsupportedLayout2.outputBuses.add(juce::AudioChannelSet::stereo()); // Stereo output (not supported)
    
    EXPECT_FALSE(processor->isBusesLayoutSupported(unsupportedLayout2));
}

TEST_F(EGProcessorTest, EnvelopeGeneration)
{
    // Prepare processor
    processor->prepareToPlay(44100.0, 512);
    
    // Create audio buffer
    juce::AudioBuffer<float> buffer(1, 512); // 1 channel for output
    juce::MidiBuffer midiBuffer;
    
    // Set ADSR parameters for testing
    apvts->getParameter(ParameterIds::attack)->setValueNotifyingHost(
        apvts->getParameter(ParameterIds::attack)->convertTo0to1(0.01f)); // 10ms attack
    
    apvts->getParameter(ParameterIds::decay)->setValueNotifyingHost(
        apvts->getParameter(ParameterIds::decay)->convertTo0to1(0.1f)); // 100ms decay
    
    apvts->getParameter(ParameterIds::sustain)->setValueNotifyingHost(0.5f); // 50% sustain
    
    apvts->getParameter(ParameterIds::release)->setValueNotifyingHost(
        apvts->getParameter(ParameterIds::release)->convertTo0to1(0.2f)); // 200ms release
    
    // Trigger note on
    processor->startEnvelope();
    
    // Process block (should generate attack phase)
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);
    
    // Check that envelope is active
    EXPECT_TRUE(processor->isActive());
    
    // Check that output buffer has non-zero values
    float sum = 0.0f;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        sum += std::abs(buffer.getSample(0, i));
    }
    EXPECT_GT(sum, 0.0001f);
    
    // Process more blocks to reach sustain phase
    for (int i = 0; i < 10; ++i)
    {
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
    }
    
    // Check that envelope is still active
    EXPECT_TRUE(processor->isActive());
    
    // Trigger note off
    processor->releaseEnvelope();
    
    // Process block (should generate release phase)
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);
    
    // Check that envelope is still active during release phase
    EXPECT_TRUE(processor->isActive());
    
    // Process more blocks to complete release phase
    for (int i = 0; i < 20; ++i)
    {
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
    }
    
    // Check that envelope is no longer active
    EXPECT_FALSE(processor->isActive());
}

TEST_F(EGProcessorTest, NoteOnOff)
{
    // Prepare processor
    processor->prepareToPlay(44100.0, 512);
    
    // Check initial state
    EXPECT_FALSE(processor->isActive());
    
    // Trigger note on
    processor->startEnvelope();
    
    // Check that envelope is active
    EXPECT_TRUE(processor->isActive());
    
    // Trigger note off
    processor->releaseEnvelope();
    
    // Note: We can't check that envelope is inactive immediately after noteOff
    // because the release phase takes time. In a real test, we would process
    // audio blocks until the release phase is complete.
}
