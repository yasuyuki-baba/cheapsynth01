#include "CS01Synth/ExperimentalOriginalVCF.h"
#include "CS01Synth/CS01VCFCircuit.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>
#include <vector>

namespace {
constexpr int characterizationSamples = 32768;

std::vector<float> renderFilter(double sampleRate, float cutoffHz, float resonance,
                                double frequencyHz, float amplitude) {
    CS01VCFCircuit filter;
    filter.prepare(sampleRate);
    std::vector<float> audio(characterizationSamples);
    std::vector<float> cutoff(characterizationSamples, cutoffHz);
    for (int i = 0; i < characterizationSamples; ++i) {
        const double phase = juce::MathConstants<double>::twoPi * frequencyHz * i / sampleRate;
        audio[i] = amplitude * static_cast<float>(std::sin(phase));
    }
    filter.processBlock(audio.data(), characterizationSamples, cutoff.data(), resonance);
    return audio;
}

double projectedAmplitude(const std::vector<float>& audio, double sampleRate, double frequencyHz,
                          int harmonic = 1) {
    double sine = 0.0;
    double cosine = 0.0;
    const int begin = characterizationSamples / 2;
    for (int i = begin; i < characterizationSamples; ++i) {
        const double phase =
            juce::MathConstants<double>::twoPi * frequencyHz * harmonic * i / sampleRate;
        sine += audio[i] * std::sin(phase);
        cosine += audio[i] * std::cos(phase);
    }
    return 2.0 * std::hypot(sine, cosine) / (characterizationSamples - begin);
}
}  // namespace

TEST(ExperimentalOriginalVCFTest, RemainsFiniteAndBoundedDuringFastModulation) {
    for (const double rate : {44100.0, 48000.0, 96000.0}) {
        ExperimentalOriginalVCF filter;
        filter.prepare(rate * 4.0);  // Match the project's 4x internal filter rate.
        for (int i = 0; i < 200000; ++i) {
            const float cutoff = (i & 1) ? 20.0f : 20000.0f;
            const float input = 4.0f * std::sin(static_cast<float>(i) * 0.17f);
            const float output = filter.processSample(input, cutoff, 1.0f);
            ASSERT_TRUE(std::isfinite(output));
            EXPECT_LE(std::abs(output),
                      ExperimentalOriginalVCF::EmpiricalParameters::maximumOutput);
        }
    }
}

TEST(ExperimentalOriginalVCFTest, RenderingIsDeterministicAndBlockPartitionIndependent) {
    constexpr int count = 4096;
    std::vector<float> whole(count), split(count);
    ExperimentalOriginalVCF a, b;
    a.prepare(192000.0);
    b.prepare(192000.0);
    for (int i = 0; i < count; ++i) {
        const float input = std::sin(i * 0.071f);
        const float cutoff = 100.0f + static_cast<float>(i % 1200) * 12.0f;
        whole[i] = a.processSample(input, cutoff, 0.8f);
    }
    for (int block = 0; block < count; block += 37) {
        for (int i = block; i < juce::jmin(block + 37, count); ++i) {
            const float input = std::sin(i * 0.071f);
            const float cutoff = 100.0f + static_cast<float>(i % 1200) * 12.0f;
            split[i] = b.processSample(input, cutoff, 0.8f);
        }
    }
    EXPECT_EQ(whole, split);
}

TEST(ExperimentalOriginalVCFTest, BehavioralSaturationDependsOnSignalLevel) {
    const auto low = renderFilter(192000.0, 1000.0f, 0.7f, 440.0, 0.05f);
    const auto high = renderFilter(192000.0, 1000.0f, 0.7f, 440.0, 0.5f);
    double normalizedDifferencePower = 0.0;
    const int begin = characterizationSamples / 2;
    for (int i = begin; i < characterizationSamples; ++i) {
        const double difference = high[i] / 10.0 - low[i];
        normalizedDifferencePower += difference * difference;
    }
    normalizedDifferencePower /= characterizationSamples - begin;
    // Check that the explicitly nonlinear behavioral stages are active without
    // encoding a desired tone or any hardware THD target.
    EXPECT_GT(normalizedDifferencePower, 1.0e-8);
}

TEST(ExperimentalOriginalVCFTest, Observation_CompiledCoreCpuComparison) {
    constexpr int sampleCount = 65536;
    for (const double hostRate : {44100.0, 48000.0, 96000.0}) {
        const double coreRate = hostRate * 4.0;
        for (const bool modulated : {false, true}) {
            std::vector<float> source(sampleCount), cutoff(sampleCount);
            for (int i = 0; i < sampleCount; ++i) {
                const double phase = i * 2.0 * juce::MathConstants<double>::pi * 220.0 / coreRate;
                source[i] = 0.2f * static_cast<float>(std::sin(phase));
                const float modulation =
                    0.25f * std::sin(i * 2.0f * juce::MathConstants<float>::pi / 4096.0f);
                cutoff[i] = modulated ? 1000.0f * (1.0f + modulation) : 1000.0f;
            }

            {
                CS01VCFCircuit filter;
                filter.prepare(coreRate);
                auto audio = source;
                const auto start = std::chrono::steady_clock::now();
                filter.processBlock(audio.data(), sampleCount, cutoff.data(), 0.7f);
                const auto elapsed =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                const bool finite = std::all_of(audio.begin(), audio.end(),
                                                [](float x) { return std::isfinite(x); });
                std::cout << "ORIGINAL_VCF_CPP_BENCH,"
                          << "experimental_tpt" << ',' << hostRate << ',' << coreRate << ','
                          << (modulated ? "modulated" : "static") << ',' << sampleCount << ','
                          << elapsed << ',' << (elapsed * 1e9 / sampleCount) << ','
                          << (finite ? "finite" : "nonfinite") << '\n';
            }
        }
    }
}

TEST(ExperimentalOriginalVCFTest, Observation_CompiledCoreResponseAndHarmonics) {
    for (const double hostRate : {44100.0, 48000.0, 96000.0}) {
        const double coreRate = hostRate * 4.0;
        for (const float cutoffHz : {80.0f, 1000.0f, 10000.0f}) {
            for (const float resonance : {0.2f, 0.7f}) {
                for (const double cutoffRatio : {0.5, 1.0, 2.0}) {
                    const double requestedHz = cutoffHz * cutoffRatio;
                    const int cycles = juce::jmax(
                        8, juce::roundToInt(requestedHz * characterizationSamples / coreRate));
                    const double frequencyHz = cycles * coreRate / characterizationSamples;
                    if (frequencyHz >= coreRate * 0.45)
                        continue;
                    {
                        const auto audio =
                            renderFilter(coreRate, cutoffHz, resonance, frequencyHz, 0.01f);
                        const double output = projectedAmplitude(audio, coreRate, frequencyHz);
                        const double gainDb = 20.0 * std::log10(std::max(output / 0.01, 1.0e-15));
                        std::cout << "ORIGINAL_VCF_CPP_RESPONSE,"
                                  << "experimental_tpt" << ',' << hostRate << ',' << coreRate << ','
                                  << cutoffHz << ',' << resonance << ',' << frequencyHz << ','
                                  << gainDb << ','
                                  << (std::isfinite(gainDb) ? "finite" : "nonfinite") << '\n';
                    }
                }
            }
        }

        for (const float amplitude : {0.05f, 0.5f}) {
            constexpr float cutoffHz = 1000.0f;
            constexpr float resonance = 0.7f;
            const int cycles = juce::roundToInt(440.0 * characterizationSamples / coreRate);
            const double frequencyHz = cycles * coreRate / characterizationSamples;
            {
                const auto audio =
                    renderFilter(coreRate, cutoffHz, resonance, frequencyHz, amplitude);
                const double fundamental = projectedAmplitude(audio, coreRate, frequencyHz);
                std::cout << "ORIGINAL_VCF_CPP_HARMONICS,"
                          << "experimental_tpt" << ',' << hostRate << ',' << coreRate << ','
                          << cutoffHz << ',' << resonance << ',' << amplitude << ',' << frequencyHz;
                for (int harmonic = 1; harmonic <= 8; ++harmonic) {
                    const double level = projectedAmplitude(audio, coreRate, frequencyHz, harmonic);
                    const double relativeDb =
                        harmonic == 1
                            ? 0.0
                            : 20.0 * std::log10(
                                         std::max(level / std::max(fundamental, 1.0e-30), 1.0e-15));
                    std::cout << ',' << relativeDb;
                }
                std::cout << '\n';
            }
        }
    }
}
