#pragma once

#include <JuceHeader.h>
#include "../../Source/CS01Synth/VCOProcessor.h"
#include "../../Source/Parameters.h"
#include "../mocks/MockToneGenerator.h"

class VCOProcessorTest : public juce::UnitTest
{
public:
    VCOProcessorTest() : juce::UnitTest("VCOProcessor Tests") {}
    
    void runTest() override
    {
        testInitialization();
        testProcessBlock();
        testNoteHandling();
        testWaveformChange();
        testOctaveChange();
        testPitchBend();
    }
    
private:
    // Create a parameter layout for testing
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        
        // Add parameters needed for VCOProcessor
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            ParameterIds::waveType, "Wave Type", 
            juce::StringArray("Triangle", "Sawtooth", "Square", "Pulse", "PWM"), 0));
        
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            ParameterIds::feet, "Feet", 
            juce::StringArray("32'", "16'", "8'", "4'", "Noise"), 2));
        
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::modDepth, "Mod Depth", 
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        
        layout.add(std::make_unique<juce::AudioParameterBool>(
            ParameterIds::glissando, "Glissando", false));
            
        // Add parameters required by ToneGenerator
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::release, "Release", 
            juce::NormalisableRange<float>(0.01f, 5.0f), 0.1f));
            
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::pitchBendUpRange, "Pitch Bend Up Range", 
            juce::NormalisableRange<float>(0.0f, 12.0f), 2.0f));
            
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::pitchBendDownRange, "Pitch Bend Down Range", 
            juce::NormalisableRange<float>(0.0f, 12.0f), 2.0f));
            
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::pitch, "Pitch", 
            juce::NormalisableRange<float>(-12.0f, 12.0f), 0.0f));
            
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::pwmSpeed, "PWM Speed", 
            juce::NormalisableRange<float>(0.1f, 10.0f), 1.0f));
        
        // Add parameters for MockToneGenerator
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            "vco_waveform", "VCO Waveform", 
            juce::NormalisableRange<float>(0.0f, 4.0f), 0.0f));
        
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            "vco_octave", "VCO Octave", 
            juce::NormalisableRange<float>(-2.0f, 2.0f), 0.0f));
        
        return layout;
    }
    
    void testInitialization()
    {
        beginTest("Initialization Test");
        
        // Create a dummy processor for APVTS
        std::unique_ptr<juce::AudioProcessor> dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();
        
        auto parameterLayout = createParameterLayout();
        juce::UndoManager undoManager;
        juce::AudioProcessorValueTreeState apvts(*dummyProcessor, &undoManager, "PARAMETERS", std::move(parameterLayout));
        
        // Create VCOProcessor
        std::unique_ptr<VCOProcessor> processor = std::make_unique<VCOProcessor>(apvts);
        
        // Check that processor was created successfully
        expect(processor != nullptr);
        
        // Check that processor has expected properties
        expectEquals(processor->getName(), juce::String("VCOProcessor"));
        expect(!processor->acceptsMidi());
        expect(!processor->producesMidi());
    }
    
    void testProcessBlock()
    {
        beginTest("Process Block Test");
        
        // Test is now enabled
        // Removed: // Skip this test as it's still causing segmentation faults
        // return;
        
        juce::AudioProcessorGraph dummyProcessor;
        
        auto parameterLayout = createParameterLayout();
        juce::UndoManager undoManager;
        juce::AudioProcessorValueTreeState apvts(dummyProcessor, &undoManager, "PARAMETERS", std::move(parameterLayout));
        
        // Set initial parameter values
        auto waveTypeParam = static_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::waveType));
        auto feetParam = static_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::feet));
        auto modDepthParam = static_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParameterIds::modDepth));
        auto glissandoParam = static_cast<juce::AudioParameterBool*>(apvts.getParameter(ParameterIds::glissando));
        
        if (waveTypeParam != nullptr)
            waveTypeParam->setValueNotifyingHost(waveTypeParam->convertTo0to1(1)); // Sawtooth
        
        if (feetParam != nullptr)
            feetParam->setValueNotifyingHost(feetParam->convertTo0to1(2)); // 8'
        
        if (modDepthParam != nullptr)
            modDepthParam->setValueNotifyingHost(0.0f); // No modulation
        
        if (glissandoParam != nullptr)
            glissandoParam->setValueNotifyingHost(false); // No glissando
        
        // Create VCOProcessor
        std::unique_ptr<VCOProcessor> processor = std::make_unique<VCOProcessor>(apvts);
        
        // Prepare processor
        const double sampleRate = 44100.0;
        const int samplesPerBlock = 512;
        processor->prepareToPlay(sampleRate, samplesPerBlock);
        
        // Get note handler (ToneGenerator)
        INoteHandler* noteHandler = processor->getNoteHandler();
        expect(noteHandler != nullptr);
        
        // Start a note
        noteHandler->startNote(60, 1.0f, 8192);
        
        // Create audio buffer for processing with more defensive channel handling
        const int numChannels = processor->getTotalNumOutputChannels();
        DBG("VCOProcessor has " + juce::String(numChannels) + " output channels");
        
        // Always ensure at least one channel and handle gracefully
        const int safeNumChannels = std::max(1, numChannels);
        juce::AudioBuffer<float> buffer(safeNumChannels, samplesPerBlock);
        buffer.clear(); // Ensure buffer is initialized with zeros
        juce::MidiBuffer midiBuffer;
        
        try {
            // Process block with additional safeguards
            processor->processBlock(buffer, midiBuffer);
        } catch (const std::exception& e) {
            // Log any exceptions rather than crashing
            DBG("Exception in processBlock: " + juce::String(e.what()));
            expect(false); // Mark test as failed but don't crash
            return;
        }
        
        // Check that output buffer has non-zero values (sound is generated)
        float sum = 0.0f;
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            sum += std::abs(buffer.getSample(0, sample)); // Only one output channel
        }
        
        expectGreaterThan(sum, 0.0001f);
        
        // Stop the note
        noteHandler->stopNote(false);
        
        // Clear buffer
        buffer.clear();
        
        // Process block again
        processor->processBlock(buffer, midiBuffer);
        
        // After note is stopped, buffer may still have some sound due to release time
        // Just make sure we don't have a crash
        expect(true);
    }
    
    void testNoteHandling()
    {
        beginTest("Note Handling Test");
        std::unique_ptr<juce::AudioProcessor> dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();
        
        auto parameterLayout = createParameterLayout();
        juce::UndoManager undoManager;
        juce::AudioProcessorValueTreeState apvts(*dummyProcessor, &undoManager, "PARAMETERS", std::move(parameterLayout));
        
        // Create VCOProcessor
        std::unique_ptr<VCOProcessor> processor = std::make_unique<VCOProcessor>(apvts);
        
        // Prepare processor
        const double sampleRate = 44100.0;
        const int samplesPerBlock = 512;
        processor->prepareToPlay(sampleRate, samplesPerBlock);
        
        // Get note handler (ToneGenerator)
        INoteHandler* noteHandler = processor->getNoteHandler();
        expect(noteHandler != nullptr);
        
        // Test 1: Start note
        noteHandler->startNote(60, 1.0f, 8192);
        
        // Check that note is active
        expect(noteHandler->isActive());
        expectEquals(noteHandler->getCurrentlyPlayingNote(), 60);
        
        // Test 2: Change note
        noteHandler->changeNote(64);
        
        // Check that note has changed
        expect(noteHandler->isActive());
        expectEquals(noteHandler->getCurrentlyPlayingNote(), 64);
        
        // Test 3: Stop note with tail off
        noteHandler->stopNote(true);
        
        // Check that note is still active during tail off
        expect(noteHandler->isActive());
        
        // Test 4: Stop note without tail off
        noteHandler->startNote(67, 1.0f, 8192);
        noteHandler->stopNote(false);
        
        // Check that note is no longer active
        expect(!noteHandler->isActive());
        expectEquals(noteHandler->getCurrentlyPlayingNote(), 0);
        
        // Test 5: Multiple notes in sequence
        noteHandler->startNote(60, 1.0f, 8192);
        expect(noteHandler->isActive());
        expectEquals(noteHandler->getCurrentlyPlayingNote(), 60);
        
        noteHandler->startNote(64, 1.0f, 8192);
        expect(noteHandler->isActive());
        expectEquals(noteHandler->getCurrentlyPlayingNote(), 64);
        
        noteHandler->startNote(67, 1.0f, 8192);
        expect(noteHandler->isActive());
        expectEquals(noteHandler->getCurrentlyPlayingNote(), 67);
        
        noteHandler->stopNote(false);
        expect(!noteHandler->isActive());
    }
    
    void testWaveformChange()
    {
        beginTest("Waveform Change Test");
        
        // Test is now enabled
        // Removed: // Skip this test as it's still causing segmentation faults
        // return;
        
        // Create a dummy processor for APVTS
        juce::AudioProcessorGraph dummyProcessor;
        
        auto parameterLayout = createParameterLayout();
        juce::UndoManager undoManager;
        juce::AudioProcessorValueTreeState apvts(dummyProcessor, &undoManager, "PARAMETERS", std::move(parameterLayout));
        
        // Set initial parameter values
        auto waveTypeParam = static_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::waveType));
        auto feetParam = static_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::feet));
        auto modDepthParam = static_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParameterIds::modDepth));
        auto glissandoParam = static_cast<juce::AudioParameterBool*>(apvts.getParameter(ParameterIds::glissando));
        
        if (feetParam != nullptr)
            feetParam->setValueNotifyingHost(feetParam->convertTo0to1(2)); // 8'
        
        if (modDepthParam != nullptr)
            modDepthParam->setValueNotifyingHost(0.0f); // No modulation
        
        if (glissandoParam != nullptr)
            glissandoParam->setValueNotifyingHost(false); // No glissando
        
        // Create VCOProcessor
        std::unique_ptr<VCOProcessor> processor = std::make_unique<VCOProcessor>(apvts);
        
        // Prepare processor
        const double sampleRate = 44100.0;
        const int samplesPerBlock = 512;
        processor->prepareToPlay(sampleRate, samplesPerBlock);
        
        // Get note handler (ToneGenerator)
        INoteHandler* noteHandler = processor->getNoteHandler();
        expect(noteHandler != nullptr);
        
        // Start a note
        noteHandler->startNote(60, 1.0f, 8192);
        
        // Create audio buffer for processing with defensive handling
        const int numChannels = processor->getTotalNumOutputChannels();
        DBG("VCOProcessor has " + juce::String(numChannels) + " output channels for waveform test");
        
        // Always ensure at least one channel
        const int safeNumChannels = std::max(1, numChannels);
        juce::AudioBuffer<float> buffer(safeNumChannels, samplesPerBlock);
        juce::MidiBuffer midiBuffer;
        
        // Test different waveforms
        // Triangle waveform
        if (waveTypeParam != nullptr)
            waveTypeParam->setValueNotifyingHost(waveTypeParam->convertTo0to1(0)); // Triangle
        
        buffer.clear(); // Ensure buffer is initialized with zeros
        
        try {
            processor->processBlock(buffer, midiBuffer);
        } catch (const std::exception& e) {
            DBG("Exception in testWaveformChange (triangle): " + juce::String(e.what()));
            expect(false);
            return;
        }
        
        // Store samples for triangle waveform
        std::vector<float> triangleSamples;
        triangleSamples.resize(buffer.getNumSamples());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            triangleSamples[i] = buffer.getSample(0, i); // First output channel
        }
        
        // Sawtooth waveform
        if (waveTypeParam != nullptr)
            waveTypeParam->setValueNotifyingHost(waveTypeParam->convertTo0to1(1)); // Sawtooth
        
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
        
        // Store samples for sawtooth waveform
        std::vector<float> sawtoothSamples;
        sawtoothSamples.resize(buffer.getNumSamples());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            sawtoothSamples[i] = buffer.getSample(0, i); // First output channel
        }
        
        // Square waveform
        if (waveTypeParam != nullptr)
            waveTypeParam->setValueNotifyingHost(waveTypeParam->convertTo0to1(2)); // Square
        
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
        
        // Store samples for square waveform
        std::vector<float> squareSamples;
        squareSamples.resize(buffer.getNumSamples());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            squareSamples[i] = buffer.getSample(0, i); // First output channel
        }
        
        // Compare waveforms - they should be different
        bool allWaveformsIdentical = true;
        
        // Compare Triangle vs Sawtooth
        float diffSum = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            diffSum += std::abs(triangleSamples[i] - sawtoothSamples[i]);
        }
        if (diffSum > 0.1f) allWaveformsIdentical = false;
        
        // Compare Sawtooth vs Square
        diffSum = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            diffSum += std::abs(sawtoothSamples[i] - squareSamples[i]);
        }
        if (diffSum > 0.1f) allWaveformsIdentical = false;
        
        // Check that waveforms are different
        expect(!allWaveformsIdentical);
    }
    
    void testOctaveChange()
    {
        beginTest("Octave Change Test");
        
        // Test is now enabled
        // Removed: // Skip this test as it's still causing segmentation faults
        // return;
        
        // Create a dummy processor for APVTS
        juce::AudioProcessorGraph dummyProcessor;
        
        auto parameterLayout = createParameterLayout();
        juce::UndoManager undoManager;
        juce::AudioProcessorValueTreeState apvts(dummyProcessor, &undoManager, "PARAMETERS", std::move(parameterLayout));
        
        // Set initial parameter values
        auto waveTypeParam = static_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::waveType));
        auto feetParam = static_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::feet));
        auto modDepthParam = static_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParameterIds::modDepth));
        auto glissandoParam = static_cast<juce::AudioParameterBool*>(apvts.getParameter(ParameterIds::glissando));
        
        if (waveTypeParam != nullptr)
            waveTypeParam->setValueNotifyingHost(waveTypeParam->convertTo0to1(1)); // Sawtooth
        
        if (modDepthParam != nullptr)
            modDepthParam->setValueNotifyingHost(0.0f); // No modulation
        
        if (glissandoParam != nullptr)
            glissandoParam->setValueNotifyingHost(false); // No glissando
        
        // Create VCOProcessor
        std::unique_ptr<VCOProcessor> processor = std::make_unique<VCOProcessor>(apvts);
        
        // Prepare processor
        const double sampleRate = 44100.0;
        const int samplesPerBlock = 512;
        processor->prepareToPlay(sampleRate, samplesPerBlock);
        
        // Get note handler (ToneGenerator)
        INoteHandler* noteHandler = processor->getNoteHandler();
        expect(noteHandler != nullptr);
        
        // Start a note
        noteHandler->startNote(60, 1.0f, 8192);
        
        // Create audio buffer for processing with defensive handling
        const int numChannels = processor->getTotalNumOutputChannels();
        DBG("VCOProcessor has " + juce::String(numChannels) + " output channels for octave test");
        
        // Always ensure at least one channel
        const int safeNumChannels = std::max(1, numChannels);
        juce::AudioBuffer<float> buffer(safeNumChannels, samplesPerBlock);
        buffer.clear(); // Ensure buffer is initialized with zeros
        juce::MidiBuffer midiBuffer;
        
        // Test different octave settings
        // 32' setting
        if (feetParam != nullptr)
            feetParam->setValueNotifyingHost(feetParam->convertTo0to1(0)); // 32'
        
        try {
            processor->processBlock(buffer, midiBuffer);
        } catch (const std::exception& e) {
            DBG("Exception in testOctaveChange (32'): " + juce::String(e.what()));
            expect(false);
            return;
        }
        
        // Store samples for 32' setting
        std::vector<float> feet32Samples;
        feet32Samples.resize(buffer.getNumSamples());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            feet32Samples[i] = buffer.getSample(0, i); // First output channel
        }
        
        // 16' setting
        if (feetParam != nullptr)
            feetParam->setValueNotifyingHost(feetParam->convertTo0to1(1)); // 16'
        
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
        
        // Store samples for 16' setting
        std::vector<float> feet16Samples;
        feet16Samples.resize(buffer.getNumSamples());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            feet16Samples[i] = buffer.getSample(0, i); // First output channel
        }
        
        // 8' setting
        if (feetParam != nullptr)
            feetParam->setValueNotifyingHost(feetParam->convertTo0to1(2)); // 8'
        
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
        
        // Store samples for 8' setting
        std::vector<float> feet8Samples;
        feet8Samples.resize(buffer.getNumSamples());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            feet8Samples[i] = buffer.getSample(0, i); // First output channel
        }
        
        // 4' setting
        if (feetParam != nullptr)
            feetParam->setValueNotifyingHost(feetParam->convertTo0to1(3)); // 4'
        
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
        
        // Store samples for 4' setting
        std::vector<float> feet4Samples;
        feet4Samples.resize(buffer.getNumSamples());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            feet4Samples[i] = buffer.getSample(0, i); // First output channel
        }
        
        // Compare octave settings - they should be different
        bool allOctavesIdentical = true;
        
        // Compare 32' vs 16'
        float diffSum = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            diffSum += std::abs(feet32Samples[i] - feet16Samples[i]);
        }
        if (diffSum > 0.1f) allOctavesIdentical = false;
        
        // Compare 16' vs 8'
        diffSum = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            diffSum += std::abs(feet16Samples[i] - feet8Samples[i]);
        }
        if (diffSum > 0.1f) allOctavesIdentical = false;
        
        // Compare 8' vs 4'
        diffSum = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            diffSum += std::abs(feet8Samples[i] - feet4Samples[i]);
        }
        if (diffSum > 0.1f) allOctavesIdentical = false;
        
        // Check that octave settings produce different sounds
        expect(!allOctavesIdentical);
    }
    
    void testPitchBend()
    {
        beginTest("Pitch Bend Test");
        
        // Test is now enabled
        // Removed: // Skip this test as it's still causing segmentation faults
        // return;
        
        // Create a dummy processor for APVTS
        juce::AudioProcessorGraph dummyProcessor;
        
        auto parameterLayout = createParameterLayout();
        juce::UndoManager undoManager;
        juce::AudioProcessorValueTreeState apvts(dummyProcessor, &undoManager, "PARAMETERS", std::move(parameterLayout));
        
        // Set initial parameter values
        auto waveTypeParam = static_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::waveType));
        auto feetParam = static_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::feet));
        auto modDepthParam = static_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParameterIds::modDepth));
        auto glissandoParam = static_cast<juce::AudioParameterBool*>(apvts.getParameter(ParameterIds::glissando));
        
        if (waveTypeParam != nullptr)
            waveTypeParam->setValueNotifyingHost(waveTypeParam->convertTo0to1(1)); // Sawtooth
        
        if (feetParam != nullptr)
            feetParam->setValueNotifyingHost(feetParam->convertTo0to1(2)); // 8'
        
        if (modDepthParam != nullptr)
            modDepthParam->setValueNotifyingHost(0.0f); // No modulation
        
        if (glissandoParam != nullptr)
            glissandoParam->setValueNotifyingHost(false); // No glissando
        
        // Create VCOProcessor
        std::unique_ptr<VCOProcessor> processor = std::make_unique<VCOProcessor>(apvts);
        
        // Prepare processor
        const double sampleRate = 44100.0;
        const int samplesPerBlock = 512;
        processor->prepareToPlay(sampleRate, samplesPerBlock);
        
        // Get note handler (ToneGenerator)
        INoteHandler* noteHandler = processor->getNoteHandler();
        expect(noteHandler != nullptr);
        
        // Start a note with center pitch bend
        noteHandler->startNote(60, 1.0f, 8192);
        
        // Create audio buffer for processing with defensive handling
        const int numChannels = processor->getTotalNumOutputChannels();
        DBG("VCOProcessor has " + juce::String(numChannels) + " output channels for pitch bend test");
        
        // Always use a safe approach with fixed channel count
        juce::AudioBuffer<float> buffer(1, samplesPerBlock);
        buffer.clear(); // Ensure buffer is initialized with zeros
        juce::MidiBuffer midiBuffer;
        
        // Process block with center pitch bend
        try {
            processor->processBlock(buffer, midiBuffer);
        } catch (const std::exception& e) {
            DBG("Exception in testPitchBend (center): " + juce::String(e.what()));
            expect(false);
            return;
        }
        
        // Store samples for center pitch bend
        std::vector<float> centerPitchSamples;
        centerPitchSamples.resize(buffer.getNumSamples());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            centerPitchSamples[i] = buffer.getSample(0, i); // First channel
        }
        
        // Apply pitch bend up
        noteHandler->pitchWheelMoved(16383); // Maximum pitch bend up
        
        // Clear buffer
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
        
        // Store samples for pitch bend up
        std::vector<float> pitchUpSamples;
        pitchUpSamples.resize(buffer.getNumSamples());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            pitchUpSamples[i] = buffer.getSample(0, i); // First channel
        }
        
        // Apply pitch bend down
        noteHandler->pitchWheelMoved(0); // Maximum pitch bend down
        
        // Clear buffer
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
        
        // Store samples for pitch bend down
        std::vector<float> pitchDownSamples;
        pitchDownSamples.resize(buffer.getNumSamples());
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            pitchDownSamples[i] = buffer.getSample(0, i); // First channel
        }
        
        // Compare pitch bend settings - they should be different
        bool allPitchBendsIdentical = true;
        
        // Compare center vs up
        float diffSum = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            diffSum += std::abs(centerPitchSamples[i] - pitchUpSamples[i]);
        }
        if (diffSum > 0.1f) allPitchBendsIdentical = false;
        
        // Compare center vs down
        diffSum = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            diffSum += std::abs(centerPitchSamples[i] - pitchDownSamples[i]);
        }
        if (diffSum > 0.1f) allPitchBendsIdentical = false;
        
        // Compare up vs down
        diffSum = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            diffSum += std::abs(pitchUpSamples[i] - pitchDownSamples[i]);
        }
        if (diffSum > 0.1f) allPitchBendsIdentical = false;
        
        // Check that pitch bend settings produce different sounds
        expect(!allPitchBendsIdentical);
    }
};

static VCOProcessorTest vcoProcessorTest;
