#pragma once

#include <JuceHeader.h>

/**
 * IWaveformStrategy - Strategy interface for waveform generation
 * 
 * This interface defines the contract for different waveform generation strategies.
 * Each concrete strategy implements a specific waveform type (Triangle, Sawtooth, etc.)
 */
class IWaveformStrategy
{
public:
    virtual ~IWaveformStrategy() = default;
    
    /**
     * Generate a waveform sample from the master square wave
     * 
     * @param masterSquare The master square wave input
     * @param phase Current phase (0.0f to 1.0f)
     * @param phaseIncrement Phase increment per sample
     * @param sampleRate Current sample rate
     * @param leakyIntegratorState Reference to integrator state (for triangle wave)
     * @param dcBlockerState Reference to DC blocker state (for triangle wave)
     * @param previousSample Reference to previous sample state (for filtering)
     * @param pwmLfo Reference to PWM LFO (for PWM wave)
     * @return Generated waveform sample
     */
    virtual float generate(
        float masterSquare,
        float phase,
        float phaseIncrement,
        float sampleRate,
        float& previousSample,
        juce::dsp::Oscillator<float>& pwmLfo
    ) = 0;
    
    /**
     * Reset the strategy's internal state
     */
    virtual void reset() {}
};
