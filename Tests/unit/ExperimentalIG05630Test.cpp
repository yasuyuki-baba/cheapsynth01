#include <JuceHeader.h>

#include "CS01Synth/CS01IIVCFCircuit.h"
#include "CS01Synth/ExperimentalIG05630.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
constexpr int characterSamples = 32768;

std::vector<float> render(double sampleRate, float cutoff, float resonance,
                          double toneHz, float amplitude) {
    CS01IIVCFCircuit circuit;
    circuit.prepare(sampleRate);
    circuit.setCutoffFrequency(cutoff);
    circuit.setResonance(resonance);
    std::vector<float> output(characterSamples);
    for (int i = 0; i < characterSamples; ++i) {
        const float input = amplitude *
                            static_cast<float>(std::sin(2.0 * juce::MathConstants<double>::pi *
                                                        toneHz * i / sampleRate));
        output[i] = circuit.processSample(0, input);
    }
    return output;
}

double amplitudeAt(const std::vector<float>& signal, double sampleRate, double frequency,
                   int harmonic = 1) {
    double sine = 0.0;
    double cosine = 0.0;
    for (int i = characterSamples / 2; i < characterSamples; ++i) {
        const double phase = 2.0 * juce::MathConstants<double>::pi * frequency * harmonic * i /
                             sampleRate;
        sine += signal[i] * std::sin(phase);
        cosine += signal[i] * std::cos(phase);
    }
    return 2.0 * std::hypot(sine, cosine) / (characterSamples / 2);
}
}  // namespace

TEST(ExperimentalIG05630Test, RemainsFiniteAndBoundedUnderFastModulation) {
    for (double hostRate : {44100.0, 48000.0, 96000.0}) {
        ExperimentalIG05630 filter;
        filter.prepare(hostRate * 4.0);
        for (int i = 0; i < 100000; ++i) {
            filter.setCutoffFrequency((i & 1) ? 20.0f : 20000.0f);
            filter.setResonance((i % 17) / 16.0f);
            const float input = 4.0f * std::sin(i * 0.19f);
            const float output = filter.processSample(input);
            ASSERT_TRUE(std::isfinite(output));
            EXPECT_LE(std::abs(output),
                      ExperimentalIG05630::EmpiricalParameters::maximumOutput);
        }
    }
}

TEST(ExperimentalIG05630Test, SustainsBoundedResonanceAfterAnImpulse) {
    ExperimentalIG05630 filter;
    filter.prepare(192000.0);
    filter.setCutoffFrequency(1000.0f);
    filter.setResonance(1.0f);

    double latePower = 0.0;
    constexpr int sampleCount = 192000;
    for (int i = 0; i < sampleCount; ++i) {
        const float output = filter.processSample(i == 0 ? 0.5f : 0.0f);
        ASSERT_TRUE(std::isfinite(output));
        EXPECT_LE(std::abs(output),
                  ExperimentalIG05630::EmpiricalParameters::maximumOutput);
        if (i >= sampleCount * 3 / 4)
            latePower += static_cast<double>(output) * output;
    }
    const double lateRms = std::sqrt(latePower / (sampleCount / 4));
    EXPECT_GT(lateRms, 1.0e-5);
}

TEST(ExperimentalIG05630Test, RenderingIsDeterministicAcrossBlockPartitions) {
    constexpr int sampleCount = 8192;
    std::vector<float> whole(sampleCount), partitioned(sampleCount);
    ExperimentalIG05630 a, b;
    a.prepare(192000.0);
    b.prepare(192000.0);
    for (int i = 0; i < sampleCount; ++i) {
        const float cutoff = 80.0f + (i % 2000) * 10.0f;
        const float input = 0.4f * std::sin(i * 0.11f);
        a.setCutoffFrequency(cutoff);
        a.setResonance(0.8f);
        whole[i] = a.processSample(input);
    }
    for (int block = 0; block < sampleCount; block += 53) {
        for (int i = block; i < std::min(block + 53, sampleCount); ++i) {
            const float cutoff = 80.0f + (i % 2000) * 10.0f;
            const float input = 0.4f * std::sin(i * 0.11f);
            b.setCutoffFrequency(cutoff);
            b.setResonance(0.8f);
            partitioned[i] = b.processSample(input);
        }
    }
    EXPECT_EQ(whole, partitioned);
}

TEST(ExperimentalIG05630Test, ZeroResonanceHasFourPoleLowpassResponse) {
    constexpr double rate = 192000.0;
    const auto cutoffSignal = render(rate, 1000.0f, 0.0f, 1000.0, 0.01f);
    const auto lowerSignal = render(rate, 1000.0f, 0.0f, 2000.0, 0.01f);
    const auto upperSignal = render(rate, 1000.0f, 0.0f, 4000.0, 0.01f);
    const double atCutoff = amplitudeAt(cutoffSignal, rate, 1000.0) / 0.01;
    const double lower = amplitudeAt(lowerSignal, rate, 2000.0);
    const double upper = amplitudeAt(upperSignal, rate, 4000.0);
    EXPECT_NEAR(atCutoff, 1.0 / std::sqrt(2.0), 0.02);
    EXPECT_LT(upper / lower, 0.08);  // Four-pole asymptote: roughly 24 dB/octave.
}

TEST(ExperimentalIG05630Test, Observation_ExperimentalCharacterization) {
    for (double hostRate : {44100.0, 48000.0, 96000.0}) {
        const double rate = hostRate * 4.0;
        for (float cutoff : {80.0f, 1000.0f, 10000.0f}) {
            for (float resonance : {0.0f, 0.4f, 0.8f, 1.0f}) {
                for (double ratio : {0.5, 1.0, 2.0}) {
                    const double frequency = cutoff * ratio;
                    if (frequency >= rate * 0.4)
                        continue;
                    {
                        const auto signal = render(rate, cutoff, resonance, frequency,
                                                   0.01f);
                        const double gain = 20.0 * std::log10(
                            std::max(amplitudeAt(signal, rate, frequency) / 0.01, 1.0e-15));
                        const bool finite = std::all_of(signal.begin(), signal.end(),
                                                        [](float value) { return std::isfinite(value); });
                        std::cout << "IG05630_RESPONSE,experimental"
                                  << ',' << hostRate << ',' << rate << ',' << cutoff << ','
                                  << resonance << ',' << frequency << ',' << gain << ','
                                  << (finite ? "finite" : "nonfinite") << '\n';
                    }
                }
            }
        }

        for (float resonance : {0.4f, 0.8f, 1.0f}) {
            for (float inputPeak : {0.05f, 0.5f}) {
                constexpr float cutoff = 1000.0f;
                const int cycles = juce::jmax(
                    8, juce::roundToInt(440.0 * characterSamples / rate));
                const double frequency = cycles * rate / characterSamples;
                {
                    const auto signal = render(rate, cutoff, resonance, frequency,
                                               inputPeak);
                    const double fundamental = amplitudeAt(signal, rate, frequency);
                    std::cout << "IG05630_HARMONICS,experimental"
                              << ',' << hostRate << ',' << rate << ',' << cutoff << ',' << resonance
                              << ',' << inputPeak << ',' << frequency;
                    for (int harmonic = 1; harmonic <= 8; ++harmonic) {
                        const double level = amplitudeAt(signal, rate, frequency, harmonic);
                        const double relativeDb = harmonic == 1 ? 0.0 : 20.0 * std::log10(
                            std::max(level / std::max(fundamental, 1.0e-30), 1.0e-15));
                        std::cout << ',' << relativeDb;
                    }
                    std::cout << '\n';
                }
            }
        }

        constexpr int sampleCount = 65536;
        for (bool modulated : {false, true}) {
            std::vector<float> source(sampleCount);
            for (int i = 0; i < sampleCount; ++i)
                source[i] = 0.2f * static_cast<float>(std::sin(
                    i * 2.0 * juce::MathConstants<double>::pi * 220.0 / rate));
            {
                auto input = source;
                CS01IIVCFCircuit circuit;
                circuit.prepare(rate);
                const auto start = std::chrono::steady_clock::now();
                bool finite = true;
                for (int i = 0; i < sampleCount; ++i) {
                    const float cutoff = modulated
                        ? 1000.0f * (1.0f + 0.8f * std::sin(i * 2.0f * juce::MathConstants<float>::pi /
                                                               8192.0f))
                        : 1000.0f;
                    circuit.setCutoffFrequency(cutoff);
                    circuit.setResonance(0.8f);
                    input[i] = circuit.processSample(0, input[i]);
                    finite = finite && std::isfinite(input[i]);
                }
                const double elapsed = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - start).count();
                std::cout << "IG05630_BENCH,experimental"
                          << ',' << hostRate << ',' << rate << ','
                          << (modulated ? "modulated" : "static") << ',' << sampleCount << ','
                          << elapsed << ',' << elapsed * 1.0e9 / sampleCount << ','
                          << (finite ? "finite" : "nonfinite") << '\n';
            }
        }
    }
}
