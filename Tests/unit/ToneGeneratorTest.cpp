#pragma once

#include <JuceHeader.h>
#include "../../Source/CS01Synth/ToneGenerator.h"
#include "../mocks/MockToneGenerator.h"

// Helper class to manage APVTS and processor lifecycle
class APVTSHolder
{
public:
    APVTSHolder()
    {
        // Create a dummy processor
        dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();
        
        // Create parameter layout
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        
        layout.add(std::make_unique<juce::AudioParameterFloat>("vco_waveform", "Waveform", 0.0f, 4.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>("vco_octave", "Octave", -2.0f, 2.0f, 0.0f));
        
        // Create APVTS
        apvts = std::make_unique<juce::AudioProcessorValueTreeState>(*dummyProcessor, nullptr, "Parameters", std::move(layout));
    }
    
    ~APVTSHolder() = default;
    
    juce::AudioProcessorValueTreeState& getAPVTS() { return *apvts; }
    
private:
    std::unique_ptr<juce::AudioProcessorGraph> dummyProcessor;
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
};

class ToneGeneratorTest : public juce::UnitTest
{
public:
    ToneGeneratorTest() : juce::UnitTest("ToneGenerator Tests") {}
    
    void runTest() override
    {
        testInitialization();
        testNoteOnOff();
        testPitchBend();
        testWaveformGeneration();
        testGlissando();
        testLfoModulation();
    }
    
private:
    void testInitialization()
    {
        beginTest("Initialization Test");
        
        // Create APVTS holder to manage memory
        APVTSHolder apvtsHolder;
        
        // Create mock tone generator
        testing::MockToneGenerator toneGenerator(apvtsHolder.getAPVTS());
        
        // Check initial state
        expect(!toneGenerator.isActive());
        expectEquals(toneGenerator.getCurrentlyPlayingNote(), 0);
        
        // Prepare the tone generator
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        
        toneGenerator.prepare(spec);
        
        // Check that prepare doesn't crash
        expect(true);
    }
    
    void testNoteOnOff()
    {
        beginTest("Note On/Off Test");
        
        // Create APVTS holder to manage memory
        APVTSHolder apvtsHolder;
        
        // Create mock tone generator
        testing::MockToneGenerator toneGenerator(apvtsHolder.getAPVTS());
        
        // Prepare the tone generator
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        toneGenerator.prepare(spec);
        
        // Check initial state
        expect(!toneGenerator.isActive());
        expectEquals(toneGenerator.getCurrentlyPlayingNote(), 0);
        
        // Start a note
        toneGenerator.startNote(60, 1.0f, 8192);
        
        // Check that note is active
        expect(toneGenerator.isActive());
        expectEquals(toneGenerator.getCurrentlyPlayingNote(), 60);
        
        // Get a sample and check it's non-zero
        float sample = toneGenerator.getNextSample();
        expect(std::abs(sample) > 0.0f);
        
        // Stop the note without tail off
        toneGenerator.stopNote(false);
        
        // Check that note is no longer active
        expect(!toneGenerator.isActive());
        expectEquals(toneGenerator.getCurrentlyPlayingNote(), 0);
        
        // Get a sample and check it's zero
        sample = toneGenerator.getNextSample();
        expectWithinAbsoluteError(sample, 0.0f, 0.0001f);
        
        // Start a note again
        toneGenerator.startNote(60, 1.0f, 8192);
        
        // Check that note is active
        expect(toneGenerator.isActive());
        
        // Stop the note with tail off
        toneGenerator.stopNote(true);
        
        // Check that note is still active during tail off
        expect(toneGenerator.isActive());
        
        // Get a sample and check it's non-zero (tail off)
        sample = toneGenerator.getNextSample();
        expect(std::abs(sample) > 0.0f);
    }
    
    void testPitchBend()
    {
        beginTest("Pitch Bend Test");
        
        // Create APVTS holder to manage memory
        APVTSHolder apvtsHolder;
        
        // Create mock tone generator
        testing::MockToneGenerator toneGenerator(apvtsHolder.getAPVTS());
        
        // Prepare the tone generator
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        toneGenerator.prepare(spec);
        
        // Start a note with center pitch bend
        toneGenerator.startNote(60, 1.0f, 8192);
        
        // Get samples with center pitch bend
        float sample1 = toneGenerator.getNextSample();
        float sample2 = toneGenerator.getNextSample();
        
        // Apply pitch bend up
        toneGenerator.pitchWheelMoved(16383); // Maximum pitch bend up
        
        // Get samples with pitch bend up
        float sample3 = toneGenerator.getNextSample();
        float sample4 = toneGenerator.getNextSample();
        
        // Check that the samples are different (frequency has changed)
        expect(std::abs(sample1 - sample3) > 0.0001f || std::abs(sample2 - sample4) > 0.0001f);
        
        // Apply center pitch bend
        toneGenerator.pitchWheelMoved(8192);
        
        // Get samples with center pitch bend
        float sample5 = toneGenerator.getNextSample();
        float sample6 = toneGenerator.getNextSample();
        
        // Apply pitch bend down
        toneGenerator.pitchWheelMoved(0); // Maximum pitch bend down
        
        // Get samples with pitch bend down
        float sample7 = toneGenerator.getNextSample();
        float sample8 = toneGenerator.getNextSample();
        
        // Check that the samples are different (frequency has changed)
        expect(std::abs(sample5 - sample7) > 0.0001f || std::abs(sample6 - sample8) > 0.0001f);
    }
    
    void testWaveformGeneration()
    {
        beginTest("Waveform Generation Test");
        
        // Create APVTS holder to manage memory
        APVTSHolder apvtsHolder;
        
        // Create mock tone generator
        testing::MockToneGenerator toneGenerator(apvtsHolder.getAPVTS());
        
        // Prepare the tone generator
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        toneGenerator.prepare(spec);
        
        // Start a note
        toneGenerator.startNote(60, 1.0f, 8192);
        
        // Set waveform to saw
        apvtsHolder.getAPVTS().getParameter("vco_waveform")->setValueNotifyingHost(0.0f);
        
        // Get samples with saw waveform
        float sawSample1 = toneGenerator.getNextSample();
        float sawSample2 = toneGenerator.getNextSample();
        
        // Set waveform to square
        apvtsHolder.getAPVTS().getParameter("vco_waveform")->setValueNotifyingHost(0.25f);
        
        // Get samples with square waveform
        float squareSample1 = toneGenerator.getNextSample();
        float squareSample2 = toneGenerator.getNextSample();
        
        // Check that the samples are different (waveform has changed)
        expect(std::abs(sawSample1 - squareSample1) > 0.0001f || std::abs(sawSample2 - squareSample2) > 0.0001f);
        
        // Set waveform to triangle
        apvtsHolder.getAPVTS().getParameter("vco_waveform")->setValueNotifyingHost(0.5f);
        
        // Get samples with triangle waveform
        float triangleSample1 = toneGenerator.getNextSample();
        float triangleSample2 = toneGenerator.getNextSample();
        
        // Check that the samples are different (waveform has changed)
        expect(std::abs(squareSample1 - triangleSample1) > 0.0001f || std::abs(squareSample2 - triangleSample2) > 0.0001f);
    }
    
    void testGlissando()
    {
        beginTest("Glissando Test");
        
        // Create APVTS holder to manage memory
        APVTSHolder apvtsHolder;
        
        // Create mock tone generator
        testing::MockToneGenerator toneGenerator(apvtsHolder.getAPVTS());
        
        // Prepare the tone generator
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        toneGenerator.prepare(spec);
        
        // Enable glissando
        toneGenerator.setGlissando(true);
        
        // Start a note
        toneGenerator.startNote(60, 1.0f, 8192);
        
        // Get samples with initial note
        float sample1 = toneGenerator.getNextSample();
        float sample2 = toneGenerator.getNextSample();
        
        // Start a new note (should trigger glissando)
        toneGenerator.startNote(72, 1.0f, 8192);
        
        // Get samples during glissando
        float sample3 = toneGenerator.getNextSample();
        float sample4 = toneGenerator.getNextSample();
        
        // Check that the samples are different (frequency is changing)
        expect(std::abs(sample1 - sample3) > 0.0001f || std::abs(sample2 - sample4) > 0.0001f);
        
        // Disable glissando
        toneGenerator.setGlissando(false);
        
        // Start a note
        toneGenerator.startNote(60, 1.0f, 8192);
        
        // Get samples with initial note
        sample1 = toneGenerator.getNextSample();
        sample2 = toneGenerator.getNextSample();
        
        // Start a new note (should not trigger glissando)
        toneGenerator.startNote(72, 1.0f, 8192);
        
        // Get samples with new note
        sample3 = toneGenerator.getNextSample();
        sample4 = toneGenerator.getNextSample();
        
        // Check that the samples are different (frequency has changed immediately)
        expect(std::abs(sample1 - sample3) > 0.0001f || std::abs(sample2 - sample4) > 0.0001f);
    }
    
    void testLfoModulation()
    {
        beginTest("LFO Modulation Test");
        
        // Create APVTS holder to manage memory
        APVTSHolder apvtsHolder;
        
        // Create mock tone generator
        testing::MockToneGenerator toneGenerator(apvtsHolder.getAPVTS());
        
        // Prepare the tone generator
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        toneGenerator.prepare(spec);
        
        // Start a note
        toneGenerator.startNote(60, 1.0f, 8192);
        
        // Get samples without LFO modulation
        float sample1 = toneGenerator.getNextSample();
        float sample2 = toneGenerator.getNextSample();
        
        // Apply LFO modulation
        toneGenerator.setLfoModulation(1.0f);
        
        // Get samples with LFO modulation
        float sample3 = toneGenerator.getNextSample();
        float sample4 = toneGenerator.getNextSample();
        
        // Check that the samples are different (frequency has changed)
        expect(std::abs(sample1 - sample3) > 0.0001f || std::abs(sample2 - sample4) > 0.0001f);
        
        // Remove LFO modulation
        toneGenerator.setLfoModulation(0.0f);
        
        // Get samples without LFO modulation
        float sample5 = toneGenerator.getNextSample();
        float sample6 = toneGenerator.getNextSample();
        
        // Check that the samples are different (frequency has changed back)
        expect(std::abs(sample3 - sample5) > 0.0001f || std::abs(sample4 - sample6) > 0.0001f);
    }
};

static ToneGeneratorTest toneGeneratorTest;
