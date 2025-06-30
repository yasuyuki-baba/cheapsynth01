#pragma once

#include <JuceHeader.h>
#include "../../Source/CS01AudioProcessor.h"
#include "../../Source/Parameters.h"

class AudioGraphTest : public juce::UnitTest
{
public:
    AudioGraphTest() : juce::UnitTest("Audio Graph Integration Tests") {}
    
    void runTest() override
    {
        testProcessorCreation();
        testAudioProcessing();
        testParameterConnections();
        testPresetLoading();
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
        expectEquals(processor->getName(), juce::String("CheapSynth01"));
        expect(processor->acceptsMidi());
        expect(!processor->producesMidi());
        expect(processor->isMidiEffect() == false);
        
        // Check that APVTS is initialized
        auto& apvts = processor->getValueTreeState();
        expect(apvts.getParameter(ParameterIds::waveType) != nullptr);
        expect(apvts.getParameter(ParameterIds::feet) != nullptr);
        expect(apvts.getParameter(ParameterIds::cutoff) != nullptr);
        expect(apvts.getParameter(ParameterIds::resonance) != nullptr);
        
        // Explicitly reset the processor to trigger cleanup
        processor.reset();
        
        // Allow a short delay for any async cleanup
        juce::Thread::sleep(10);
    }
    
    void testAudioProcessing()
    {
        beginTest("Audio Processing Test");
        
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
        expectLessThan(sum, 0.0001f);
        
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
        expectGreaterThan(sum, 0.0001f);
        
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
        expectLessThan(sum, 0.01f); // Allow some small residual sound due to release phase
        
        // Clean up
        processor->releaseResources();
    }
    
    void testParameterConnections()
    {
        beginTest("Parameter Connections Test");
        
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
        
        expect(isDifferent, "Parameter change should affect audio output");
        
        // Clean up
        processor->releaseResources();
    }
    
    void testPresetLoading()
    {
        beginTest("Preset Loading Test");
        
        // Create processor
        std::unique_ptr<CS01AudioProcessor> processor = std::make_unique<CS01AudioProcessor>();
        
        // Check that there are presets available
        int numPrograms = processor->getNumPrograms();
        expectGreaterThan(numPrograms, 0, "Processor should have at least one preset");
        
        // Get initial program
        int initialProgram = processor->getCurrentProgram();
        
        // Check that initial program name is not empty
        juce::String initialProgramName = processor->getProgramName(initialProgram);
        expect(!initialProgramName.isEmpty(), "Initial program name should not be empty");
        
        // Change program (load preset)
        int newProgram = (initialProgram + 1) % numPrograms;
        processor->setCurrentProgram(newProgram);
        
        // Check that current program has changed
        expectEquals(processor->getCurrentProgram(), newProgram, "Current program should be updated");
        
        // Check that program name is not empty
        juce::String programName = processor->getProgramName(newProgram);
        expect(!programName.isEmpty(), "Program name should not be empty");
        
        // Clean up
        processor.reset();
    }
};

static AudioGraphTest audioGraphTest;
