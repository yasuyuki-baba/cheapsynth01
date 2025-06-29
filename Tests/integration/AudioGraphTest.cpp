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
        testProcessorGraph();
        testMidiProcessing();
    }
    
private:
    void testProcessorCreation()
    {
        beginTest("Processor Creation Test");
        
        // Smart pointers to prevent memory leaks
        std::unique_ptr<juce::AudioProcessor> processorToDelete;
        
        // Create processor
        processorToDelete.reset(new CS01AudioProcessor());
        CS01AudioProcessor* processor = static_cast<CS01AudioProcessor*>(processorToDelete.get());
        
        // Check that processor was created successfully
        expect(processor != nullptr);
        
        // Check that processor has expected properties
        expectEquals(processor->getName(), juce::String("Cheap Synth 01"));
        expect(processor->acceptsMidi());
        expect(!processor->producesMidi());
        expect(processor->isMidiEffect() == false);
    }
    
    void testProcessorGraph()
    {
        beginTest("Processor Graph Test");
        
        // Smart pointers to prevent memory leaks
        std::unique_ptr<juce::AudioProcessor> processorToDelete;
        
        // Create processor
        processorToDelete.reset(new CS01AudioProcessor());
        CS01AudioProcessor* processor = static_cast<CS01AudioProcessor*>(processorToDelete.get());
        
        // Prepare processor
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        
        processor->setBusesLayout(layout);
        
        double sampleRate = 44100.0;
        int blockSize = 512;
        processor->prepareToPlay(sampleRate, blockSize);
        
        // Create buffers
        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midiBuffer;
        
        // Process block and check that it doesn't crash
        processor->processBlock(buffer, midiBuffer);
        
        // Check that output is not all zeros (some noise should be generated)
        float sum = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                sum += std::abs(buffer.getSample(channel, sample));
            }
        }
        
        // With no MIDI input, output should be close to zero
        expectLessThan(sum, 0.1f);
    }
    
    void testMidiProcessing()
    {
        beginTest("MIDI Processing Test");
        
        // Smart pointers to prevent memory leaks
        std::unique_ptr<juce::AudioProcessor> processorToDelete;
        
        // Create processor
        processorToDelete.reset(new CS01AudioProcessor());
        CS01AudioProcessor* processor = static_cast<CS01AudioProcessor*>(processorToDelete.get());
        
        // Prepare processor
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        
        processor->setBusesLayout(layout);
        
        double sampleRate = 44100.0;
        int blockSize = 512;
        processor->prepareToPlay(sampleRate, blockSize);
        
        // Create buffers
        juce::AudioBuffer<float> buffer(2, blockSize);
        buffer.clear();
        
        // Create MIDI buffer with note on message
        juce::MidiBuffer midiBuffer;
        juce::MidiMessage noteOn = juce::MidiMessage::noteOn(1, 60, 1.0f);
        midiBuffer.addEvent(noteOn, 0);
        
        // Process block with note on
        processor->processBlock(buffer, midiBuffer);
        
        // Check that output contains audio (note is playing)
        float sumWithNote = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                sumWithNote += std::abs(buffer.getSample(channel, sample));
            }
        }
        
        // With note on, output should contain audio
        expectGreaterThan(sumWithNote, 0.1f);
        
        // Create MIDI buffer with note off message
        buffer.clear();
        midiBuffer.clear();
        juce::MidiMessage noteOff = juce::MidiMessage::noteOff(1, 60);
        midiBuffer.addEvent(noteOff, 0);
        
        // Process block with note off
        processor->processBlock(buffer, midiBuffer);
        
        // Process a few more blocks to allow note to fully release
        for (int i = 0; i < 10; ++i)
        {
            midiBuffer.clear();
            processor->processBlock(buffer, midiBuffer);
        }
        
        // Check that output is silent (note is off)
        float sumWithNoteOff = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                sumWithNoteOff += std::abs(buffer.getSample(channel, sample));
            }
        }
        
        // With note off, output should be close to zero
        expectLessThan(sumWithNoteOff, 0.1f);
    }
};

static AudioGraphTest audioGraphTest;
