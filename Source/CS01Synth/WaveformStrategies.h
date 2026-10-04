#pragma once

#include <JuceHeader.h>
#include "IWaveformStrategy.h"

namespace {
float poly_blep(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}
}  // namespace

// Preserve empirical 44.1 kHz time constants, not hardware-calibrated values.
class WaveformTimeConstants {
public:
    void update(float sampleRate) {
        if (sampleRate == cachedSampleRate || sampleRate <= 0.0f)
            return;
        cachedSampleRate = sampleRate;
        const double ratio = 44100.0 / sampleRate;
        triangleLeak = static_cast<float>(std::pow(static_cast<double>(0.9999f), ratio));
        triangleDcAmount = static_cast<float>(-std::expm1(std::log(1.0 - static_cast<double>(0.005f)) * ratio));
        sawLeak = static_cast<float>(std::pow(static_cast<double>(0.998f), ratio));
        pwmPole = static_cast<float>(std::pow(static_cast<double>(0.98f), ratio));
    }
    float triangleLeak = 0.9999f;
    float triangleDcAmount = 0.005f;
    float sawLeak = 0.998f;
    float pwmPole = 0.98f;
private:
    float cachedSampleRate = 44100.0f;
};

/**
 * TriangleWaveformStrategy - Generates CS-01 style triangle wave
 */
class TriangleWaveformStrategy : public IWaveformStrategy {
   public:
    float generate(float masterSquare, float phase, float phaseIncrement, float sampleRate,
                   float& previousSample, juce::dsp::Oscillator<double>& pwmLfo) override {
        // Use internal state like other waveforms for independence
        timeConstants.update(sampleRate);
        triangleIntegrator += masterSquare * phaseIncrement * 8.0f;

        // Apply gentle leaky integration
        triangleIntegrator *= timeConstants.triangleLeak;

        // Simple DC blocker
        float output = triangleIntegrator - triangleDCBlocker;
        triangleDCBlocker += (triangleIntegrator - triangleDCBlocker) * timeConstants.triangleDcAmount;

        // CS-01 triangle wave characteristics - proper amplitude
        float triangleWave = output * 1.2f;

        // Add slight harmonic coloration typical of CS-01
        triangleWave += std::sin(triangleWave * juce::MathConstants<float>::pi) * 0.1f;

        return triangleWave;
    }

    void reset() override {
        triangleIntegrator = 0.0f;
        triangleDCBlocker = 0.0f;
    }

   private:
    float triangleIntegrator = 0.0f;
    WaveformTimeConstants timeConstants;
    float triangleDCBlocker = 0.0f;
};

/**
 * SawtoothWaveformStrategy - Generates CS-01 style sawtooth wave
 */
class SawtoothWaveformStrategy : public IWaveformStrategy {
   public:
    float generate(float masterSquare, float phase, float phaseIncrement, float sampleRate,
                   float& previousSample, juce::dsp::Oscillator<double>& pwmLfo) override {
        // Convert square to sawtooth using integration-like process
        timeConstants.update(sampleRate);
        sawtoothState += (masterSquare > 0 ? phaseIncrement : -phaseIncrement) * 2.0f;
        sawtoothState *= timeConstants.sawLeak;  // Decay to prevent buildup

        // CS-01 sawtooth wave characteristics with downward slope
        float sawValue = 1.0f - (phase * 2.0f) + sawtoothState * 0.1f;

        // Emphasize higher harmonics (CS-01 characteristic)
        return sawValue * 0.7f + std::sin(sawValue * juce::MathConstants<float>::pi) * 0.3f;
    }

    void reset() override {
        sawtoothState = 0.0f;
    }

   private:
    float sawtoothState = 0.0f;
    WaveformTimeConstants timeConstants;
};

/**
 * SquareWaveformStrategy - Generates square wave directly from master
 */
class SquareWaveformStrategy : public IWaveformStrategy {
   public:
    float generate(float masterSquare, float phase, float phaseIncrement, float sampleRate,
                   float& previousSample, juce::dsp::Oscillator<double>& pwmLfo) override {
        // Use master square directly
        return masterSquare;
    }
};

/**
 * PulseWaveformStrategy - Generates pulse wave with 25% duty cycle
 */
class PulseWaveformStrategy : public IWaveformStrategy {
   public:
    float generate(float masterSquare, float phase, float phaseIncrement, float sampleRate,
                   float& previousSample, juce::dsp::Oscillator<double>& pwmLfo) override {
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
class PWMWaveformStrategy : public IWaveformStrategy {
   public:
    float generate(float masterSquare, float phase, float phaseIncrement, float sampleRate,
                   float& previousSample, juce::dsp::Oscillator<double>& pwmLfo) override {
        // Generate PWM from master timing with LFO modulation
        float pwmModulation = pwmLfo.processSample(0.0f);
        float pulseWidth = 0.5f + (pwmModulation * 0.4f);     // 10% to 90% range
        pulseWidth = juce::jlimit(0.05f, 0.95f, pulseWidth);  // Safety clamp

        float t = phase;
        float value = (t < pulseWidth) ? 1.0f : -1.0f;

        // Apply poly_blep anti-aliasing
        value += poly_blep(t, phaseIncrement);
        value -= poly_blep(fmod(t + (1.0f - pulseWidth), 1.0f), phaseIncrement);

        // Apply analog-style saturation with PWM character
        value = std::tanh(value * 1.3f);

        // Subtle high-frequency roll-off
        timeConstants.update(sampleRate);
        previousSample = previousSample * timeConstants.pwmPole + value * (1.0f - timeConstants.pwmPole);
        return value * 0.9f + previousSample * 0.1f;
    }
private:
    WaveformTimeConstants timeConstants;
};
