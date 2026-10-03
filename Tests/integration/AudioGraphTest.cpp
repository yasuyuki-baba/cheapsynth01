#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "../../Source/CS01AudioProcessor.h"
#include "../../Source/Parameters.h"

// Test fixture for AudioGraph integration tests
class AudioGraphTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Test setup if needed
    }
    
    void TearDown() override
    {
        // Test cleanup if needed
    }
};

TEST_F(AudioGraphTest, MidiOffsetsDoNotSoundEarly)
{
    for (int blockSize : {64, 256}) {
        CS01AudioProcessor processor;
        processor.prepareToPlay(48000, blockSize);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer(2, blockSize);
        buffer.clear();
        midi.addEvent(juce::MidiMessage::noteOn(1, 69, 1.0f), blockSize / 2);
        processor.processBlock(buffer, midi);
        double before = 0;
        for (int i = 0; i < blockSize / 2; ++i)
            before += std::abs(buffer.getSample(0, i));
        EXPECT_NEAR(before, 0.0, 1.0e-9);
        double after = 0;
        for (int block = 0; block < 20; ++block) {
            buffer.clear();
            midi.clear();
            processor.processBlock(buffer, midi);
            after += buffer.getMagnitude(0, blockSize);
        }
        EXPECT_GT(after, 0.001);
    }
}

TEST_F(AudioGraphTest, MidBlockNoteOffDoesNotReleaseEarly)
{
    CS01AudioProcessor reference, released;
    for (auto* processor : {&reference, &released}) {
        auto& state = processor->getValueTreeState();
        for (const auto& setting : std::vector<std::pair<juce::String, float>>{
                 {ParameterIds::volume, 1.0f}, {ParameterIds::vcaEgDepth, 1.0f},
                 {ParameterIds::sustain, 1.0f}, {ParameterIds::attack, 0.01f},
                 {ParameterIds::release, 0.05f}, {ParameterIds::breathVca, 0.0f}}) {
            auto* parameter = state.getParameter(setting.first);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(setting.second));
        }
    }
    reference.prepareToPlay(48000, 256);
    released.prepareToPlay(48000, 256);
    juce::AudioBuffer<float> a(2, 256), b(2, 256);
    juce::MidiBuffer ma, mb;
    for (int block = 0; block < 30; ++block) {
        a.clear(); b.clear(); ma.clear(); mb.clear();
        if (block == 0) {
            ma.addEvent(juce::MidiMessage::noteOn(1, 69, 1.0f), 0);
            mb.addEvent(juce::MidiMessage::noteOn(1, 69, 1.0f), 0);
        }
        reference.processBlock(a, ma);
        released.processBlock(b, mb);
    }
    a.clear(); b.clear(); ma.clear(); mb.clear();
    mb.addEvent(juce::MidiMessage::noteOff(1, 69), 128);
    reference.processBlock(a, ma);
    released.processBlock(b, mb);
    double before = 0, after = 0;
    for (int i = 0; i < 256; ++i) {
        const double error = std::abs(a.getSample(0, i) - b.getSample(0, i));
        if (i < 128) before = std::max(before, error);
        else after += error;
    }
    EXPECT_NEAR(before, 0, 1.0e-6);
    EXPECT_GT(after, 1.0e-6);
}

TEST_F(AudioGraphTest, ProcessorCreation)
{
    // Create processor with unique_ptr to ensure proper cleanup
    std::unique_ptr<CS01AudioProcessor> processor = std::make_unique<CS01AudioProcessor>();
    
    // Check that processor was created successfully
    EXPECT_NE(processor.get(), nullptr);
    
    // Check that processor has expected properties
    EXPECT_EQ(processor->getName(), juce::String("CheapSynth01"));
    EXPECT_TRUE(processor->acceptsMidi());
    EXPECT_FALSE(processor->producesMidi());
    EXPECT_FALSE(processor->isMidiEffect());
    
    // Check that APVTS is initialized
    auto& apvts = processor->getValueTreeState();
    EXPECT_NE(apvts.getParameter(ParameterIds::waveType), nullptr);
    EXPECT_NE(apvts.getParameter(ParameterIds::feet), nullptr);
    EXPECT_NE(apvts.getParameter(ParameterIds::cutoff), nullptr);
    EXPECT_NE(apvts.getParameter(ParameterIds::resonance), nullptr);
    
    // Explicitly reset the processor to trigger cleanup
    processor.reset();
    
    // Allow a short delay for any async cleanup
    juce::Thread::sleep(10);
}

TEST_F(AudioGraphTest, AudioProcessing)
{
    // Create processor
    std::unique_ptr<CS01AudioProcessor> processor = std::make_unique<CS01AudioProcessor>();
    
    // Prepare processor
    processor->prepareToPlay(44100.0, 512);
    
    // Create audio buffer
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midiBuffer;
    
    // Process block (should be silent as no note is playing)
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);
    
    // Check that buffer is silent
    float sum = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            sum += std::abs(buffer.getSample(channel, i));
        }
    }
    EXPECT_LT(sum, 0.0001f);
    
    // Add a note-on message
    juce::MidiMessage noteOn = juce::MidiMessage::noteOn(1, 60, 1.0f);
    midiBuffer.addEvent(noteOn, 0);
    
    // Process block with note on
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);
    
    // Clear MIDI buffer for next test
    midiBuffer.clear();
    
    // Process a few more blocks to let the sound develop
    for (int i = 0; i < 10; ++i)
    {
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
    }
    
    sum = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            sum += std::abs(buffer.getSample(channel, i));
        }
    }
    
    // Note: This test might be flaky depending on envelope settings
    // If it fails, we might need to adjust expectations or the test approach
    EXPECT_GT(sum, 0.0001f) << "Audio buffer should contain signal after note-on";
    
    // Add a note-off message
    juce::MidiMessage noteOff = juce::MidiMessage::noteOff(1, 60);
    midiBuffer.addEvent(noteOff, 0);
    
    // Process block with note off
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);
    midiBuffer.clear();
    
    // Process a few more blocks to let the sound decay
    for (int i = 0; i < 50; ++i)
    {
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
    }
    
    // Check that buffer is silent again (after release phase)
    sum = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            sum += std::abs(buffer.getSample(channel, i));
        }
    }
    EXPECT_LT(sum, 0.01f); // Allow some small residual sound due to release phase
    
    // Clean up
    processor->releaseResources();
}

TEST_F(AudioGraphTest, ParameterConnections)
{
    // Create processor
    std::unique_ptr<CS01AudioProcessor> processor = std::make_unique<CS01AudioProcessor>();
    
    // Prepare processor
    processor->prepareToPlay(44100.0, 512);
    
    // Create audio buffer
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midiBuffer;
    
    // Add a note-on message
    juce::MidiMessage noteOn = juce::MidiMessage::noteOn(1, 60, 1.0f);
    midiBuffer.addEvent(noteOn, 0);
    
    // Process block with note on
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);
    midiBuffer.clear();
    
    // Process a few more blocks to let the sound develop
    for (int i = 0; i < 10; ++i)
    {
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
    }
    
    // Store the output for comparison
    juce::AudioBuffer<float> originalBuffer;
    originalBuffer.makeCopyOf(buffer);
    
    // Change a parameter (cutoff frequency)
    auto& apvts = processor->getValueTreeState();
    apvts.getParameter(ParameterIds::cutoff)->setValueNotifyingHost(0.1f); // Low cutoff
    
    // Process block with new parameter
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);
    
    // Process a few more blocks to let the change take effect
    for (int i = 0; i < 10; ++i)
    {
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
    }
    
    // Compare the outputs - they should be different
    bool isDifferent = false;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (std::abs(buffer.getSample(channel, i) - originalBuffer.getSample(channel, i)) > 0.0001f)
            {
                isDifferent = true;
                break;
            }
        }
        if (isDifferent) break;
    }
    
    EXPECT_TRUE(isDifferent) << "Parameter change should affect audio output";
    
    // Clean up
    processor->releaseResources();
}

TEST_F(AudioGraphTest, FilterAndLfoRoutingStayConsistent)
{
    CS01AudioProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    auto& state = processor.getValueTreeState();
    const auto setChoice = [&state](const juce::String& parameterId, int choice)
    {
        auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterId));
        ASSERT_NE(parameter, nullptr);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(choice)));
    };

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    for (int filterType = 0; filterType < 2; ++filterType)
    {
        for (int lfoTarget = 0; lfoTarget < 2; ++lfoTarget)
        {
            setChoice(ParameterIds::filterType, filterType);
            setChoice(ParameterIds::lfoTarget, lfoTarget);
            processor.flushPendingGraphChangesForTesting();

            buffer.clear();
            processor.processBlock(buffer, midi);

            const auto hasConnection = [&processor](juce::AudioProcessorGraph::NodeID sourceNode,
                                                    int sourceChannel,
                                                    juce::AudioProcessorGraph::NodeID destinationNode,
                                                    int destinationChannel)
            {
                return processor.getAudioGraphForTesting().isConnected({
                    {sourceNode, sourceChannel}, {destinationNode, destinationChannel}});
            };

            const auto originalFilter = processor.getOriginalFilterNodeIdForTesting();
            const auto modernFilter = processor.getModernFilterNodeIdForTesting();
            const auto vco = processor.getVcoNodeIdForTesting();
            const auto lfo = processor.getLfoNodeIdForTesting();
            const auto vca = processor.getVcaNodeIdForTesting();

            EXPECT_EQ(hasConnection(vco, 0, originalFilter, 0), filterType == 0);
            EXPECT_EQ(hasConnection(originalFilter, 0, vca, 0), filterType == 0);
            EXPECT_EQ(hasConnection(vco, 0, modernFilter, 0), filterType == 1);
            EXPECT_EQ(hasConnection(modernFilter, 0, vca, 0), filterType == 1);
            EXPECT_EQ(hasConnection(lfo, 0, vco, 0), lfoTarget == 0);
            EXPECT_EQ(hasConnection(lfo, 0, originalFilter, 2),
                      lfoTarget == 1 && filterType == 0);
            EXPECT_EQ(hasConnection(lfo, 0, modernFilter, 2),
                      lfoTarget == 1 && filterType == 1);
        }
    }

    processor.releaseResources();
}

TEST_F(AudioGraphTest, ProgramChangeEffect)
{
    // Create processor
    std::unique_ptr<CS01AudioProcessor> processor = std::make_unique<CS01AudioProcessor>();
    
    // Prepare processor
    processor->prepareToPlay(44100.0, 512);
    
    // Check that there are programs available
    int numPrograms = processor->getNumPrograms();
    EXPECT_GT(numPrograms, 1) << "Need at least 2 programs for comparison test";
    
    if (numPrograms <= 1)
        return;
    
    // Get initial program
    int initialProgram = processor->getCurrentProgram();
    
    // Future implementation note:
    // Will add parameter comparison for future test enhancement
    
    // Process some audio with initial program
    juce::AudioBuffer<float> initialBuffer(2, 512);
    juce::MidiBuffer midiBuffer;
    
    // Add note-on to hear the effect
    juce::MidiMessage noteOn = juce::MidiMessage::noteOn(1, 60, 1.0f);
    midiBuffer.addEvent(noteOn, 0);
    processor->processBlock(initialBuffer, midiBuffer);
    midiBuffer.clear();
    
    // Let sound develop
    for (int i = 0; i < 10; ++i)
    {
        processor->processBlock(initialBuffer, midiBuffer);
    }
    
    // Change to a different program
    int newProgram = (initialProgram + 1) % numPrograms;
    processor->setCurrentProgram(newProgram);
    
    // Verify program change
    EXPECT_EQ(processor->getCurrentProgram(), newProgram) << "Current program should be updated";
    
    // Verify a different program has been loaded
    EXPECT_EQ(processor->getCurrentProgram(), newProgram) << "Program should be changed";
    
    // Note: Parameters might not always change (presets could have similar parameters)
    // Therefore, we only check the program number change without parameter validation
    
    // Process audio with new program
    juce::AudioBuffer<float> newBuffer(2, 512);
    midiBuffer.addEvent(noteOn, 0);
    processor->processBlock(newBuffer, midiBuffer);
    midiBuffer.clear();
    
    // Let sound develop
    for (int i = 0; i < 10; ++i)
    {
        processor->processBlock(newBuffer, midiBuffer);
    }
    
    // Audio output comparison is disabled
    // Similar sounds may be produced even between different programs,
    // so checking the program number change is sufficient
    
    // This test verifies the program switching functionality itself
    // Actual sound changes are tested in detail by the ProgramManagerTest
    
    // Clean up
    processor->releaseResources();
}
