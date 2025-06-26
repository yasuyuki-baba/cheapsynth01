#include "IG02610LPF.h"

IG02610LPF::IG02610LPF() 
    : cutoff(1000.0f), resonance(0.1f), sampleRate(44100.0f)
{
    reset();
}

void IG02610LPF::reset()
{
    z1 = z2 = 0.0f;
    
    // Reset input and output stages
    inputStage.reset();
    outputStage.reset();
}

void IG02610LPF::prepare(double newSampleRate)
{
    sampleRate = static_cast<float>(newSampleRate);
    updateCoefficients();
    
    // Prepare input and output stages
    inputStage.prepare(newSampleRate);
    outputStage.prepare(newSampleRate);
}

// More accurate tanh approximation
float IG02610LPF::accurateTanh(float x)
{
    // Use Padé approximation for small values (high accuracy)
    if (std::abs(x) < 1.0f) {
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    } else {
        // Use improved rational approximation for larger values
        const float absX = std::abs(x);
        const float sign = x > 0.0f ? 1.0f : -1.0f;
        return sign * (1.0f - 1.0f / (1.0f + absX + 0.25f * absX * absX));
    }
}

// Input stage processing
float IG02610LPF::processInputStage(float sample)
{
    // Model input capacitor (0.022μF) and resistor (22KΩ) effects
    // High-pass filter characteristic (approx 72Hz cutoff)
    sample = inputStage.dcBlocker.processSample(sample);
    
    // Light saturation in input stage
    const float inputSaturation = 1.1f;
    sample = accurateTanh(sample * inputSaturation);
    
    return sample;
}

// Output stage processing
float IG02610LPF::processOutputStage(float sample)
{
    // Model output capacitor (1/50) effects
    // DC blocking and slight high frequency roll-off
    const float RC = 0.03f; // Time constant
    const float alpha = 1.0f / (RC * outputStage.sampleRate + 1.0f);
    
    // Capacitor charge simulation
    outputStage.capacitorCharge = outputStage.capacitorCharge * (1.0f - alpha) + sample * alpha;
    
    // Remove DC component
    sample = sample - outputStage.capacitorCharge;
    
    // Slight attenuation from output resistor (47KΩ)
    return sample * 0.98f;
}

void IG02610LPF::setCutoffFrequency(float newCutoff)
{
    cutoff = juce::jlimit(20.0f, 20000.0f, newCutoff);
    updateCoefficients();
}

void IG02610LPF::setResonance(float newResonance)
{
    // IG02610 resonance range (limit max to 0.8f)
    resonance = juce::jlimit(0.1f, 0.8f, newResonance);
    updateCoefficients();
}

float IG02610LPF::processSample(int channel, float sample)
{
    // Apply input stage processing
    sample = processInputStage(sample);
    
    // Limit input signal range to prevent overload (using fast min/max)
    sample = sample > 1.0f ? 1.0f : (sample < -1.0f ? -1.0f : sample);
    
    // Standard processing (direct form II transposed structure)
    // More numerically stable and efficient
    const float input = sample;
    const float output = b0 * input + z1;
    
    // Update filter state
    z1 = b1 * input - a1 * output + z2;
    z2 = b2 * input - a2 * output;
    
    // Apply resonance nonlinearity with smooth blending instead of conditional branch
    // This eliminates the branch prediction failure when resonance is near the threshold
    float y = output;
    
    // Calculate resonance drive factor (will be near zero when resonance <= 0.5)
    const float resAmount = resonance > 0.5f ? (resonance - 0.5f) * 3.0f : 0.0f;
    
    // Only apply significant processing when resonance is high enough
    if (resAmount > 0.01f)
    {
        // Use more accurate tanh approximation
        const float driven = y * (1.0f + resAmount);
        const float absInput = std::abs(driven);
        const float fastTanh = accurateTanh(driven * 0.7f);
        
        // Simplified resonance shaping (avoiding expensive exp calculation)
        const float shaped = fastTanh * (1.0f - RESONANCE_SHAPE * (1.0f / (1.0f + absInput * 2.0f)));
        
        // Blend between original and shaped signal based on resonance amount
        y = y * (1.0f - resAmount) + shaped * resAmount;
    }
    
    // Use more accurate tanh approximation for output shaping
    const float driven = y * OUTPUT_DRIVE;
    y = accurateTanh(driven * 0.6f);
    
    // Apply output stage processing
    return processOutputStage(y);
}

void IG02610LPF::updateCoefficients()
{
    // Limit cutoff frequency range
    cutoff = juce::jlimit(20.0f, 20000.0f, cutoff);
    
    // Limit resonance range (max 0.8f to prevent extreme resonance)
    resonance = juce::jlimit(0.1f, 0.8f, resonance);
    
    // Normalized cutoff frequency (0 to π)
    const float wc = juce::MathConstants<float>::pi * cutoff / sampleRate;
    
    // Fast approximation of sin(wc) and cos(wc) using Taylor series
    // sin(x) ≈ x - x³/6 for small x
    // cos(x) ≈ 1 - x²/2 for small x
    const float wcSquared = wc * wc;
    const float sinWc = wc * (1.0f - wcSquared / 6.0f);
    const float cosWc = 1.0f - wcSquared / 2.0f;
    
    // Resonance coefficient (based on PSW4 switch state)
    // Q factor based on original CS-10 specs (Q=10 according to documentation)
    // Pre-compute both paths to avoid branching
    const float qFactor = resonance > 0.5f ? 10.0f : 1.0f;
    
    // Convert to standard second-order filter form
    const float alpha = sinWc / (2.0f * qFactor);
    
    // Adjustment factor based on capacitor ratio from circuit diagram
    const float adjustmentFactor = 1.658f; // Pre-computed sqrt(11) * 0.5
    
    // Standard 2-pole filter coefficients
    const float oneMinusCosWc = 1.0f - cosWc;
    b0 = oneMinusCosWc * 0.5f * adjustmentFactor;
    b1 = oneMinusCosWc * adjustmentFactor;
    b2 = b0;
    a1 = -2.0f * cosWc;
    a2 = 1.0f - alpha;
    
    // Normalize coefficients
    const float norm = 1.0f / (1.0f + alpha);
    b0 *= norm;
    b1 *= norm;
    b2 *= norm;
    a1 *= norm;
    a2 *= norm;
    
    // Apply frequency-dependent adjustments using smooth interpolation
    // instead of conditional branches
    
    // Low frequency adjustment factor (smooth transition around 500Hz)
    const float lowFreqBlend = juce::jlimit(0.0f, 1.0f, cutoff / 500.0f);
    const float lowFreqFactor = 0.8f + 0.2f * lowFreqBlend;
    
    // High frequency adjustment factor (smooth transition around 5000Hz)
    const float highFreqBlend = juce::jlimit(0.0f, 1.0f, (cutoff - 5000.0f) / 15000.0f);
    const float highFreqFactor = 1.0f - highFreqBlend * 0.15f;
    
    // Apply low frequency adjustment (affects all frequencies below 500Hz with smooth transition)
    if (cutoff < 500.0f)
    {
        b0 *= lowFreqFactor;
        b1 *= lowFreqFactor;
        b2 *= lowFreqFactor;
    }
    
    // Apply high frequency adjustment (affects all frequencies above 5000Hz with smooth transition)
    if (cutoff > 5000.0f)
    {
        a1 *= highFreqFactor;
        a2 *= highFreqFactor;
    }
}


void IG02610LPF::processBlock(float* samples, int numSamples)
{
    // Process a block of mono samples
    for (int i = 0; i < numSamples; ++i)
    {
        samples[i] = processSample(0, samples[i]);
    }
}

void IG02610LPF::processBlock(float** channelData, int numChannels, int numSamples)
{
    // Process each channel separately
    // Note: For true stereo processing, we would need separate state variables per channel
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* channelSamples = channelData[ch];
        
        // Process this channel's samples
        for (int i = 0; i < numSamples; ++i)
        {
            channelSamples[i] = processSample(ch, channelSamples[i]);
        }
    }
}

void IG02610LPF::processBlock(float* samples, int numSamples, 
                             const float* cutoffModulation, float baseResonance)
{
    // Store original cutoff and resonance to restore later
    const float originalCutoff = cutoff;
    const float originalResonance = resonance;
    
    // Set base resonance
    setResonance(baseResonance);
    
    // Process each sample with its own cutoff frequency
    for (int i = 0; i < numSamples; ++i)
    {
        // Update cutoff for this sample
        setCutoffFrequency(cutoffModulation[i]);
        
        // Process the sample
        samples[i] = processSample(0, samples[i]);
    }
    
    // Restore original parameters
    cutoff = originalCutoff;
    resonance = originalResonance;
    updateCoefficients();
}
