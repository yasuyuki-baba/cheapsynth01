#pragma once

#include <JuceHeader.h>
#include <cmath>

namespace testing
{
    /**
     * Mock implementation of JUCE dsp::Oscillator for testing
     * This provides a simplified oscillator that doesn't rely on JUCE's DSP module
     */
    class MockOscillator
    {
    public:
        // Waveform types matching the original oscillator
        enum class WaveformType
        {
            sine,
            saw,
            square,
            triangle,
            noise
        };
        
        MockOscillator() = default;
        
        // Set the oscillator waveform
        void setWaveform(WaveformType newWaveform)
        {
            waveform = newWaveform;
        }
        
        // Set the oscillator frequency
        void setFrequency(float newFrequency, bool force = false)
        {
            frequency = newFrequency;
            updatePhaseIncrement();
        }
        
        // Prepare the oscillator for processing
        void prepare(const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            updatePhaseIncrement();
        }
        
        // Reset the oscillator state
        void reset() noexcept
        {
            phase = 0.0f;
        }
        
        // Process a single sample
        float processSample(float /*input*/) noexcept
        {
            const float result = generateSample();
            phase += phaseIncrement;
            if (phase >= 1.0f)
                phase -= 1.0f;
            return result;
        }
        
    private:
        // Generate a sample based on the current waveform and phase
        float generateSample() const noexcept
        {
            switch (waveform)
            {
                case WaveformType::sine:
                    return std::sin(phase * juce::MathConstants<float>::twoPi);
                    
                case WaveformType::saw:
                    return 2.0f * phase - 1.0f;
                    
                case WaveformType::square:
                    return phase < 0.5f ? 1.0f : -1.0f;
                    
                case WaveformType::triangle:
                    return phase < 0.5f ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
                    
                case WaveformType::noise:
                    return juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
                    
                default:
                    return 0.0f;
            }
        }
        
        // Update the phase increment based on frequency and sample rate
        void updatePhaseIncrement() noexcept
        {
            if (sampleRate > 0)
                phaseIncrement = frequency / static_cast<float>(sampleRate);
        }
        
        WaveformType waveform = WaveformType::sine;
        float frequency = 440.0f;
        float phase = 0.0f;
        float phaseIncrement = 0.0f;
        double sampleRate = 44100.0;
    };
}
