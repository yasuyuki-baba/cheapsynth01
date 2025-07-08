#pragma once

#include <JuceHeader.h>
#include "IWaveformStrategy.h"

namespace
{
    float poly_blep(float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }
}

/**
 * TriangleWaveformStrategy - Generates CS-01 style triangle wave
 */
class TriangleWaveformStrategy : public IWaveformStrategy
{
public:
    float generate(
        float masterSquare,
        float phase,
        float phaseIncrement,
        float sampleRate,
        float& previousSample,
        juce::dsp::Oscillator<float>& pwmLfo
    ) override
    {
        // Use internal state like other waveforms for independence
        triangleIntegrator += masterSquare * phaseIncrement * 8.0f;
        
        // Apply gentle leaky integration
        triangleIntegrator *= 0.9999f;
        
        // Simple DC blocker
        float output = triangleIntegrator - triangleDCBlocker;
        triangleDCBlocker += (triangleIntegrator - triangleDCBlocker) * 0.005f;
        
        // CS-01 triangle wave characteristics - proper amplitude
        float triangleWave = output * 1.2f;
        
        // Add slight harmonic coloration typical of CS-01
        triangleWave += std::sin(triangleWave * juce::MathConstants<float>::pi) * 0.1f;
        
        return triangleWave;
    }
    
    void reset() override
    {
        triangleIntegrator = 0.0f;
        triangleDCBlocker = 0.0f;
    }

private:
    float triangleIntegrator = 0.0f;
    float triangleDCBlocker = 0.0f;
};

/**
 * SawtoothWaveformStrategy - Generates CS-01 style sawtooth wave
 */
class SawtoothWaveformStrategy : public IWaveformStrategy
{
public:
    float generate(
        float masterSquare,
        float phase,
        float phaseIncrement,
        float sampleRate,
        float& previousSample,
        juce::dsp::Oscillator<float>& pwmLfo
    ) override
    {
        // Convert square to sawtooth using integration-like process
        sawtoothState += (masterSquare > 0 ? phaseIncrement : -phaseIncrement) * 2.0f;
        sawtoothState *= 0.998f; // Decay to prevent buildup
        
        // CS-01 sawtooth wave characteristics with downward slope
        float sawValue = 1.0f - (phase * 2.0f) + sawtoothState * 0.1f;
        
        // Emphasize higher harmonics (CS-01 characteristic)
        return sawValue * 0.7f + std::sin(sawValue * juce::MathConstants<float>::pi) * 0.3f;
    }
    
    void reset() override
    {
        sawtoothState = 0.0f;
    }

private:
    float sawtoothState = 0.0f;
};

/**
 * SquareWaveformStrategy - Generates square wave directly from master
 */
class SquareWaveformStrategy : public IWaveformStrategy
{
public:
    float generate(
        float masterSquare,
        float phase,
        float phaseIncrement,
        float sampleRate,
        float& previousSample,
        juce::dsp::Oscillator<float>& pwmLfo
    ) override
    {
        // Use master square directly
        return masterSquare;
    }
};

/**
 * PulseWaveformStrategy - Generates pulse wave with 25% duty cycle
 */
class PulseWaveformStrategy : public IWaveformStrategy
{
public:
    float generate(
        float masterSquare,
        float phase,
        float phaseIncrement,
        float sampleRate,
        float& previousSample,
        juce::dsp::Oscillator<float>& pwmLfo
    ) override
    {
        // Generate pulse from master timing with ~25% duty cycle
        float t = phase;
        float pulseWidth = 0.25f;
        float value = (t < pulseWidth) ? 1.0f : -1.0f;
        
        // Apply poly_blep anti-aliasing
        value += poly_blep(t, phaseIncrement);
        value -= poly_blep(fmod(t + (1.0f - pulseWidth), 1.0f), phaseIncrement);
        
        // Apply analog-style saturation
        return std::tanh(value * 1.5f);
    }
};

/**
 * PWMWaveformStrategy - Generates PWM wave with LFO modulation
 */
class PWMWaveformStrategy : public IWaveformStrategy
{
public:
    float generate(
        float masterSquare,
        float phase,
        float phaseIncrement,
        float sampleRate,
        float& previousSample,
        juce::dsp::Oscillator<float>& pwmLfo
    ) override
    {
        // Generate PWM from master timing with LFO modulation
        float pwmModulation = pwmLfo.processSample(0.0f);
        float pulseWidth = 0.5f + (pwmModulation * 0.4f); // 10% to 90% range
        pulseWidth = juce::jlimit(0.05f, 0.95f, pulseWidth); // Safety clamp
        
        float t = phase;
        float value = (t < pulseWidth) ? 1.0f : -1.0f;
        
        // Apply poly_blep anti-aliasing
        value += poly_blep(t, phaseIncrement);
        value -= poly_blep(fmod(t + (1.0f - pulseWidth), 1.0f), phaseIncrement);
        
        // Apply analog-style saturation with PWM character
        value = std::tanh(value * 1.3f);
        
        // Subtle high-frequency roll-off
        previousSample = previousSample * 0.98f + value * 0.02f;
        return value * 0.9f + previousSample * 0.1f;
    }
};
