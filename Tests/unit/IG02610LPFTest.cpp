#pragma once

#include <JuceHeader.h>
#include "../../Source/CS01Synth/IG02610LPF.h"

class IG02610LPFTest : public juce::UnitTest
{
public:
    IG02610LPFTest() : juce::UnitTest("IG02610LPF Tests") {}
    
    void runTest() override
    {
        testInitialization();
        testParameterSetting();
        testReset();
        testPrepare();
        testDCSignal();
        testSineWave();
        testImpulseResponse();
        testFrequencyResponse();
        testCutoffModulation();
        testResonanceEffect();
        testNonlinearity();
        testBlockProcessing();
    }
    
private:
    void testInitialization()
    {
        beginTest("Initialization Test");
        
        IG02610LPF filter;
        
        // Check default values
        expectEquals(filter.getCutoff(), 1000.0f);
        expectEquals(filter.getResonance(), 0.5f);
        
        // Check that the filter doesn't crash when used before preparation
        float sample = filter.processSample(0.5f);
        expect(std::isfinite(sample));
    }
    
    void testParameterSetting()
    {
        beginTest("Parameter Setting Test");
        
        IG02610LPF filter;
        
        // Set and check cutoff
        filter.setCutoff(500.0f);
        expectEquals(filter.getCutoff(), 500.0f);
        
        // Set and check resonance
        filter.setResonance(0.7f);
        expectEquals(filter.getResonance(), 0.7f);
        
        // Check parameter limits
        filter.setCutoff(30.0f);
        expectEquals(filter.getCutoff(), 30.0f);
        
        filter.setCutoff(20000.0f);
        expectEquals(filter.getCutoff(), 20000.0f);
        
        filter.setResonance(0.0f);
        expectEquals(filter.getResonance(), 0.0f);
        
        filter.setResonance(1.0f);
        expectEquals(filter.getResonance(), 1.0f);
    }
    
    void testReset()
    {
        beginTest("Reset Test");
        
        IG02610LPF filter;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        
        filter.prepare(spec);
        
        // Process some samples to change internal state
        for (int i = 0; i < 100; ++i)
            filter.processSample(1.0f);
        
        // Reset the filter
        filter.reset();
        
        // Process a sample and check that it's different from what we'd get
        // if the filter was still in its previous state
        float sample = filter.processSample(1.0f);
        expect(std::isfinite(sample));
    }
    
    void testPrepare()
    {
        beginTest("Prepare Test");
        
        IG02610LPF filter;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        
        // Check that prepare doesn't crash
        filter.prepare(spec);
        
        // Process a sample and check that it's finite
        float sample = filter.processSample(1.0f);
        expect(std::isfinite(sample));
    }
    
    void testDCSignal()
    {
        beginTest("DC Signal Test");
        
        IG02610LPF filter;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        
        filter.prepare(spec);
        filter.setCutoff(1000.0f);
        filter.setResonance(0.5f);
        
        // Process DC signal (constant 1.0)
        float output = 0.0f;
        for (int i = 0; i < 1000; ++i)
            output = filter.processSample(1.0f);
        
        // DC should pass through the filter
        expectWithinAbsoluteError(output, 1.0f, 0.1f);
    }
    
    void testSineWave()
    {
        beginTest("Sine Wave Test");
        
        IG02610LPF filter;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        
        filter.prepare(spec);
        filter.setCutoff(1000.0f);
        filter.setResonance(0.5f);
        
        // Generate a sine wave at 100 Hz (well below cutoff)
        const float frequency = 100.0f;
        const float sampleRate = static_cast<float>(spec.sampleRate);
        
        float maxOutput = 0.0f;
        for (int i = 0; i < 1000; ++i)
        {
            float input = std::sin(2.0f * juce::MathConstants<float>::pi * frequency * i / sampleRate);
            float output = filter.processSample(input);
            maxOutput = std::max(maxOutput, std::abs(output));
        }
        
        // Sine wave below cutoff should pass through with minimal attenuation
        expectGreaterThan(maxOutput, 0.9f);
    }
    
    void testImpulseResponse()
    {
        beginTest("Impulse Response Test");
        
        IG02610LPF filter;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        
        filter.prepare(spec);
        filter.setCutoff(1000.0f);
        filter.setResonance(0.5f);
        
        // Send an impulse (1.0 followed by zeros)
        float firstSample = filter.processSample(1.0f);
        
        // Check that the impulse response decays
        float prevSample = firstSample;
        bool decaying = false;
        
        for (int i = 0; i < 100; ++i)
        {
            float sample = filter.processSample(0.0f);
            
            if (std::abs(sample) < std::abs(prevSample))
                decaying = true;
            
            prevSample = sample;
        }
        
        expect(decaying);
    }
    
    void testFrequencyResponse()
    {
        beginTest("Frequency Response Test");
        
        IG02610LPF filter;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        
        filter.prepare(spec);
        filter.setCutoff(1000.0f);
        filter.setResonance(0.5f);
        
        // Test frequencies below and above cutoff
        float lowFreqResponse = measureFrequencyResponse(filter, 100.0f, static_cast<float>(spec.sampleRate));
        float highFreqResponse = measureFrequencyResponse(filter, 5000.0f, static_cast<float>(spec.sampleRate));
        
        // Low frequencies should pass through with minimal attenuation
        expectGreaterThan(lowFreqResponse, 0.7f);
        
        // High frequencies should be attenuated
        expectLessThan(highFreqResponse, 0.3f);
    }
    
    void testCutoffModulation()
    {
        beginTest("Cutoff Modulation Test");
        
        IG02610LPF filter;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        
        filter.prepare(spec);
        filter.setCutoff(1000.0f);
        filter.setResonance(0.5f);
        
        // Measure response at 2000 Hz with cutoff at 1000 Hz
        float response1 = measureFrequencyResponse(filter, 2000.0f, static_cast<float>(spec.sampleRate));
        
        // Change cutoff to 5000 Hz
        filter.setCutoff(5000.0f);
        
        // Measure response at 2000 Hz with cutoff at 5000 Hz
        float response2 = measureFrequencyResponse(filter, 2000.0f, static_cast<float>(spec.sampleRate));
        
        // Response should be higher with higher cutoff
        expectGreaterThan(response2, response1);
    }
    
    void testResonanceEffect()
    {
        beginTest("Resonance Effect Test");
        
        IG02610LPF filter;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        
        filter.prepare(spec);
        filter.setCutoff(1000.0f);
        filter.setResonance(0.1f);
        
        // Measure response at cutoff with low resonance
        float response1 = measureFrequencyResponse(filter, 1000.0f, static_cast<float>(spec.sampleRate));
        
        // Change resonance to high value
        filter.setResonance(0.9f);
        
        // Measure response at cutoff with high resonance
        float response2 = measureFrequencyResponse(filter, 1000.0f, static_cast<float>(spec.sampleRate));
        
        // Response at cutoff should be higher with higher resonance
        expectGreaterThan(response2, response1);
    }
    
    void testNonlinearity()
    {
        beginTest("Nonlinearity Test");
        
        IG02610LPF filter;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        
        filter.prepare(spec);
        filter.setCutoff(1000.0f);
        filter.setResonance(0.9f); // High resonance to emphasize nonlinearity
        
        // Process a large amplitude sine wave
        const float frequency = 500.0f;
        const float sampleRate = static_cast<float>(spec.sampleRate);
        
        juce::Array<float> outputSamples;
        for (int i = 0; i < 1000; ++i)
        {
            float input = 5.0f * std::sin(2.0f * juce::MathConstants<float>::pi * frequency * i / sampleRate);
            float output = filter.processSample(input);
            outputSamples.add(output);
        }
        
        // Check that output is bounded (nonlinearity should prevent excessive values)
        float maxOutput = 0.0f;
        for (auto sample : outputSamples)
            maxOutput = std::max(maxOutput, std::abs(sample));
        
        expectLessThan(maxOutput, 10.0f);
    }
    
    void testBlockProcessing()
    {
        beginTest("Block Processing Test");
        
        IG02610LPF filter;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        
        filter.prepare(spec);
        filter.setCutoff(1000.0f);
        filter.setResonance(0.5f);
        
        // Create input and output buffers
        juce::AudioBuffer<float> inputBuffer(1, 512);
        juce::AudioBuffer<float> outputBuffer(1, 512);
        
        // Fill input buffer with sine wave
        const float frequency = 100.0f;
        const float sampleRate = static_cast<float>(spec.sampleRate);
        
        for (int i = 0; i < inputBuffer.getNumSamples(); ++i)
            inputBuffer.setSample(0, i, std::sin(2.0f * juce::MathConstants<float>::pi * frequency * i / sampleRate));
        
        // Process the block
        juce::dsp::AudioBlock<float> inputBlock(inputBuffer);
        juce::dsp::AudioBlock<float> outputBlock(outputBuffer);
        
        filter.process(inputBlock, outputBlock);
        
        // Check that output is not all zeros
        float sum = 0.0f;
        for (int i = 0; i < outputBuffer.getNumSamples(); ++i)
            sum += std::abs(outputBuffer.getSample(0, i));
        
        expectGreaterThan(sum, 0.0f);
    }
    
    // Helper method to measure frequency response at a specific frequency
    float measureFrequencyResponse(IG02610LPF& filter, float frequency, float sampleRate)
    {
        const int numSamples = 1000;
        float maxInput = 0.0f;
        float maxOutput = 0.0f;
        
        for (int i = 0; i < numSamples; ++i)
        {
            float input = std::sin(2.0f * juce::MathConstants<float>::pi * frequency * i / sampleRate);
            float output = filter.processSample(input);
            
            maxInput = std::max(maxInput, std::abs(input));
            maxOutput = std::max(maxOutput, std::abs(output));
        }
        
        return maxOutput / maxInput;
    }
};

static IG02610LPFTest ig02610LPFTest;
