#include "cs01synth/OriginalVCFProcessor.h"
#include "cs01synth/ModernVCFProcessor.h"
#include "cs01synth/VCAProcessor.h"
#include "cs01synth/IG02610BehavioralModel.h"
#include "cs01synth/IG05630BehavioralModel.h"
#include "../reference/BehavioralBeforeRefactor.h"
#include <gtest/gtest.h>
#include <array>
#include <bit>
#include <complex>
#include <numbers>
#include <vector>
#include <limits>
#include <chrono>
#include <iostream>

namespace {
using cs01::Param;
constexpr double tau = 2 * std::numbers::pi;
float stimulus(int i) {
    if (i < 128 || i >= 6000)
        return 0;
    if (i == 128)
        return 3;
    return .1f + (i < 2000 ? .01f : 2.f) * static_cast<float>(std::sin(i * .137));
}
double power(const std::vector<float>& values) {
    double sum = 0;
    for (double value : values)
        sum += value * value;
    return sum;
}
template <class Process>
double gain(double rate, double hz, Process process) {
    double sum = 0;
    for (int i = 0; i < static_cast<int>(2 * rate); ++i) {
        float y = process(static_cast<float>(.01 * std::sin(tau * hz * i / rate)));
        if (!std::isfinite(y))
            return std::numeric_limits<double>::quiet_NaN();
        if (i >= rate)
            sum += static_cast<double>(y) * y;
    }
    return std::sqrt(sum / rate) / (.01 / std::sqrt(2.));
}
double amplitude(const std::vector<float>& values, double rate, double hz) {
    double sine = 0, cosine = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        sine += values[i] * std::sin(tau * hz * i / rate);
        cosine += values[i] * std::cos(tau * hz * i / rate);
    }
    return 2 * std::hypot(sine, cosine) / values.size();
}
std::vector<float> original(float egDepth, float eg, float modDepth, float lfo,
                            float breathDepth = 0, float breath = 0) {
    cs01::ParameterState state;
    state.set(Param::Cutoff, 1000);
    state.set(Param::Resonance, 0);
    state.set(Param::VcfEgDepth, egDepth);
    state.set(Param::ModDepth, modDepth);
    state.set(Param::BreathVcf, breathDepth);
    state.set(Param::BreathInput, breath);
    OriginalVCFProcessor filter(state);
    filter.prepare(48000);
    std::vector<float> values;
    for (int i = 0; i < 96000; ++i) {
        float y = filter.processSample(static_cast<float>(.001 * std::sin(tau * 10000 * i / 48000)),
                                       eg, lfo);
        EXPECT_TRUE(std::isfinite(y));
        if (i >= 48000)
            values.push_back(y);
    }
    return values;
}
}  // namespace

TEST(SignalPathTest, BehavioralModelsMatchFrozenArithmeticIncludingSafetyExactly) {
    for (double rate : {44100., 48000., 96000., 192000.}) {
        IG02610BehavioralModel original;
        BeforeIG02610 beforeOriginal;
        IG05630BehavioralModel modern;
        BeforeIG05630 beforeModern;
        original.prepare(rate);
        beforeOriginal.prepare(rate);
        modern.prepare(rate);
        beforeModern.prepare(rate);
        for (int pass = 0; pass < 2; ++pass) {
            for (int i = 0; i < 8192; ++i) {
                float cutoff = 20.f + static_cast<float>(i * 37 % 19980);
                float resonance = static_cast<float>(i % 11) / 10.f;
                float input = i == 10   ? std::numeric_limits<float>::quiet_NaN()
                              : i == 20 ? std::numeric_limits<float>::infinity()
                              : i == 30 ? std::numeric_limits<float>::max()
                                        : stimulus(i);
                // Independent arithmetic references include the intentional 0b342641 sound edit.
                modern.setCutoffFrequency(cutoff);
                beforeModern.setCutoffFrequency(cutoff);
                modern.setResonance(resonance);
                beforeModern.setResonance(resonance);
                EXPECT_EQ(std::bit_cast<uint32_t>(modern.processSample(input)),
                          std::bit_cast<uint32_t>(beforeModern.processSample(input)))
                    << i;
                EXPECT_EQ(
                    std::bit_cast<uint32_t>(original.processSample(input, cutoff, resonance)),
                    std::bit_cast<uint32_t>(beforeOriginal.processSample(input, cutoff, resonance)))
                    << i;
            }
            original.reset();
            beforeOriginal.reset();
            modern.reset();
            beforeModern.reset();
        }
    }
}
TEST(SignalPathTest, OriginalBreathDepthIndependenceAndControlDirection) {
    const auto baseline = original(0, 0, 0, 0);
    EXPECT_EQ(baseline, original(0, 0, 0, 0, 0, 1));
    EXPECT_EQ(baseline, original(0, 0, 0, 0, 1, 0));
    double previous = 0;
    for (float breath : {0.f, .25f, .5f, .75f, 1.f}) {
        double current = power(original(0, 0, 0, 0, 1, breath));
        EXPECT_GT(current, previous);
        previous = current;
    }
}
TEST(SignalPathTest, OriginalEgLfoDepthIndependenceAndDirection) {
    const auto baseline = original(0, 0, 0, 0);
    EXPECT_EQ(baseline, original(0, 1, 0, 1));
    EXPECT_EQ(baseline, original(1, 0, 1, 0));
    ASSERT_GT(power(baseline), 0);
    EXPECT_GT(power(original(1, 1, 0, 0)), power(baseline));
    EXPECT_LT(power(original(0, 0, 1, -1)), power(baseline));
    EXPECT_GT(power(original(0, 0, 1, 1)), power(baseline));
}
TEST(SignalPathTest, OriginalCombinedModulationMatchesIndependentSemitoneCutoff) {
    for (double rate : {44100., 48000., 96000., 192000.}) {
        cs01::ParameterState state;
        state.set(Param::Cutoff, 730);
        state.set(Param::VcfEgDepth, .3f);
        state.set(Param::ModDepth, .4f);
        state.set(Param::BreathInput, .6f);
        state.set(Param::BreathVcf, .7f);
        state.set(Param::Resonance, 1);
        OriginalVCFProcessor filter(state);
        filter.prepare(rate);
        CS01VCFCircuit reference;
        reference.prepare(rate);
        reference.setResonance(.7f);
        for (int i = 0; i < 8192; ++i) {
            float eg = static_cast<float>(i % 101) / 100;
            float lfo = std::sin(i * .017f);
            // Read the independently configured inputs at runtime: constant folding
            // of .6f * .7f * 24.f differs from runtime fused arithmetic on ARM.
            const float egDepth = state.get(Param::VcfEgDepth);
            const float modDepth = state.get(Param::ModDepth);
            const float breath = state.get(Param::BreathInput);
            const float breathDepth = state.get(Param::BreathVcf);
            float semitones =
                eg * egDepth * 36.f + lfo * modDepth * 24.f + breath * breathDepth * 24.f;
            reference.setCutoffFrequency(
                std::clamp(730.f * std::exp2(semitones / 12.f), 20.f, 20000.f));
            EXPECT_FLOAT_EQ(filter.processSample(stimulus(i), eg, lfo),
                            reference.processSample(0, stimulus(i)));
        }
    }
}
TEST(SignalPathTest, VcaBreathEndpointsAndIndependentEgDepth) {
    const auto measure = [](float eg, float depth, float breath, float egDepth = 1.f) {
        cs01::ParameterState state;
        state.set(Param::Volume, 1);
        state.set(Param::VcaEgDepth, egDepth);
        state.set(Param::BreathVca, depth);
        state.set(Param::BreathInput, breath);
        VCAProcessor vca(state);
        vca.prepare(48000);
        return gain(48000, 1000, [&](float input) { return vca.processSample(input, eg); });
    };
    double bypass = measure(1, 0, 0);
    ASSERT_GT(bypass, 0);
    EXPECT_NEAR(measure(1, 0, 1), bypass, 1e-10);
    EXPECT_EQ(measure(1, 1, 0), 0);
    EXPECT_NEAR(measure(1, 1, 1), bypass, 1e-10);
    EXPECT_EQ(measure(0, 1, 1), 0);
    double previous = 0;
    for (float breath : {.25f, .5f, .75f, 1.f}) {
        double current = measure(1, 1, breath);
        EXPECT_GT(current, previous);
        previous = current;
        double egBypass = measure(0, 1, breath, 0);
        ASSERT_GT(egBypass, 0);
        EXPECT_NEAR(measure(1, 1, breath, 0), egBypass, 1e-10);
        double previousEg = 0;
        for (float eg : {.25f, .5f, .75f, 1.f}) {
            double currentEg = measure(eg, 1, breath);
            EXPECT_GT(currentEg, previousEg);
            previousEg = currentEg;
        }
    }
}
TEST(SignalPathTest, MeasurementDetectsKnownGainAndAnalyticalFirstOrderFilter) {
    for (double rate : {44100., 48000., 96000.})
        for (double hz : {20., 40., 80., 200., 1000.}) {
            EXPECT_NEAR(gain(rate, hz, [](float x) { return x; }), 1, 1e-6);
            EXPECT_NEAR(20 * std::log10(gain(rate, hz, [](float x) { return x * .5f; })),
                        -6.020599913, 1e-5);
            double a = std::exp(-tau * 40 / rate), previousInput = 0, previousOutput = 0;
            double measured = gain(rate, hz, [&](float x) {
                previousOutput = a * (previousOutput + x - previousInput);
                previousInput = x;
                return static_cast<float>(previousOutput);
            });
            auto z = std::polar(1., -tau * hz / rate);
            EXPECT_NEAR(measured, std::abs(a * (1. - z) / (1. - a * z)), 1e-6);
        }
}
TEST(SignalPathTest, VcaResponseConvergesAndMatchesIndependentCircuitTransfer) {
    std::array<double, 5> references{};
    for (double rate : {44100., 48000., 96000.}) {
        int index = 0;
        for (double hz : {20., 40., 80., 200., 1000.}) {
            SCOPED_TRACE(rate);
            SCOPED_TRACE(hz);
            cs01::ParameterState state;
            state.set(Param::Volume, 1);
            state.set(Param::VcaEgDepth, 1);
            state.set(Param::BreathVca, 0);
            VCAProcessor vca(state);
            vca.prepare(rate);
            double early = 0, late = 0, sine = 0, cosine = 0;
            for (int i = 0; i < static_cast<int>(5 * rate); ++i) {
                double phase = tau * hz * i / rate;
                float y = vca.processSample(static_cast<float>(.01 * std::sin(phase)), 1);
                ASSERT_TRUE(std::isfinite(y));
                if (i >= rate && i < 2 * rate)
                    early += static_cast<double>(y) * y;
                if (i >= 4 * rate) {
                    late += static_cast<double>(y) * y;
                    sine += y * std::sin(phase);
                    cosine += y * std::cos(phase);
                }
            }
            ASSERT_GT(late, 0);
            double lateGain = std::sqrt(late / rate) / (.01 / std::sqrt(2.));
            if (rate == 44100)
                references[index] = lateGain;
            else
                EXPECT_NEAR(20 * std::log10(lateGain / references[index]), 0, .1);
            EXPECT_NEAR(10 * std::log10(early / late), 0, .01);
            auto z = std::polar(1., -tau * hz / rate), s = 2 * rate * (1. - z) / (1. + z);
            auto butterworth = [&](double cutoff, bool high) {
                double w = 2 * rate * std::tan(std::numbers::pi * cutoff / rate);
                return (high ? s * s : std::complex<double>(w * w, 0)) /
                       (s * s + std::sqrt(2.) * w * s + w * w);
            };
            auto fixed = [&](double pole) { return pole * (1. - z) / (1. - pole * z); };
            double expected = std::abs(butterworth(40, true) * butterworth(20, true) *
                                       fixed(std::pow(static_cast<double>(.997f), 44100 / rate)) *
                                       .935 * (1. + .004 * (1. - z)) *
                                       fixed(std::pow(static_cast<double>(.9995f), 44100 / rate)) *
                                       butterworth(15000, false));
            double fundamental = 2 * std::hypot(sine, cosine) / rate / .01;
            EXPECT_NEAR(20 * std::log10(fundamental / expected), 0, .05);
            ++index;
        }
    }
}
TEST(SignalPathTest, SpectrumProbeDetectsKnownHarmonicAndFoldedTone) {
    for (double rate : {44100., 48000.}) {
        std::vector<float> values(static_cast<size_t>(rate));
        for (size_t i = 0; i < values.size(); ++i)
            values[i] = static_cast<float>(.5 * std::sin(tau * 5000 * i / rate) +
                                           .1 * std::sin(tau * 15000 * i / rate) +
                                           .02 * std::sin(tau * 25000 * i / rate));
        EXPECT_NEAR(amplitude(values, rate, 5000), .5, 1e-7);
        EXPECT_NEAR(amplitude(values, rate, 15000), .1, 1e-7);
        EXPECT_NEAR(amplitude(values, rate, rate - 25000), .02, 1e-7);
        EXPECT_LT(amplitude(values, rate, 1000), 1e-7);
    }
}
TEST(SignalPathTest, FirInternalRateSignalRetainsFundamentalAndRejectsFoldedTone) {
    for (double rate : {44100., 48000.})
        for (float resonance : {.7f, .8f})
            for (bool bypass : {true, false}) {
                cs01::OutputConverter converter;
                CS01VCFCircuit filter;
                filter.prepare(rate * 4);
                filter.setCutoffFrequency(5000);
                filter.setResonance(resonance);
                std::vector<float> values;
                for (int i = 0; i < static_cast<int>(2 * rate); ++i) {
                    float out = 0;
                    for (int j = 0; j < 4; ++j) {
                        float x = static_cast<float>(
                            .5 * std::sin(tau * 5000 * (i * 4 + j) / (rate * 4)));
                        converter.push(bypass ? x : filter.processSample(0, x));
                        if (j == 0)
                            out = converter.output();
                    }
                    ASSERT_TRUE(std::isfinite(out));
                    if (i >= rate)
                        values.push_back(out);
                }
                double fundamental = amplitude(values, rate, 5000);
                ASSERT_GT(fundamental, 1e-8);
                if (bypass) {
                    EXPECT_NEAR(fundamental, .5, .001);
                    EXPECT_LT(amplitude(values, rate, rate - 25000), 1e-6);
                }
            }
}

TEST(SignalPathTest, ModernWrapperMatchesFrozenCircuitArithmeticAndReset) {
    for (double rate : {44100., 48000., 96000., 192000.}) {
        CS01IIVCFCircuit actual;
        BeforeIG05630 before;
        actual.prepare(rate);
        before.prepare(rate);
        for (int pass = 0; pass < 2; ++pass) {
            for (int i = 0; i < 8192; ++i) {
                float cutoff = 20.f + static_cast<float>(i * 37 % 19980);
                float resonance = static_cast<float>(i % 11) / 10;
                actual.setCutoffFrequency(cutoff);
                before.setCutoffFrequency(std::min(cutoff, static_cast<float>(rate * .49)));
                actual.setResonance(resonance);
                before.setResonance(resonance);
                EXPECT_EQ(std::bit_cast<uint32_t>(actual.processSample(0, stimulus(i))),
                          std::bit_cast<uint32_t>(before.processSample(stimulus(i))));
            }
            actual.reset();
            before.reset();
        }
    }
}
TEST(SignalPathTest, Observation_OriginalFilterResponseAcrossLevelPeakAndLeakageRanges) {
    const auto measure = [](double rate, float cutoff, bool high, double level, double hz,
                            int settle, int window) {
        cs01::ParameterState state;
        state.set(Param::Cutoff, cutoff);
        state.set(Param::Resonance, high ? 1 : 0);
        state.set(Param::VcfEgDepth, 0);
        state.set(Param::ModDepth, 0);
        state.set(Param::BreathVcf, 0);
        OriginalVCFProcessor filter(state);
        filter.prepare(rate);
        double sine = 0, cosine = 0;
        int measured = 0;
        for (int i = 0; i < static_cast<int>(rate * (settle + window)); ++i) {
            double phase = tau * hz * i / rate;
            float out = filter.processSample(static_cast<float>(level * std::sin(phase)), 0, 0);
            EXPECT_TRUE(std::isfinite(out));
            if (i >= rate * settle) {
                sine += out * std::sin(phase);
                cosine += out * std::cos(phase);
                ++measured;
            }
        }
        EXPECT_EQ(measured, static_cast<int>(rate * window));
        double fundamental = 2 * std::hypot(sine, cosine) / measured;
        EXPECT_GT(fundamental, 0);
        return fundamental;
    };
    for (double rate : {44100., 48000., 96000.})
        for (bool high : {false, true})
            for (double hz : {100., 500., 800., 1000., 1200., 2000., 4000., 8000.})
                measure(rate, 1000, high, .01, hz, 1, 1);
    for (float cutoff : {250.f, 1000.f})
        for (bool high : {false, true}) {
            for (double level : {.01, .1, .5})
                for (int step = 12; step <= 24; ++step)
                    measure(48000, cutoff, high, level, cutoff * step / 20., 1, 2);
            for (double hz : {500., 1000., 2000., 4000., 8000., 16000.})
                measure(48000, cutoff, high, .01, hz, 1, 2);
        }
}
TEST(SignalPathTest, VcfVcaCascadeRetainsSmallSignalTransferWithinHistoricalTolerance) {
    for (double rate : {44100., 48000., 96000.})
        for (bool high : {false, true})
            for (double hz : {20., 40., 80., 250., 1000., 4000.}) {
                cs01::ParameterState state;
                state.set(Param::Cutoff, 250);
                state.set(Param::Resonance, high ? 1 : 0);
                state.set(Param::VcfEgDepth, 0);
                state.set(Param::Volume, 1);
                state.set(Param::VcaEgDepth, 1);
                state.set(Param::BreathVca, 0);
                state.set(Param::BreathVcf, 0);
                state.set(Param::ModDepth, 0);
                OriginalVCFProcessor filter(state);
                VCAProcessor alone(state), cascade(state);
                filter.prepare(rate);
                alone.prepare(rate);
                cascade.prepare(rate);
                std::array<double, 3> sine{}, cosine{};
                for (int i = 0; i < static_cast<int>(3 * rate); ++i) {
                    double phase = tau * hz * i / rate;
                    float x = static_cast<float>(.001 * std::sin(phase));
                    float filtered = filter.processSample(x, 0, 0);
                    const std::array<float, 3> values{filtered, alone.processSample(x, 1),
                                                      cascade.processSample(filtered, 1)};
                    for (int j = 0; j < 3; ++j) {
                        ASSERT_TRUE(std::isfinite(values[j]));
                        if (i >= 2 * rate) {
                            sine[j] += values[j] * std::sin(phase);
                            cosine[j] += values[j] * std::cos(phase);
                        }
                    }
                }
                std::array<double, 3> db{};
                for (int j = 0; j < 3; ++j) {
                    double fundamental = 2 * std::hypot(sine[j], cosine[j]) / rate;
                    ASSERT_GT(fundamental, 0);
                    db[j] = 20 * std::log10(fundamental / .001);
                }
                EXPECT_NEAR(db[2], db[0] + db[1], .05);
            }
}

TEST(OriginalVCFProcessorModulationTest, Observation_CombinedSemitoneExponent) {
    constexpr int sampleCount = 262144;
    std::vector<float> eg(sampleCount), lfo(sampleCount), breath(sampleCount),
        baseCutoff(sampleCount);
    for (int i = 0; i < sampleCount; ++i) {
        const float phase = static_cast<float>(i) * 0.013f;
        eg[i] = std::sin(phase);
        lfo[i] = std::sin(phase * 0.37f);
        breath[i] = std::sin(phase * 0.11f);
        baseCutoff[i] = 20.0f + static_cast<float>(i % 19981);
    }

    std::vector<float> separate(sampleCount), combined(sampleCount);
    const auto separateStart = std::chrono::steady_clock::now();
    double separateSum = 0.0;
    for (int i = 0; i < sampleCount; ++i) {
        const float egMod = eg[i] * 0.7f * 36.0f;
        const float lfoMod = lfo[i] * 0.8f * 24.0f;
        const float breathMod = breath[i] * 0.6f * 24.0f;
        const float egRatio = static_cast<float>(std::exp2(static_cast<double>(egMod / 12.0f)));
        const float lfoRatio = static_cast<float>(std::exp2(static_cast<double>(lfoMod / 12.0f)));
        const float breathRatio =
            static_cast<float>(std::exp2(static_cast<double>(breathMod / 12.0f)));
        separate[i] = baseCutoff[i] * egRatio * lfoRatio * breathRatio;
        separateSum += separate[i];
    }
    const auto separateElapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - separateStart).count();

    const auto combinedStart = std::chrono::steady_clock::now();
    double combinedSum = 0.0;
    for (int i = 0; i < sampleCount; ++i) {
        const float egMod = eg[i] * 0.7f * 36.0f;
        const float lfoMod = lfo[i] * 0.8f * 24.0f;
        const float breathMod = breath[i] * 0.6f * 24.0f;
        const float totalModSemitones = egMod + lfoMod + breathMod;
        const float ratio =
            static_cast<float>(std::exp2(static_cast<double>(totalModSemitones / 12.0f)));
        combined[i] = baseCutoff[i] * ratio;
        combinedSum += combined[i];
    }
    const auto combinedElapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - combinedStart).count();

    float maxRelativeError = 0.0f;
    for (int i = 0; i < sampleCount; ++i) {
        maxRelativeError = std::max(maxRelativeError, std::abs(combined[i] - separate[i]) /
                                                          std::max(separate[i], 1.0f));
    }

    std::vector<float> input(sampleCount), separateOutput(sampleCount), combinedOutput(sampleCount);
    for (int i = 0; i < sampleCount; ++i) {
        input[i] = 0.25f * std::sin(static_cast<float>(i) * 0.047f);
    }
    separateOutput = input;
    combinedOutput = input;
    CS01VCFCircuit separateFilter, combinedFilter;
    separateFilter.prepare(192000.0);
    combinedFilter.prepare(192000.0);
    separateFilter.processBlock(separateOutput.data(), sampleCount, separate.data(), 0.7f);
    combinedFilter.processBlock(combinedOutput.data(), sampleCount, combined.data(), 0.7f);
    float maxAbsAudioDifference = 0.0f;
    for (int i = 0; i < sampleCount; ++i) {
        maxAbsAudioDifference =
            std::max(maxAbsAudioDifference, std::abs(separateOutput[i] - combinedOutput[i]));
    }

    ASSERT_TRUE(std::isfinite(separateSum));
    ASSERT_TRUE(std::isfinite(combinedSum));
    EXPECT_LT(maxRelativeError, 1.0e-6f);
    std::cout << "ORIGINAL_VCF_MODULATION_BENCH," << sampleCount << ',' << separateElapsed << ','
              << combinedElapsed << ',' << maxRelativeError << ',' << separateSum << ','
              << combinedSum << ',' << maxAbsAudioDifference << '\n';
}

TEST(IG02610BehavioralModelTest, Observation_ProductionPanelResponse) {
    for (double hostRate : {44100.0, 48000.0, 96000.0}) {
        const double rate = hostRate * cs01::OutputConverter::factor;
        for (float position : {0.0f, 0.5f, 1.0f}) {
            const float cutoff = cs01::fromNormalized(Param::Cutoff, position);
            for (float resonance : {0.2f, 0.7f}) {
                for (double ratio : {0.5, 0.75, 0.9, 1.0, 1.1, 1.5, 2.0}) {
                    // Observe only the host-audible band. No peak claim at the
                    // maximum setting, whose resonance is near the band edge.
                    const double frequency = std::round(cutoff * ratio);
                    if (frequency >= hostRate * 0.45)
                        continue;
                    CS01VCFCircuit filter;
                    filter.prepare(rate);
                    filter.setCutoffFrequency(cutoff);
                    filter.setResonance(resonance);
                    double sine = 0.0, cosine = 0.0;
                    const int count = static_cast<int>(rate);
                    for (int i = 0; i < count * 2; ++i) {
                        const double phase = tau * frequency * i / rate;
                        const double output =
                            filter.processSample(0, static_cast<float>(0.01 * std::sin(phase)));
                        ASSERT_TRUE(std::isfinite(output));
                        if (i >= count) {
                            sine += output * std::sin(phase);
                            cosine += output * std::cos(phase);
                        }
                    }
                    const double gain = 2.0 * std::hypot(sine, cosine) / count / 0.01;
                    ASSERT_GT(gain, 0.0);
                    std::cout << "VCF panel: host=" << hostRate << ", position=" << position
                              << ", cutoff=" << cutoff << ", resonance=" << resonance
                              << ", frequency=" << frequency << ", gain=" << 20.0 * std::log10(gain)
                              << " dB\n";
                }
            }
        }
    }
}

TEST(IG02610BehavioralModelSpectrumTest,
     Observation_CharacterizeDrivenFilterHarmonicsAndFoldedComponents) {
    // One-second coherent window after one-second settling. Observations only:
    // folded bins can contain multiple harmonics, not exclusively harmonic five.
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        for (float resonance : {0.2f, 0.7f, 0.8f}) {
            for (float amplitude : {0.01f, 0.5f, 2.0f}) {
                CS01VCFCircuit filter;
                filter.prepare(rate);
                filter.setCutoffFrequency(5000.0f);
                filter.setResonance(resonance);
                std::vector<float> samples;
                samples.reserve(static_cast<size_t>(rate));
                for (int i = 0; i < static_cast<int>(2.0 * rate); ++i) {
                    const float input =
                        amplitude * static_cast<float>(std::sin(tau * 5000.0 * i / rate));
                    const float output = filter.processSample(0, input);
                    ASSERT_TRUE(std::isfinite(output));
                    if (i >= static_cast<int>(rate))
                        samples.push_back(output);
                }
                const double fundamental = ::amplitude(samples, rate, 5000.0);
                ASSERT_GT(fundamental, 1.0e-8);
                const double fifthFrequency = rate < 50000.0 ? rate - 25000.0 : 25000.0;
                const auto dBc = [&](double frequency) {
                    return 20.0 *
                           std::log10(std::max(1.0e-15, ::amplitude(samples, rate, frequency)) /
                                      fundamental);
                };
                std::cout << "VCF spectrum: fs=" << rate << ", resonance=" << resonance
                          << ", amplitude=" << amplitude << ", H3=" << dBc(15000.0)
                          << " dBc, fifth-bin=" << fifthFrequency << " Hz: " << dBc(fifthFrequency)
                          << " dBc\n";
            }
        }
    }
}

TEST(SignalPathTest, HighPassQuantizationMatchesIndependentTransferAt96kHz) {
    const double rate = 96000, hz = 40;
    auto z = std::polar(1., -tau * hz / rate);
    for (double cutoff : {20., 40.}) {
        double k = std::tan(std::numbers::pi * cutoff / rate);
        double norm = 1 / (1 + std::sqrt(2.) * k + k * k);
        std::array<double, 5> precise{norm, -2 * norm, norm, 2 * (k * k - 1) * norm,
                                      (1 - std::sqrt(2.) * k + k * k) * norm};
        auto response = [&](auto coefficients) {
            return std::abs((coefficients[0] + coefficients[1] * z + coefficients[2] * z * z) /
                            (1. + coefficients[3] * z + coefficients[4] * z * z));
        };
        double ratio = std::tan(std::numbers::pi * hz / rate) / k;
        double ideal = ratio * ratio / std::sqrt(1 + std::pow(ratio, 4));
        EXPECT_NEAR(response(precise), ideal, 1e-9);
        // Round the independently derived coefficients as the real float DSP does.
        for (auto& c : precise)
            c = static_cast<float>(c);
        cs01::Biquad filter;
        filter.highPass(rate, cutoff);
        double measured = gain(rate, hz, [&](float input) { return filter.processSample(input); });
        EXPECT_NEAR(20 * std::log10(measured / response(precise)), 0, .01);
    }
}
TEST(SignalPathTest, ModernImpulseAndModulationRetainStateAcrossShortPartitions) {
    for (bool modulation : {false, true}) {
        auto render = [&](int size) {
            cs01::ParameterState state;
            state.set(Param::Cutoff, 1000);
            state.set(Param::VcfEgDepth, modulation ? .7f : 0);
            state.set(Param::ModDepth, modulation ? .6f : 0);
            ModernVCFProcessor filter(state);
            filter.prepare(48000);
            std::vector<float> output;
            for (int offset = 0; offset < 1024;) {
                int count = std::min(size, 1024 - offset);
                for (int i = 0; i < count; ++i) {
                    float t = static_cast<float>(offset + i);
                    output.push_back(filter.processSample(modulation ? .2f * std::sin(t * .17f)
                                                          : t == 0   ? 1.f
                                                                     : 0.f,
                                                          modulation ? t / 1024 : 0,
                                                          modulation ? std::sin(t * .031f) : 0));
                }
                offset += count;
            }
            return output;
        };
        auto expected = render(512);
        EXPECT_GT(*std::max_element(expected.begin(), expected.end()), .001f);
        for (int size : {1, 7, 64, 127, 511}) {
            auto actual = render(size);
            for (size_t i = 0; i < actual.size(); ++i) {
                ASSERT_TRUE(std::isfinite(actual[i]));
                ASSERT_NEAR(actual[i], expected[i], 1e-6);
            }
        }
    }
}
TEST(SignalPathTest, VcaLiveDepthPreservesSmoothingAcrossPartitions) {
    auto render = [](int size) {
        cs01::ParameterState state;
        state.set(Param::VcaEgDepth, 0);
        state.set(Param::Volume, 1);
        VCAProcessor vca(state);
        vca.prepare(48000);
        state.set(Param::VcaEgDepth, 1);
        std::vector<float> output;
        for (int offset = 0; offset < 960;) {
            int count = std::min(size, 960 - offset);
            for (int i = 0; i < count; ++i)
                output.push_back(vca.processSample(.1f, 0));
            offset += count;
        }
        return output;
    };
    auto reference = render(1);
    EXPECT_GT(power(reference), 0);
    for (int size : {7, 64}) {
        auto actual = render(size);
        for (size_t i = 0; i < actual.size(); ++i) {
            ASSERT_TRUE(std::isfinite(actual[i]));
            ASSERT_NEAR(actual[i], reference[i], 1e-7);
        }
    }
}

TEST(SignalPathTest, ModernFilterLiveDepthPreservesSmoothingAcrossPartitions) {
    auto render = [](int size) {
        cs01::ParameterState state;
        state.set(Param::Cutoff, 500);
        state.set(Param::VcfEgDepth, 0);
        ModernVCFProcessor filter(state);
        filter.prepare(48000);
        state.set(Param::VcfEgDepth, 1);
        std::vector<float> output;
        for (int offset = 0; offset < 960;) {
            int count = std::min(size, 960 - offset);
            for (int i = 0; i < count; ++i) {
                const int t = offset + i;
                output.push_back(filter.processSample(
                    static_cast<float>(.1 * std::sin(tau * 1500 * t / 48000.)), 1.f, 0.f));
            }
            offset += count;
        }
        return output;
    };
    auto reference = render(1);
    EXPECT_GT(power(reference), 0);
    for (int size : {7, 64}) {
        auto actual = render(size);
        for (size_t i = 0; i < actual.size(); ++i) {
            ASSERT_TRUE(std::isfinite(actual[i]));
            ASSERT_NEAR(actual[i], reference[i], 1e-7);
        }
    }
}

TEST(SignalPathTest, BothFilterModelsGenerateEvenHarmonicsUnderDrivenInput) {
    constexpr double rate = 48000.0;
    constexpr double hz = 500.0;
    IG02610BehavioralModel original;
    IG05630BehavioralModel modern;
    original.prepare(rate);
    modern.prepare(rate);
    modern.setCutoffFrequency(4000.0f);
    modern.setResonance(0.4f);
    std::vector<float> origOut, modOut;
    for (int i = 0; i < static_cast<int>(2 * rate); ++i) {
        const float x = static_cast<float>(0.6 * std::sin(tau * hz * i / rate));
        const float yOrig = original.processSample(x, 4000.0f, 0.4f);
        const float yMod = modern.processSample(x);
        if (i >= static_cast<int>(rate)) {
            origOut.push_back(yOrig);
            modOut.push_back(yMod);
        }
    }
    EXPECT_GT(amplitude(origOut, rate, 2 * hz), 1e-3);
    EXPECT_GT(amplitude(modOut, rate, 2 * hz), 1e-3);
}

namespace {
struct CutoffModulationHarness : IFilter {
    using IFilter::IFilter;
    using IFilter::nextModulatedCutoff;
    using IFilter::prepareCutoffModulation;
};
}  // namespace

TEST(SignalPathTest, VcfDepthLinearRampHasExactDurationSlopeAndRetargeting) {
    for (double rate : {44100., 48000., 96000., 192000., 384000.}) {
        SCOPED_TRACE(rate);
        cs01::ParameterState state;
        state.set(Param::Cutoff, 500);
        state.set(Param::ModDepth, 0);
        state.set(Param::BreathInput, 0);
        state.set(Param::VcfEgDepth, 0);
        CutoffModulationHarness modulation(state);
        modulation.prepareCutoffModulation(rate);
        const int duration = static_cast<int>(rate * .005);
        const auto depth = [&] {
            return std::log2(modulation.nextModulatedCutoff(1, 0) / 500.) / 3.;
        };
        for (float target : {1.f, 0.f}) {
            state.set(Param::VcfEgDepth, target);
            for (int i = 1; i <= duration; ++i) {
                const double progress = double(i) / duration;
                EXPECT_NEAR(depth(), target == 1 ? progress : 1 - progress, 2e-5);
            }
            EXPECT_DOUBLE_EQ(depth(), target);
        }
        state.set(Param::VcfEgDepth, 1);
        double current = 0;
        for (int i = 0; i < duration / 2; ++i)
            current = depth();
        state.set(Param::VcfEgDepth, 0);
        for (int i = 1; i <= duration; ++i)
            EXPECT_NEAR(depth(), current * (1 - double(i) / duration), 2e-5);
        EXPECT_DOUBLE_EQ(depth(), 0);
        state.set(Param::VcfEgDepth, 1);
        modulation.prepareCutoffModulation(rate);
        EXPECT_DOUBLE_EQ(depth(), 1);
    }
}

namespace {
struct FilterSpectrum {
    double dc, rms, h1, h2, h3;
};
double filterSpectrumTolerance(double expected) {
    // Raw model samples are float; bins near -160 dBFS are below a stable cross-platform floor.
    return std::max(1e-8, std::abs(expected) * 5e-4);
}
FilterSpectrum measureFilterSpectrum(const std::vector<float>& output, double rate, double hz) {
    double dc = 0;
    for (double value : output)
        dc += value;
    return {dc / output.size(), std::sqrt(power(output) / output.size()),
            amplitude(output, rate, hz), amplitude(output, rate, 2 * hz),
            amplitude(output, rate, 3 * hz)};
}
std::vector<float> renderFilterSpectrum(bool modern, double rate, double hz, float inputLevel,
                                        float resonance) {
    IG02610BehavioralModel original;
    IG05630BehavioralModel updated;
    original.prepare(rate);
    updated.prepare(rate);
    updated.setCutoffFrequency(12000);
    updated.setResonance(resonance);
    std::vector<float> output;
    output.reserve(static_cast<size_t>(rate));
    for (int i = 0; i < static_cast<int>(2 * rate); ++i) {
        const float input = static_cast<float>(inputLevel * std::sin(tau * hz * i / rate));
        const float value =
            modern ? updated.processSample(input) : original.processSample(input, 12000, resonance);
        if (i >= static_cast<int>(rate))
            output.push_back(value);
    }
    return output;
}
}  // namespace

TEST(SignalPathTest, FilterSpectrumMeasurementMatchesKnownSignal) {
    std::vector<float> output;
    for (int i = 0; i < 48000; ++i) {
        const double phase = tau * 500 * i / 48000;
        output.push_back(static_cast<float>(.02 + .5 * std::sin(phase) + .1 * std::cos(2 * phase) +
                                            .05 * std::sin(3 * phase)));
    }
    const auto measured = measureFilterSpectrum(output, 48000, 500);
    EXPECT_NEAR(measured.dc, .02, 1e-8);
    EXPECT_NEAR(measured.rms, std::sqrt(.0004 + (.25 + .01 + .0025) / 2), 1e-8);
    EXPECT_NEAR(measured.h1, .5, 1e-8);
    EXPECT_NEAR(measured.h2, .1, 1e-8);
    EXPECT_NEAR(measured.h3, .05, 1e-8);
}

TEST(SignalPathTest, FilterSpectrumMatchesFixedProductSoundBaseline) {
    // Captured at ff9245a: raw IC models, no VCA DC removal or engine oversampling.
    struct Baseline {
        bool modern;
        double rate, hz;
        float inputLevel, resonance;
        FilterSpectrum spectrum;
        double probe;
    };
    const Baseline baselines[]{
        {false, 44100, 500, .6f, .8f, {.0115299, .261777, .368879, .0128233, .0230996}, .00326019},
        {true, 44100, 500, .6f, .8f, {.00944631, .246188, .347218, .0113608, .0184045}, .00218251},
        {false, 48000, 500, .6f, .8f, {.0115256, .261805, .368916, .0128327, .0231248}, .00327261},
        {true, 48000, 500, .6f, .8f, {.00944343, .246182, .34721, .011362, .0184008}, .00218173},
        {false, 96000, 500, .6f, .8f, {.0115279, .261906, .369056, .0128477, .0231881}, .0032922},
        {true, 96000, 500, .6f, .8f, {.00944046, .246094, .347093, .0113385, .0182988}, .00213779},
        {false, 48000, 100, .01f, 0, {0, .00314274, .0044445, 0, 8.94737e-9}, 3.47827e-11},
        {true, 48000, 100, .01f, 0, {0, .00707036, .009999, 0, 3.33299e-7}, 9.08337e-11},
        {false,
         48000,
         5000,
         .6f,
         0,
         {.00230044, .192971, .271859, .00342861, .0233192},
         2.37219e-5},
        {true, 48000, 5000, .6f, 0, {.0170318, .300985, .424672, .0136926, .00809005}, 1.57799e-7},
        {false,
         48000,
         10000,
         .6f,
         0,
         {.00739138, .243506, .344144, .00121785, .00524165},
         .00524165},
        {true,
         48000,
         10000,
         .6f,
         0,
         {.0170631, .284316, .401232, 7.46432e-5, .00121464},
         .00121464}};
    for (const auto& baseline : baselines) {
        SCOPED_TRACE(baseline.modern);
        SCOPED_TRACE(baseline.rate);
        SCOPED_TRACE(baseline.hz);
        const auto output = renderFilterSpectrum(baseline.modern, baseline.rate, baseline.hz,
                                                 baseline.inputLevel, baseline.resonance);
        const auto measured = measureFilterSpectrum(output, baseline.rate, baseline.hz);
        const auto check = [](double actual, double expected) {
            EXPECT_NEAR(actual, expected, filterSpectrumTolerance(expected));
        };
        check(measured.dc, baseline.spectrum.dc);
        check(measured.rms, baseline.spectrum.rms);
        check(measured.h1, baseline.spectrum.h1);
        check(measured.h2, baseline.spectrum.h2);
        check(measured.h3, baseline.spectrum.h3);
        const int harmonic = baseline.hz >= 10000 ? 3 : 5;
        const double wrapped = std::fmod(harmonic * baseline.hz, baseline.rate);
        check(amplitude(output, baseline.rate, std::min(wrapped, baseline.rate - wrapped)),
              baseline.probe);
    }
}

TEST(SignalPathTest, FilterSpectrumToleranceAllowsFloatNoiseButRejectsLostAliasComponent) {
    constexpr double baseline = 1.57799e-7;
    constexpr double windowsMeasurement = 1.5605581065933193e-7;
    EXPECT_LE(std::abs(windowsMeasurement - baseline), filterSpectrumTolerance(baseline));
    EXPECT_GT(baseline, filterSpectrumTolerance(baseline));
    EXPECT_GT(baseline * .1, filterSpectrumTolerance(baseline));
    EXPECT_DOUBLE_EQ(filterSpectrumTolerance(.4), .4 * 5e-4);
    EXPECT_GT(.4 * .001, filterSpectrumTolerance(.4));
}

TEST(SignalPathTest, Observation_FilterSpectrumAndFoldedBins) {
    for (double rate : {44100., 48000., 96000.})
        for (bool modern : {false, true})
            for (double hz : {100., 500., 5000., 10000.})
                for (float resonance : {0.f, .8f}) {
                    const float inputLevel = hz == 100 ? .01f : .6f;
                    const auto output =
                        renderFilterSpectrum(modern, rate, hz, inputLevel, resonance);
                    for (float value : output) {
                        ASSERT_TRUE(std::isfinite(value));
                        ASSERT_LE(std::abs(value), 1.5f);
                    }
                    const auto measured = measureFilterSpectrum(output, rate, hz);
                    ASSERT_GT(measured.h1, 0);
                    const int harmonic = hz >= 10000 ? 3 : 5;
                    const double wrapped = std::fmod(harmonic * hz, rate);
                    const double bin = std::min(wrapped, rate - wrapped);
                    const double probe = amplitude(output, rate, bin);
                    std::cout << "VCF spectrum: modern=" << modern << ", fs=" << rate
                              << ", hz=" << hz << ", input=" << inputLevel
                              << ", resonance=" << resonance << ", dc=" << measured.dc
                              << ", rms=" << measured.rms << ", h1=" << measured.h1
                              << ", h2=" << measured.h2 << ", h3=" << measured.h3
                              << ", probeHz=" << bin << ", probe=" << probe << '\n';
                }
}
