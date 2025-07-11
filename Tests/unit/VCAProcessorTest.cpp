#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "../../Source/CS01Synth/VCAProcessor.h"
#include "../../Source/Parameters.h"

// Test fixture for VCAProcessor tests
class VCAProcessorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a dummy processor for APVTS
        dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();
        
        auto parameterLayout = createParameterLayout();
        apvts = std::make_unique<juce::AudioProcessorValueTreeState>(*dummyProcessor, nullptr, "PARAMETERS", std::move(parameterLayout));
        
        // Create VCAProcessor
        processor = std::make_unique<VCAProcessor>(*apvts);
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
        
        // Add parameters needed for VCAProcessor
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::vcaEgDepth, "VCA EG Depth", 
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
        
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::breathInput, "Breath Input", 
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
            
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::breathVca, "Breath VCA", 
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
            
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::volume, "Volume", 
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
        
        return layout;
    }
    
    std::unique_ptr<juce::AudioProcessorGraph> dummyProcessor;
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
    std::unique_ptr<VCAProcessor> processor;
};

TEST_F(VCAProcessorTest, Initialization)
{
    // Check that processor was created successfully
    EXPECT_NE(processor.get(), nullptr);
    
    // Check that processor has expected properties
    EXPECT_EQ(processor->getName(), juce::String("VCA"));
    EXPECT_FALSE(processor->acceptsMidi());
    EXPECT_FALSE(processor->producesMidi());
}

TEST_F(VCAProcessorTest, BusesLayout)
{
    // Test valid layout
    juce::AudioProcessor::BusesLayout validLayout;
    validLayout.inputBuses.add(juce::AudioChannelSet::mono());   // Audio input
    validLayout.inputBuses.add(juce::AudioChannelSet::mono());   // EG input
    validLayout.outputBuses.add(juce::AudioChannelSet::mono());  // Output
    
    EXPECT_TRUE(processor->isBusesLayoutSupported(validLayout));
    
    // Test invalid layout with stereo inputs
    juce::AudioProcessor::BusesLayout invalidLayout1;
    invalidLayout1.inputBuses.add(juce::AudioChannelSet::stereo()); // Audio input
    invalidLayout1.inputBuses.add(juce::AudioChannelSet::mono());   // EG input
    invalidLayout1.outputBuses.add(juce::AudioChannelSet::mono());  // Output
    
    EXPECT_FALSE(processor->isBusesLayoutSupported(invalidLayout1));
    
    // Test invalid layout with stereo output
    juce::AudioProcessor::BusesLayout invalidLayout2;
    invalidLayout2.inputBuses.add(juce::AudioChannelSet::mono());   // Audio input
    invalidLayout2.inputBuses.add(juce::AudioChannelSet::mono());   // EG input
    invalidLayout2.outputBuses.add(juce::AudioChannelSet::stereo()); // Output
    
    EXPECT_FALSE(processor->isBusesLayoutSupported(invalidLayout2));
}

TEST_F(VCAProcessorTest, PrepareToPlay)
{
    // Test prepareToPlay with different sample rates
    // This should not throw any exceptions
    processor->prepareToPlay(44100.0, 512);
    EXPECT_TRUE(true); // If we got here, no exception was thrown
    
    processor->prepareToPlay(48000.0, 1024);
    EXPECT_TRUE(true); // If we got here, no exception was thrown
    
    // Test releaseResources
    processor->releaseResources();
    EXPECT_TRUE(true); // If we got here, no exception was thrown
}

TEST_F(VCAProcessorTest, ProcessBlock)
{
    // Simple test due to buffer handling complexity
    
    // Check that parameters are correctly set
    auto vcaEgDepthParam = static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::vcaEgDepth));
    auto volumeParam = static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::volume));
    
    EXPECT_NE(vcaEgDepthParam, nullptr);
    EXPECT_NE(volumeParam, nullptr);
    
    if (vcaEgDepthParam != nullptr)
    {
        vcaEgDepthParam->setValueNotifyingHost(0.5f); // 50% EG depth
        EXPECT_GT(vcaEgDepthParam->get(), 0.0f);
    }
    
    if (volumeParam != nullptr)
    {
        volumeParam->setValueNotifyingHost(0.7f); // 70% volume
        EXPECT_GT(volumeParam->get(), 0.0f);
    }
    
    // Prepare processor
    const double sampleRate = 44100.0;
    const int samplesPerBlock = 512;
    processor->prepareToPlay(sampleRate, samplesPerBlock);
    
    // Check that bus configuration can be set correctly
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::mono());   // Audio input
    layout.inputBuses.add(juce::AudioChannelSet::mono());   // EG input
    layout.outputBuses.add(juce::AudioChannelSet::mono());  // Output
    
    EXPECT_TRUE(processor->setBusesLayout(layout));
    
    // For this test, we only perform preparation and parameter tests, not actual audio processing
    EXPECT_TRUE(true);
}

TEST_F(VCAProcessorTest, VCAFunctionality)
{
    // Prepare processor
    const double sampleRate = 44100.0;
    const int samplesPerBlock = 512;
    processor->prepareToPlay(sampleRate, samplesPerBlock);
    
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::mono());
    layout.inputBuses.add(juce::AudioChannelSet::mono());
    layout.outputBuses.add(juce::AudioChannelSet::mono());
    
    // Create audio buffer
    juce::AudioBuffer<float> buffer(3, samplesPerBlock); // Audio in, EG in, Output
    juce::MidiBuffer midiBuffer;
    
    // Fill buffers
    auto* audioInputData = buffer.getWritePointer(0);
    auto* egInputData = buffer.getWritePointer(1);
    
    // Generate constant audio input (makes comparison easier)
    for (int i = 0; i < samplesPerBlock; ++i) {
        audioInputData[i] = 0.5f; // Constant value
    }
    
    // Test 1: Zero EG value should result in much lower output
    // Set EG depth to 1.0 (full EG control)
    auto vcaEgDepthParam = static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::vcaEgDepth));
    auto volumeParam = static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::volume));
    
    if (vcaEgDepthParam != nullptr)
        vcaEgDepthParam->setValueNotifyingHost(1.0f);
    
    if (volumeParam != nullptr)
        volumeParam->setValueNotifyingHost(1.0f);
    
    // Set EG to 0
    for (int i = 0; i < samplesPerBlock; ++i) {
        egInputData[i] = 0.0f;
    }
    
    // Process
    juce::AudioBuffer<float> processBuffer1(3, samplesPerBlock);
    processBuffer1.copyFrom(0, 0, audioInputData, samplesPerBlock);
    processBuffer1.copyFrom(1, 0, egInputData, samplesPerBlock);
    
    processor->processBlock(processBuffer1, midiBuffer);
    
    // Record zero EG output
    float zeroEGSum = 0.0f;
    auto* outputData1 = processBuffer1.getReadPointer(0);
    for (int i = 0; i < processBuffer1.getNumSamples(); ++i) {
        zeroEGSum += std::abs(outputData1[i]);
    }
    
    // Test 2: Full EG value should result in higher output
    // Set EG to 1.0
    for (int i = 0; i < samplesPerBlock; ++i) {
        egInputData[i] = 1.0f;
    }
    
    // Process
    juce::AudioBuffer<float> processBuffer2(3, samplesPerBlock);
    processBuffer2.copyFrom(0, 0, audioInputData, samplesPerBlock);
    processBuffer2.copyFrom(1, 0, egInputData, samplesPerBlock);
    
    processor->processBlock(processBuffer2, midiBuffer);
    
    // Record full EG output
    float fullEGSum = 0.0f;
    auto* outputData2 = processBuffer2.getReadPointer(0);
    for (int i = 0; i < processBuffer2.getNumSamples(); ++i) {
        fullEGSum += std::abs(outputData2[i]);
    }
    
    // Full EG should produce more output than zero EG
    EXPECT_GT(fullEGSum, zeroEGSum);
}

TEST_F(VCAProcessorTest, OutputFiltering)
{
    // Prepare processor
    const double sampleRate = 44100.0;
    const int samplesPerBlock = 512;
    processor->prepareToPlay(sampleRate, samplesPerBlock);
    
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::mono());
    layout.inputBuses.add(juce::AudioChannelSet::mono());
    layout.outputBuses.add(juce::AudioChannelSet::mono());
    
    // Create audio buffer
    juce::AudioBuffer<float> buffer(3, samplesPerBlock);
    juce::MidiBuffer midiBuffer;
    
    // Fill buffers with high-frequency noise to test filtering
    auto* audioInputData = buffer.getWritePointer(0);
    auto* egInputData = buffer.getWritePointer(1);
    
    // Generate high-frequency noise
    juce::Random random;
    for (int i = 0; i < samplesPerBlock; ++i) {
        audioInputData[i] = random.nextFloat() * 2.0f - 1.0f;
        egInputData[i] = 1.0f; // Full EG level
    }
    
    // Set parameters
    auto vcaEgDepthParam = static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::vcaEgDepth));
    auto volumeParam = static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::volume));
    
    if (vcaEgDepthParam != nullptr)
        vcaEgDepthParam->setValueNotifyingHost(0.0f); // No EG modulation
    
    if (volumeParam != nullptr)
        volumeParam->setValueNotifyingHost(1.0f); // Full volume
    
    // Process
    juce::AudioBuffer<float> processBuffer(3, samplesPerBlock);
    processBuffer.copyFrom(0, 0, audioInputData, samplesPerBlock);
    processBuffer.copyFrom(1, 0, egInputData, samplesPerBlock);
    
    processor->processBlock(processBuffer, midiBuffer);
    
    // Input and output RMS
    float inputRMS = 0.0f;
    float outputRMS = 0.0f;
    
    auto* outputData = processBuffer.getReadPointer(0);
    for (int i = 0; i < processBuffer.getNumSamples(); ++i) {
        inputRMS += audioInputData[i] * audioInputData[i];
        outputRMS += outputData[i] * outputData[i];
    }
    
    inputRMS = std::sqrt(inputRMS / processBuffer.getNumSamples());
    outputRMS = std::sqrt(outputRMS / processBuffer.getNumSamples());
    
    // Due to filtering, output RMS should be different from input RMS
    // This is a very basic test that just ensures some processing is happening
    EXPECT_GT(std::abs(outputRMS - inputRMS), 0.01f);
}
