#include "IG02610LPF.h"

IG02610LPF::IG02610LPF() 
    : cutoff(1000.0f), resonance(0.1f), sampleRate(0.0f)  // Unset state, will be set by prepare()
{
    reset();
}

IG02610LPF::IG02610LPF(double sampleRate) 
    : cutoff(1000.0f), resonance(0.1f), sampleRate(static_cast<float>(sampleRate))
{
    reset();
    updateCoefficients();
}

void IG02610LPF::reset()
{
    z1 = z2 = 0.0f;
    inputLevelSmoothed = 0.0f;
    
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

// Input stage processing - Clean DC blocking based on circuit diagram
float IG02610LPF::processInputStage(float sample)
{
    // Model input capacitor (0.022μF) and resistor (22KΩ) from circuit diagram
    // Clean DC blocking without distortion - actual circuit is linear
    sample = inputStage.dcBlocker.processSample(sample);
    
    return sample;
}

// Output stage processing - Clean DC blocking based on circuit diagram
float IG02610LPF::processOutputStage(float sample)
{
    // Model the output capacitor (1/50 = 0.02µF) and resistor (10KΩ) from circuit diagram
    // Clean DC blocking without additional coloration - actual circuit is linear
    const float cutoffFreq = 8.0f; // Approximately 8Hz cutoff based on RC values
    const float alpha = 1.0f / (1.0f + 2.0f * juce::MathConstants<float>::pi * cutoffFreq / outputStage.sampleRate);
    
    // Clean DC blocking filter
    outputStage.prevOutput = alpha * (outputStage.prevOutput + sample - outputStage.prevInput);
    outputStage.prevInput = sample;
    
    return outputStage.prevOutput;
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
    // Apply input stage processing (clean DC blocking only)
    sample = processInputStage(sample);
    
    // Soft limiting to prevent overload (gentler than hard clipping)
    sample = sample > 1.0f ? 1.0f : (sample < -1.0f ? -1.0f : sample);
    
    // Track input level with envelope follower for OTA input level dependency
    float inputLevel = std::abs(sample);
    inputLevelSmoothed = inputLevelSmoothed * LEVEL_SMOOTHING + 
                       inputLevel * (1.0f - LEVEL_SMOOTHING);
    
    // Apply OTA input level dependent cutoff modulation
    // Large signals make cutoff slightly higher (brighter), small signals make it lower (darker)
    float levelModulation = (inputLevelSmoothed - 0.5f) * INPUT_LEVEL_INFLUENCE;
    float dynamicCutoff = cutoff * (1.0f + levelModulation);
    
    // Temporarily update cutoff for this sample if there's significant modulation
    bool needsUpdate = std::abs(levelModulation) > 0.001f;
    float originalCutoff = cutoff;
    if (needsUpdate) {
        cutoff = juce::jlimit(20.0f, 20000.0f, dynamicCutoff);
        updateCoefficients();
    }
    
    // Standard 2nd order filter processing (direct form II transposed)
    const float input = sample;
    const float output = b0 * input + z1;
    
    // Update filter state variables
    z1 = b1 * input - a1 * output + z2;
    z2 = b2 * input - a2 * output;
    
    // Restore original cutoff if it was temporarily changed
    if (needsUpdate) {
        cutoff = originalCutoff;
        // Note: We don't update coefficients back here for performance,
        // they will be updated when setCutoffFrequency is called next time
    }
    
    // IG02610's unique characteristic: Mix lowpass with slight highpass for notch behavior
    // Based on analysis showing "half lowpass, half highpass mixed to create notch"
    float y = output;
    
    // Add subtle notch characteristic when cutoff is lowered (below ~500Hz)
    if (cutoff < 500.0f && resonance > 0.5f) {
        const float notchAmount = (500.0f - cutoff) / 500.0f; // 0.0 to 1.0 as cutoff decreases
        const float highpassComponent = input - output; // Simple highpass approximation
        
        // Mix slight highpass to create notch effect (very subtle)
        y = output + highpassComponent * notchAmount * 0.1f;
    }
    
    // Enhanced OTA-based nonlinear distortion characteristics
    // Apply across all resonance ranges with varying intensity
    {
        // Stage 1: Subtle even harmonics for low resonance (OTA input stage)
        float lightDistortion = 0.0f;
        if (resonance <= 0.4f) {
            const float lightAmount = resonance / 0.4f; // 0.0 to 1.0
            const float evenHarmonics = y * y * y * 0.05f; // Cubic for even harmonics
            lightDistortion = evenHarmonics * lightAmount * 0.3f;
        }
        
        // Stage 2: Balanced distortion for medium resonance
        float mediumDistortion = 0.0f;
        if (resonance > 0.4f && resonance <= 0.7f) {
            const float medAmount = (resonance - 0.4f) / 0.3f; // 0.0 to 1.0
            
            // Frequency-dependent drive (low frequencies get more distortion)
            const float freqFactor = cutoff < 1000.0f ? 
                1.2f - (cutoff / 1000.0f) * 0.4f : 0.8f;
            
            const float drivenSignal = y * (1.0f + medAmount * 0.15f * freqFactor);
            const float balancedSat = accurateTanh(drivenSignal * 0.4f);
            mediumDistortion = balancedSat * medAmount * 0.4f;
        }
        
        // Stage 3: Strong distortion for high resonance (enhanced from original)
        float strongDistortion = 0.0f;
        if (resonance > 0.7f) {
            const float strongAmount = (resonance - 0.7f) / 0.1f; // 0.0 to 1.0
            
            // Input level dependent drive (larger signals get more distortion)
            const float inputLevel = std::abs(y);
            const float levelFactor = 1.0f + inputLevel * 0.5f;
            
            // Frequency-dependent saturation characteristics
            const float freqSaturation = cutoff < 500.0f ? 1.3f : 
                (cutoff > 5000.0f ? 0.7f : 1.0f);
            
            const float heavilyDriven = y * levelFactor * (1.0f + strongAmount * 0.25f);
            const float primarySat = accurateTanh(heavilyDriven * 0.5f * freqSaturation);
            
            // Add asymmetric clipping for OTA-like behavior
            const float asymmetric = y > 0.0f ? 
                accurateTanh(y * 1.2f) : accurateTanh(y * 0.8f);
            
            strongDistortion = (primarySat * 0.7f + asymmetric * 0.3f) * strongAmount * 0.5f;
        }
        
        // Combine all distortion stages
        const float totalDistortion = lightDistortion + mediumDistortion + strongDistortion;
        
        // Apply distortion with smooth blending
        const float distortionAmount = resonance * 0.6f; // Overall distortion scaling
        y = y * (1.0f - distortionAmount) + totalDistortion * distortionAmount;
        
        // Final gentle limiting to prevent extreme values
        y = juce::jlimit(-1.5f, 1.5f, y);
    }
    
    // Apply output stage processing and reduce volume to 50%
    return processOutputStage(y);// * 0.5f;
}

void IG02610LPF::updateCoefficients()
{
    if (sampleRate <= 0.0f) return; // Prevent division by zero
    
    // Limit cutoff frequency range
    cutoff = juce::jlimit(20.0f, 20000.0f, cutoff);
    
    // Limit resonance range (max 0.8f to prevent extreme resonance)
    resonance = juce::jlimit(0.1f, 0.8f, resonance);
    
    // Use standard biquad lowpass filter design
    const float frequency = cutoff / sampleRate;
    const float omega = 2.0f * juce::MathConstants<float>::pi * frequency;
    const float sin_omega = std::sin(omega);
    const float cos_omega = std::cos(omega);
    
    // Q factor - more reasonable range
    const float Q = 0.5f + resonance * 4.5f; // Range from 0.5 to 5.0
    
    const float alpha = sin_omega / (2.0f * Q);
    
    // Standard lowpass biquad coefficients
    const float norm = 1.0f / (1.0f + alpha);
    
    b0 = ((1.0f - cos_omega) * 0.5f) * norm;
    b1 = (1.0f - cos_omega) * norm;
    b2 = b0;
    a1 = (-2.0f * cos_omega) * norm;
    a2 = (1.0f - alpha) * norm;
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
