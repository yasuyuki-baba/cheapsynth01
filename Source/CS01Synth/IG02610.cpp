#include "IG02610.h"
void IG02610::reset() { z1 = z2 = 0; inputLevelSmoothed = 0; }
void IG02610::prepare(double rate) {
    sampleRate = static_cast<float>(rate);
    levelSmoothing = static_cast<float>(std::pow(static_cast<double>(LEVEL_SMOOTHING), 44100.0 / rate));
    updateCoefficients();
}
float IG02610::accurateTanh(float x) {
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
    // Soft limiting to prevent overload (gentler than hard clipping)
    sample = sample > 1.0f ? 1.0f : (sample < -1.0f ? -1.0f : sample);

    // Track input level with envelope follower for OTA input level dependency
    float inputLevel = std::abs(sample);
    inputLevelSmoothed =
        inputLevelSmoothed * levelSmoothing + inputLevel * (1.0f - levelSmoothing);

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
    const double output = b0 * input + z1;

    // Update filter state variables
    z1 = b1 * input - a1 * output + z2;
    z2 = b2 * input - a2 * output;

    // Restore original cutoff if it was temporarily changed
    if (needsUpdate) {
        cutoff = originalCutoff;
        // Note: We don't update coefficients back here for performance,
        // they will be updated when setCutoffFrequency is called next time
    }

    // Use the lowpass output without an unverified dry-input/notch blend.
    // The available CS-01 schematic does not establish such a bypass path.
    float y = output;

    // Enhanced OTA-based nonlinear distortion characteristics
    // Apply across all resonance ranges with varying intensity
    {
        // Stage 1: Subtle even harmonics for low resonance (OTA input stage)
        float lightDistortion = 0.0f;
        if (resonance <= 0.4f) {
            const float lightAmount = resonance / 0.4f;     // 0.0 to 1.0
            const float evenHarmonics = y * y * y * 0.05f;  // Cubic for even harmonics
            lightDistortion = evenHarmonics * lightAmount * 0.3f;
        }

        // Stage 2: Balanced distortion for medium resonance
        float mediumDistortion = 0.0f;
        if (resonance > 0.4f && resonance <= 0.7f) {
            const float medAmount = (resonance - 0.4f) / 0.3f;  // 0.0 to 1.0

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
            const float freqSaturation = cutoff < 500.0f ? 1.3f : (cutoff > 5000.0f ? 0.7f : 1.0f);

            const float heavilyDriven = y * levelFactor * (1.0f + strongAmount * 0.25f);
            const float primarySat = accurateTanh(heavilyDriven * 0.5f * freqSaturation);

            // Add asymmetric clipping for OTA-like behavior
            const float asymmetric = y > 0.0f ? accurateTanh(y * 1.2f) : accurateTanh(y * 0.8f);

            strongDistortion = (primarySat * 0.7f + asymmetric * 0.3f) * strongAmount * 0.5f;
        }

        // Combine all distortion stages
        const float totalDistortion = lightDistortion + mediumDistortion + strongDistortion;

        // Apply distortion with smooth blending
        const float distortionAmount = resonance * 0.6f;  // Overall distortion scaling
        y = y * (1.0f - distortionAmount) + totalDistortion * distortionAmount;

        // Final gentle limiting to prevent extreme values
        y = juce::jlimit(-1.5f, 1.5f, y);
    }

    // Apply output stage processing and reduce volume to 50%
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
    const double frequency = static_cast<double>(cutoff) / sampleRate;
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
