#include "CS01Synth/IG02610.h"

#include <JuceHeader.h>

#include <cmath>

void IG02610::reset() {
    z1 = z2 = 0;
    inputLevelSmoothed = 0;
}
void IG02610::prepare(double rate) {
    sampleRate = static_cast<float>(rate);
    levelSmoothing =
        static_cast<float>(std::pow(static_cast<double>(LEVEL_SMOOTHING), 44100.0 / rate));
    updateCoefficients();
}
float IG02610::accurateTanh(float x) {
    return std::tanh(x);
}

void IG02610::setCutoffFrequency(float newCutoff) {
    cutoff = juce::jlimit(20.0f, 20000.0f, newCutoff);
    updateCoefficients();
}

void IG02610::setResonance(float newResonance) {
    // IG02610 resonance range (limit max to 0.8f)
    resonance = juce::jlimit(0.1f, 0.8f, newResonance);
    updateCoefficients();
}

float IG02610::processSample(float sample) {
    // Input safety clamp (hard clipping, not the nonlinear stage model).
    sample = sample > 1.0f ? 1.0f : (sample < -1.0f ? -1.0f : sample);

    // Track input level with envelope follower for OTA input level dependency
    float inputLevel = std::abs(sample);
    inputLevelSmoothed = inputLevelSmoothed * levelSmoothing + inputLevel * (1.0f - levelSmoothing);

    // Apply OTA input level dependent cutoff modulation
    // Large signals make cutoff slightly higher (brighter), small signals make it lower (darker)
    float levelModulation = (inputLevelSmoothed - 0.5f) * INPUT_LEVEL_INFLUENCE;
    float dynamicCutoff = cutoff * (1.0f + levelModulation);

    // Compute the effective coefficients every sample, including near-zero
    // modulation. Never leave coefficients from a previous modulation value.
    float originalCutoff = cutoff;
    cutoff = juce::jlimit(20.0f, 20000.0f, dynamicCutoff);
    updateCoefficients();
    cutoff = originalCutoff;

    // Standard 2nd order filter processing (direct form II transposed)
    const float input = sample;
    const double output = b0 * input + z1;

    // Update filter state variables
    z1 = b1 * input - a1 * output + z2;
    z2 = b2 * input - a2 * output;


    // Use the lowpass output without an unverified dry-input/notch blend.
    // The available CS-01 schematic does not establish such a bypass path.
    float y = output;

    // Enhanced OTA-based nonlinear distortion characteristics
    // Apply across all resonance ranges with varying intensity
    {
        // Empirical odd-harmonic coloration; not an OTA circuit reconstruction.
        float lightDistortion = 0.0f;
        {
            const float lightAmount = juce::jlimit(0.0f, 1.0f, resonance / 0.4f);
            const float oddHarmonics = y * y * y * 0.05f;
            lightDistortion = oddHarmonics * lightAmount * 0.3f;
        }

        // Stage 2: Balanced distortion for medium resonance
        float mediumDistortion = 0.0f;
        if (resonance > 0.4f) {
            const float medAmount = juce::jlimit(0.0f, 1.0f, (resonance - 0.4f) / 0.3f);

            // Frequency-dependent drive (low frequencies get more distortion)
            const float freqFactor = cutoff < 1000.0f ? 1.2f - (cutoff / 1000.0f) * 0.4f : 0.8f;

            const float drivenSignal = y * (1.0f + medAmount * 0.15f * freqFactor);
            const float balancedSat = accurateTanh(drivenSignal * 0.4f);
            mediumDistortion = balancedSat * medAmount * 0.4f;
        }

        // Stage 3: Strong distortion for high resonance (enhanced from original)
        float strongDistortion = 0.0f;
        if (resonance > 0.7f) {
            const float strongAmount = (resonance - 0.7f) / 0.1f;  // 0.0 to 1.0

            // Input level dependent drive (larger signals get more distortion)
            const float inputLevel = std::abs(y);
            const float levelFactor = 1.0f + inputLevel * 0.5f;

            // Frequency-dependent saturation characteristics
            const float freqSaturation = 1.3f - 0.6f *
                juce::jlimit(0.0f, 1.0f, (cutoff - 500.0f) / 4500.0f);

            const float heavilyDriven = y * levelFactor * (1.0f + strongAmount * 0.25f);
            const float primarySat = accurateTanh(heavilyDriven * 0.5f * freqSaturation);

            // Add asymmetric clipping for OTA-like behavior
            const float asymmetric = y > 0.0f ? accurateTanh(y * 1.2f) : accurateTanh(y * 0.8f);

            strongDistortion = (primarySat * 0.7f + asymmetric * 0.3f) * strongAmount * 0.5f;
        }

        // Combine all distortion stages
        const float mediumBlend = juce::jlimit(0.0f, 1.0f, (resonance - 0.4f) / 0.3f);
        const float strongBlend = juce::jlimit(0.0f, 1.0f, (resonance - 0.7f) / 0.1f);
        const float lowerDistortion = lightDistortion * (1.0f - mediumBlend) + mediumDistortion;
        const float totalDistortion = lowerDistortion * (1.0f - strongBlend) + strongDistortion;

        // Apply distortion with smooth blending
        const float distortionAmount = resonance * 0.6f;  // Overall distortion scaling
        y = y * (1.0f - distortionAmount) + totalDistortion * distortionAmount;

        // Final gentle limiting to prevent extreme values
        y = juce::jlimit(-1.5f, 1.5f, y);
    }

    // Output gain is handled by the surrounding circuit model.
    return y;
}

void IG02610::updateCoefficients() {
    if (sampleRate <= 0.0f)
        return;  // Prevent division by zero

    // Limit cutoff frequency range
    cutoff = juce::jlimit(20.0f, 20000.0f, cutoff);

    // Limit resonance range (max 0.8f to prevent extreme resonance)
    resonance = juce::jlimit(0.1f, 0.8f, resonance);

    // Use standard biquad lowpass filter design
    // Keep coefficients and recursive state in double precision: low cutoffs
    // at the shared internal rate otherwise suffer severe cancellation.
    const double frequency = std::min(static_cast<double>(cutoff), sampleRate * 0.45) / sampleRate;
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency;
    const double sin_omega = std::sin(omega);
    const double cos_omega = std::cos(omega);

    // Q factor - more reasonable range
    const double Q = 0.5 + static_cast<double>(resonance) * 4.5;

    const double alpha = sin_omega / (2.0 * Q);

    // Standard lowpass biquad coefficients
    const double norm = 1.0 / (1.0 + alpha);

    b0 = std::pow(std::sin(omega * 0.5), 2.0) * norm;
    b1 = 2.0 * b0;
    b2 = b0;
    a1 = (-2.0f * cos_omega) * norm;
    a2 = (1.0f - alpha) * norm;
}
