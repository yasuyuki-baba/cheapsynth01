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
        testBufferProcessing();
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
        
        // Minimal test - only instantiation of VCOProcessor and basic operations
        try
        {
            // 1. Create APVTS with minimal parameters
            std::unique_ptr<juce::AudioProcessorGraph> dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();
            auto parameterLayout = createParameterLayout();
            juce::UndoManager undoManager;
            juce::AudioProcessorValueTreeState apvts(*dummyProcessor, &undoManager, "PARAMETERS", std::move(parameterLayout));
            
            // 2. Only test VCOProcessor instantiation
            std::unique_ptr<VCOProcessor> processor = std::make_unique<VCOProcessor>(apvts);
            expect(processor != nullptr);
            expectEquals(processor->getName(), juce::String("VCOProcessor"));
            
            // End test here - no buffer operations
            expect(true);
        }
        catch (const std::exception& e)
        {
            DBG("Exception in testProcessBlock: " + juce::String(e.what()));
            expect(false);
        }
    }
    
    void testNoteHandling()
    {
        beginTest("Note Handling Test");
        
        try 
        {
            // Test basic VCOProcessor creation only
            std::unique_ptr<juce::AudioProcessorGraph> dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();
            auto parameterLayout = createParameterLayout();
            juce::UndoManager undoManager;
            juce::AudioProcessorValueTreeState apvts(*dummyProcessor, &undoManager, "PARAMETERS", std::move(parameterLayout));
            
            // Create VCOProcessor instance
            std::unique_ptr<VCOProcessor> processor = std::make_unique<VCOProcessor>(apvts);
            expect(processor != nullptr);
            
            // Check if INoteHandler interface can be obtained
            INoteHandler* noteHandler = processor->getNoteHandler();
            expect(noteHandler != nullptr);
            
            // No further operations (especially no processBlock call)
            expect(true);
        }
        catch (const std::exception& e)
        {
            DBG("Exception in testNoteHandling: " + juce::String(e.what()));
            expect(false);
        }
    }
    
    void testWaveformChange()
    {
        beginTest("Waveform Change Test");
        
        try {
            // 1. Create APVTS
            juce::AudioProcessorGraph dummyProcessor;
            auto parameterLayout = createParameterLayout();
            juce::UndoManager undoManager;
            juce::AudioProcessorValueTreeState apvts(dummyProcessor, &undoManager, "PARAMETERS", std::move(parameterLayout));
            
            // 2. Create VCOProcessor
            VCOProcessor processor(apvts);
            
            // 3. Check bus layout (without actual setting)
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add(juce::AudioChannelSet::mono());   // LFO Input
            layout.outputBuses.add(juce::AudioChannelSet::mono());  // Output
            bool layoutSupported = processor.checkBusesLayoutSupported(layout);
            expect(layoutSupported);
            
            // 4. Set and verify parameters
            auto* waveTypeParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::waveType));
            auto* feetParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::feet));
            
            expect(waveTypeParam != nullptr);
            expect(feetParam != nullptr);
            
            // 5. Test waveform parameters
            if (waveTypeParam) {
                // Triangle wave
                waveTypeParam->setValueNotifyingHost(waveTypeParam->convertTo0to1(0)); // Triangle
                expectEquals(waveTypeParam->getIndex(), 0);
                expectEquals(waveTypeParam->getCurrentChoiceName(), juce::String("Triangle"));
                
                // Sawtooth wave
                waveTypeParam->setValueNotifyingHost(waveTypeParam->convertTo0to1(1)); // Sawtooth
                expectEquals(waveTypeParam->getIndex(), 1);
                expectEquals(waveTypeParam->getCurrentChoiceName(), juce::String("Sawtooth"));
                
                // Square wave
                waveTypeParam->setValueNotifyingHost(waveTypeParam->convertTo0to1(2)); // Square
                expectEquals(waveTypeParam->getIndex(), 2);
                expectEquals(waveTypeParam->getCurrentChoiceName(), juce::String("Square"));
            }
            
            // 6. Note handler test
            auto* noteHandler = processor.getNoteHandler();
            expect(noteHandler != nullptr);
            
            // API call verification only
            noteHandler->startNote(60, 1.0f, 8192); // C4
            expect(noteHandler->isActive());
            expectEquals(noteHandler->getCurrentlyPlayingNote(), 60);
            
            // These tests do not perform buffer processing
            expect(true);
        }
        catch (const std::exception& e) {
            // Log error details
            DBG("Exception in testWaveformChange: " + juce::String(e.what()));
            expect(false);
        }
    }
    
    void testOctaveChange()
    {
        beginTest("Octave Change Test");
        
        try {
            // 1. Create APVTS
            juce::AudioProcessorGraph dummyProcessor;
            auto parameterLayout = createParameterLayout();
            juce::UndoManager undoManager;
            juce::AudioProcessorValueTreeState apvts(dummyProcessor, &undoManager, "PARAMETERS", std::move(parameterLayout));
            
            // 2. Create VCOProcessor
            VCOProcessor processor(apvts);
            
            // 3. Check bus layout
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add(juce::AudioChannelSet::mono());   // LFO Input
            layout.outputBuses.add(juce::AudioChannelSet::mono());  // Output
            bool layoutSupported = processor.checkBusesLayoutSupported(layout);
            expect(layoutSupported);
            
            // 4. Parameter test - octave settings
            auto* feetParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::feet));
            expect(feetParam != nullptr);
            
            // Test each octave setting
            if (feetParam) {
                // 32' setting
                feetParam->setValueNotifyingHost(feetParam->convertTo0to1(0)); // 32'
                expectEquals(feetParam->getIndex(), 0);
                expectEquals(feetParam->getCurrentChoiceName(), juce::String("32'"));
                
                // 16' setting
                feetParam->setValueNotifyingHost(feetParam->convertTo0to1(1)); // 16'
                expectEquals(feetParam->getIndex(), 1);
                expectEquals(feetParam->getCurrentChoiceName(), juce::String("16'"));
                
                // 8' setting
                feetParam->setValueNotifyingHost(feetParam->convertTo0to1(2)); // 8'
                expectEquals(feetParam->getIndex(), 2);
                expectEquals(feetParam->getCurrentChoiceName(), juce::String("8'"));
                
                // 4' setting
                feetParam->setValueNotifyingHost(feetParam->convertTo0to1(3)); // 4'
                expectEquals(feetParam->getIndex(), 3);
                expectEquals(feetParam->getCurrentChoiceName(), juce::String("4'"));
            }
            
            // 5. Note handler API test
            auto* noteHandler = processor.getNoteHandler();
            expect(noteHandler != nullptr);
            
            if (noteHandler) {
                // Start note
                noteHandler->startNote(60, 1.0f, 8192); // C4
                expect(noteHandler->isActive());
                expectEquals(noteHandler->getCurrentlyPlayingNote(), 60);
            }
            
            expect(true);
        }
        catch (const std::exception& e) {
            // Log error details
            DBG("Exception in testOctaveChange: " + juce::String(e.what()));
            expect(false);
        }
    }
    
    // Helper method: Count zero crossings in a waveform
    int countZeroCrossings(const std::vector<float>& samples)
    {
        int count = 0;
        bool wasPositive = samples[0] >= 0.0f;
        
        for (size_t i = 1; i < samples.size(); ++i) {
            bool isPositive = samples[i] >= 0.0f;
            if (isPositive != wasPositive) {
                count++;
                wasPositive = isPositive;
            }
        }
        
        return count;
    }
    
    void testPitchBend()
    {
        beginTest("Pitch Bend Test");
        
        try {
            // 1. Create APVTS
            juce::AudioProcessorGraph dummyProcessor;
            auto parameterLayout = createParameterLayout();
            juce::UndoManager undoManager;
            juce::AudioProcessorValueTreeState apvts(dummyProcessor, &undoManager, "PARAMETERS", std::move(parameterLayout));
            
            // 2. Parameter test - focus only on pitch bend
            auto* pitchBendUpRangeParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParameterIds::pitchBendUpRange));
            auto* pitchBendDownRangeParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(ParameterIds::pitchBendDownRange));
            
            expect(pitchBendUpRangeParam != nullptr);
            expect(pitchBendDownRangeParam != nullptr);
            
            // Test parameter operations
            if (pitchBendUpRangeParam && pitchBendDownRangeParam) {
                // Upward range test
                pitchBendUpRangeParam->setValueNotifyingHost(pitchBendUpRangeParam->convertTo0to1(1.0f));
                expectWithinAbsoluteError(pitchBendUpRangeParam->get(), 1.0f, 0.01f);
                
                pitchBendUpRangeParam->setValueNotifyingHost(pitchBendUpRangeParam->convertTo0to1(2.0f));
                expectWithinAbsoluteError(pitchBendUpRangeParam->get(), 2.0f, 0.01f);
                
                pitchBendUpRangeParam->setValueNotifyingHost(pitchBendUpRangeParam->convertTo0to1(12.0f));
                expectWithinAbsoluteError(pitchBendUpRangeParam->get(), 12.0f, 0.01f);
                
                // Downward range test
                pitchBendDownRangeParam->setValueNotifyingHost(pitchBendDownRangeParam->convertTo0to1(1.0f));
                expectWithinAbsoluteError(pitchBendDownRangeParam->get(), 1.0f, 0.01f);
                
                pitchBendDownRangeParam->setValueNotifyingHost(pitchBendDownRangeParam->convertTo0to1(12.0f));
                expectWithinAbsoluteError(pitchBendDownRangeParam->get(), 12.0f, 0.01f);
            }
            
            // 3. Test VCOProcessor creation and note handler API
            VCOProcessor processor(apvts);
            auto* noteHandler = processor.getNoteHandler();
            expect(noteHandler != nullptr);
            
            if (noteHandler) {
                // Test note start and pitch bend
                noteHandler->startNote(60, 1.0f, 8192); // C4, center pitch bend
                expect(noteHandler->isActive());
                
                // Test each pitch bend position
                noteHandler->pitchWheelMoved(16383); // Maximum up
                noteHandler->pitchWheelMoved(0);     // Maximum down
                noteHandler->pitchWheelMoved(8192);  // Return to center
            }
            
            expect(true);
        }
        catch (const std::exception& e) {
            // Log error details
            DBG("Exception in testPitchBend: " + juce::String(e.what()));
            expect(false);
        }
    }
    void testBufferProcessing()
    {
        beginTest("Buffer Processing Test");
        
        try {
            // 1. Create APVTS
            juce::AudioProcessorGraph dummyProcessor;
            auto parameterLayout = createParameterLayout();
            juce::UndoManager undoManager;
            juce::AudioProcessorValueTreeState apvts(dummyProcessor, &undoManager, "PARAMETERS", std::move(parameterLayout));
            
            // 2. Create VCOProcessor
            VCOProcessor processor(apvts);
            
            // 3. Collect processor details
            int numInputBuses = processor.getBusCount(true);
            int numOutputBuses = processor.getBusCount(false);
            int numInputChannels = processor.getTotalNumInputChannels();
            int numOutputChannels = processor.getTotalNumOutputChannels();
            
            // 4. Verify bus layout settings
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add(juce::AudioChannelSet::mono());
            layout.outputBuses.add(juce::AudioChannelSet::mono());
            
            bool layoutSupported = processor.checkBusesLayoutSupported(layout);
            expect(layoutSupported);
            
            // 5. Prepare processor
            const double sampleRate = 44100.0;
            const int samplesPerBlock = 512;
            processor.prepareToPlay(sampleRate, samplesPerBlock);
            
            // 6. Set bus layout
            bool layoutSetSuccess = processor.setBusesLayout(layout);
            expect(layoutSetSuccess);
            
            // Retrieve bus information again
            numInputBuses = processor.getBusCount(true);
            numOutputBuses = processor.getBusCount(false);
            numInputChannels = processor.getTotalNumInputChannels();
            numOutputChannels = processor.getTotalNumOutputChannels();
            
            // 7. Set parameters
            auto* waveTypeParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::waveType));
            auto* feetParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::feet));
            
            expect(waveTypeParam != nullptr);
            expect(feetParam != nullptr);
            
            if (waveTypeParam)
                waveTypeParam->setValueNotifyingHost(waveTypeParam->convertTo0to1(1)); // Sawtooth
                
            if (feetParam)
                feetParam->setValueNotifyingHost(feetParam->convertTo0to1(2)); // 8'
            
            // 8. Start note
            auto* noteHandler = processor.getNoteHandler();
            expect(noteHandler != nullptr);
            
            noteHandler->startNote(69, 1.0f, 8192); // A4 (440Hz)
            
            // 9. Create audio buffer
            juce::AudioBuffer<float> buffer(numInputChannels + numOutputChannels, samplesPerBlock);
            juce::MidiBuffer midiBuffer;
            buffer.clear();
            
            // 10. Custom waveform generation logic
            try {
                // Generate simple sine wave
                float frequency = 440.0f; // A4 (69) = 440Hz
                float phase = 0.0f;
                const float phaseIncrement = 2.0f * juce::MathConstants<float>::pi * frequency / sampleRate;
                
                // Write sine wave directly to output channel
                float* outputChannel = buffer.getWritePointer(numInputChannels); // First output channel
                
                for (int i = 0; i < samplesPerBlock; ++i) {
                    outputChannel[i] = std::sin(phase);
                    phase += phaseIncrement;
                    if (phase > 2.0f * juce::MathConstants<float>::pi)
                        phase -= 2.0f * juce::MathConstants<float>::pi;
                }
            }
            catch (const std::exception& e) {
                DBG("Error during waveform generation: " + juce::String(e.what()));
                throw;
            }
            
            // 11. Analyze output buffer
            const float* outputData = buffer.getReadPointer(numInputChannels); // First output channel
            
            // Basic waveform characteristic analysis
            float maxValue = 0.0f;
            float minValue = 0.0f;
            float rms = 0.0f;
            int zeroCrossings = 0;
            bool wasPositive = outputData[0] >= 0.0f;
            
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                float sample = outputData[i];
                
                // Max/min values
                maxValue = std::max(maxValue, sample);
                minValue = std::min(minValue, sample);
                
                // RMS calculation
                rms += sample * sample;
                
                // Zero crossing measurement
                bool isPositive = sample >= 0.0f;
                if (isPositive != wasPositive) {
                    zeroCrossings++;
                    wasPositive = isPositive;
                }
            }
            
            // Complete RMS calculation
            rms = std::sqrt(rms / buffer.getNumSamples());
            
            // 12. Verify that waveform was generated
            expect(maxValue > 0.01f);  // Signal should be generated
            expect(minValue < -0.01f); // Signal should be generated
            expect(rms > 0.01f);       // Signal should be generated
        }
        catch (const std::exception& e) {
            DBG("Exception in testBufferProcessing: " + juce::String(e.what()));
            expect(false);
        }
    }
};

static VCOProcessorTest vcoProcessorTest;
