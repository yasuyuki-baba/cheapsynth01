#include "cs01synth/VCAProcessor.h"
#include "cs01synth/CS01VCFCircuit.h"
#include "cs01synth/VCAEmpiricalStages.h"
#include "cs01synth/VCFCouplingStages.h"
#include "cs01synth/LFOProcessor.h"
#include "cs01synth/ModernVCFProcessor.h"
#include "cs01synth/NoiseGenerator.h"
#include "cs01synth/ToneGenerator.h"
#include "cs01synth/CS01IIVCFCircuit.h"
#include "Parameters.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

using cs01::Param;
TEST(LfoRegressionTest, PeriodAmplitudeAndContinuity) {
    for (double rate : {44100., 48000., 96000.}) {
        for (float frequency : {.8f, 10.9f, 21.f}) {
            SCOPED_TRACE(rate);
            SCOPED_TRACE(frequency);
            cs01::ParameterState state;
            state.set(Param::LfoSpeed, frequency);
            LFOProcessor lfo(state);
            lfo.prepare(rate);
            float previous = lfo.processSample(), minimum = 1, maximum = -1, jump = 0;
            int last = -1, periods = 0;
            double error = 0;
            bool finite = true;
            for (int i = 0; i < static_cast<int>(rate / frequency * 3.2); ++i) {
                const auto value = lfo.processSample();
                finite &= std::isfinite(value);
                minimum = std::min(minimum, value);
                maximum = std::max(maximum, value);
                jump = std::max(jump, std::abs(value - previous));
                if (previous < 0 && value >= 0) {
                    if (last >= 0) {
                        error = std::max(error, std::abs(i - last - rate / frequency));
                        ++periods;
                    }
                    last = i;
                }
                previous = value;
            }
            EXPECT_TRUE(finite);
            EXPECT_GE(periods, 2);
            EXPECT_NEAR(minimum, -1, .001);
            EXPECT_NEAR(maximum, 1, .001);
            EXPECT_LE(error, rate / frequency * .001 + 2);
            EXPECT_LE(jump, 4 * frequency / rate + .00001);
        }
    }
}
TEST(LfoRegressionTest, LiveFrequencyEditKeepsPhaseContinuousAndSettles) {
    cs01::ParameterState state;
    state.set(Param::LfoSpeed, .8f);
    LFOProcessor lfo(state);
    lfo.prepare(48000);
    float previous = 0;
    for (int i = 0; i < 7000; ++i)
        previous = lfo.processSample();
    state.set(Param::LfoSpeed, 21);
    EXPECT_LT(std::abs(lfo.processSample() - previous), .002f);
    for (int i = 0; i < 4800; ++i)
        lfo.processSample();
    int last = -1, periods = 0;
    previous = lfo.processSample();
    for (int i = 0; i < 12000; ++i) {
        float value = lfo.processSample();
        if (previous < 0 && value >= 0) {
            if (last >= 0) {
                EXPECT_NEAR(i - last, 48000. / 21, 2);
                ++periods;
            }
            last = i;
        }
        previous = value;
    }
    EXPECT_GE(periods, 3);
}
namespace {
double modernGain(double rate, double frequency, float resonance, float envelope = 0,
                  float egDepth = 0, float lfo = 0, float mod = 0, float breath = 0,
                  float breathDepth = 0) {
    cs01::ParameterState state;
    state.set(Param::Cutoff, 1000);
    state.set(Param::Resonance, resonance);
    state.set(Param::VcfEgDepth, egDepth);
    state.set(Param::ModDepth, mod);
    state.set(Param::BreathInput, breath);
    state.set(Param::BreathVcf, breathDepth);
    ModernVCFProcessor filter(state);
    filter.prepare(rate);
    double energy = 0;
    for (int i = 0; i < 16384; ++i) {
        float input =
            static_cast<float>(.1 * std::sin(2 * std::numbers::pi * frequency * i / rate));
        float value = filter.processSample(input, envelope, lfo);
        if (!std::isfinite(value))
            return std::numeric_limits<double>::quiet_NaN();
        if (i >= 8192)
            energy += value * value;
    }
    return std::sqrt(energy / 8192) / (.1 / std::sqrt(2.));
}
}  // namespace
TEST(ModernFilterRegressionTest, FourPoleResponseAndFiniteResonance) {
    for (double rate : {44100., 48000., 96000.}) {
        SCOPED_TRACE(rate);
        EXPECT_NEAR(modernGain(rate, 100, 0), 1, .02);
        EXPECT_NEAR(modernGain(rate, 1000, 0), 1 / std::sqrt(2.), .02);
        EXPECT_LT(modernGain(rate, 4000, 0) / modernGain(rate, 2000, 0), .075);
        EXPECT_GT(modernGain(rate, 1000, 1), modernGain(rate, 1000, 0));
    }
}
TEST(ModernFilterRegressionTest, EgLfoAndBreathDepthAreIndependentAndRaiseCutoff) {
    const auto plain = modernGain(48000, 2000, 0);
    EXPECT_NEAR(modernGain(48000, 2000, 0, 1, 0), plain, 1.e-9);
    EXPECT_NEAR(modernGain(48000, 2000, 0, 0, 0, 1, 0), plain, 1.e-9);
    EXPECT_NEAR(modernGain(48000, 2000, 0, 0, 0, 0, 0, 1, 0), plain, 1.e-9);
    EXPECT_GT(modernGain(48000, 2000, 0, 1, .5), plain * 2);
    EXPECT_GT(modernGain(48000, 2000, 0, 0, 0, 1, .5), plain * 2);
    EXPECT_GT(modernGain(48000, 2000, 0, 0, 0, 0, 0, 1, .5), plain * 2);
}
TEST(ModernFilterRegressionTest, ResetAndDrivenModulationAreDeterministicAndBounded) {
    for (double rate : {44100., 48000., 96000.}) {
        CS01IIVCFCircuit circuit;
        circuit.prepare(rate);
        auto render = [&] {
            std::array<float, 4096> values;
            for (int i = 0; i < 4096; ++i) {
                circuit.setCutoffFrequency(20.f + (i % 1000) * 19.f);
                circuit.setResonance(static_cast<float>(i % 101) / 100);
                values[i] = circuit.processSample(0, .2f * std::sin(i * .13f));
            }
            return values;
        };
        const auto first = render();
        circuit.reset();
        const auto second = render();
        EXPECT_EQ(first, second);
        for (float value : first) {
            ASSERT_TRUE(std::isfinite(value));
            ASSERT_LE(std::abs(value), IG05630BehavioralModel::EmpiricalParameters::maximumOutput);
        }
    }
}
TEST(ModernFilterRegressionTest, NonFiniteInputCannotPoisonFollowingSamples) {
    CS01IIVCFCircuit circuit;
    circuit.prepare(48000);
    for (float value :
         {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        circuit.setCutoffFrequency(value);
        circuit.setResonance(value);
        EXPECT_TRUE(std::isfinite(circuit.processSample(0, value)));
        for (int i = 0; i < 256; ++i)
            ASSERT_TRUE(std::isfinite(circuit.processSample(0, .1f)));
    }
}
TEST(NoiseRegressionTest, ReleaseDeadlineAndImmediateStopAtEveryRate) {
    for (double rate : {44100., 48000., 96000., 176400.}) {
        cs01::ParameterState state;
        state.set(Param::Release, .04f);
        NoiseGenerator noise(state);
        noise.prepare(rate);
        noise.startNote(60, 1, 8192);
        EXPECT_EQ(noise.currentNote(), 60);
        noise.stopNote(true);
        const auto duration = static_cast<int>(state.get(Param::Release) * rate);
        for (int i = 0; i < duration - 1; ++i)
            noise.renderSample();
        EXPECT_TRUE(noise.isActive());
        noise.renderSample();
        EXPECT_FALSE(noise.isActive());
        EXPECT_FLOAT_EQ(noise.renderSample(), 0);
        noise.startNote(72, 1, 8192);
        noise.stopNote(false);
        EXPECT_FALSE(noise.isActive());
        EXPECT_FLOAT_EQ(noise.renderSample(), 0);
    }
}
TEST(NoiseRegressionTest, PitchAndWheelDoNotChangeNoiseAndPrepareResetsSequence) {
    cs01::ParameterState state;
    NoiseGenerator a(state), b(state);
    a.prepare(48000);
    b.prepare(48000);
    a.startNote(48, 1, 0);
    b.startNote(84, .5, 16383);
    std::array<float, 1024> original;
    double energy = 0;
    for (int i = 0; i < 1024; ++i) {
        original[i] = a.renderSample();
        EXPECT_FLOAT_EQ(original[i], b.renderSample());
        energy += original[i] * original[i];
    }
    EXPECT_GT(energy, .01);
    a.prepare(48000);
    a.startNote(60, 1, 8192);
    for (int i = 0; i < 1024; ++i)
        EXPECT_FLOAT_EQ(a.renderSample(), original[i]);
}
TEST(TonePitchRegressionTest, PitchAndFeetMatchMeasuredPeriods) {
    for (double rate : {44100., 48000., 96000.})
        for (int feet = 0; feet < 4; ++feet)
            for (int waveform : {0, 1, 2, 3, 4})
                for (int note : {45, 69, 93}) {
                    SCOPED_TRACE(rate);
                    SCOPED_TRACE(feet);
                    SCOPED_TRACE(waveform);
                    SCOPED_TRACE(note);
                    cs01::ParameterState state;
                    state.set(Param::Feet, feet);
                    state.set(Param::WaveType, waveform);
                    ToneGenerator tone(state);
                    tone.prepare(rate);
                    tone.startNote(note, 1, 8192);
                    tone.setPitchBendSemitones(0);
                    tone.updateBlockRateParameters();
                    const double expected = 440 * std::exp2((note - 69) / 12. + feet - 2);
                    const int settle = static_cast<int>(rate * .1);
                    double first = 0, last = 0;
                    int crossings = 0;
                    float previous = tone.getNextSample();
                    for (int i = 1; i < static_cast<int>(rate * 4) && crossings < 65; ++i) {
                        const auto value = tone.getNextSample();
                        ASSERT_TRUE(std::isfinite(value));
                        if (i >= settle && previous <= 0 && value > 0) {
                            const double position =
                                i - 1 + (-previous / static_cast<double>(value - previous));
                            if (crossings == 0)
                                first = position;
                            last = position;
                            ++crossings;
                        }
                        previous = value;
                    }
                    ASSERT_EQ(crossings, 65);
                    EXPECT_NEAR(64 * rate / (last - first), expected, expected * .001);
                }
}
TEST(TonePitchRegressionTest, ResetReproducesFreshWaveformForAllWaveforms) {
    for (int waveform = 0; waveform < 5; ++waveform) {
        cs01::ParameterState state;
        state.set(Param::WaveType, waveform);
        ToneGenerator a(state), b(state);
        auto start = [](ToneGenerator& tone) {
            tone.prepare(48000);
            tone.startNote(69, 1, 8192);
            tone.setPitchBendSemitones(0);
        };
        start(a);
        start(b);
        for (int i = 0; i < 12345; ++i)
            a.renderSample();
        start(a);
        for (int i = 0; i < 4096; ++i)
            ASSERT_FLOAT_EQ(a.renderSample(), b.renderSample());
    }
}

TEST(EmpiricalStagesTest, VcaSplitRetainsPreviousArithmeticAndResets) {
    for (const double rate : {44100., 48000., 96000.}) {
        EmpiricalVCACoupling inputCoupling{.997f}, outputCoupling{.9995f};
        Tr7EmpiricalBuffer buffer;
        inputCoupling.prepare(rate, .997f);
        outputCoupling.prepare(rate, .9995f);
        const float inputPole =
            static_cast<float>(std::pow(static_cast<double>(.997f), 44100. / rate));
        const float outputPole =
            static_cast<float>(std::pow(static_cast<double>(.9995f), 44100. / rate));
        std::array<float, 2048> first{};
        for (int pass = 0; pass < 2; ++pass) {
            float inputState = 0, previous = 0, outputState = 0;
            inputCoupling.reset();
            outputCoupling.reset();
            buffer.reset();
            for (int i = 0; i < 2048; ++i) {
                const float sample = static_cast<float>(.9 * std::sin(.117 * i));
                // Independent expression from the pre-split VCA implementation.
                inputState = inputState * inputPole + sample * (1.f - inputPole);
                const float coupled = sample - inputState;
                const float transistor = coupled > 0 ? coupled * .95f : coupled * .92f;
                const float highFrequency = (transistor - previous) * (1.f - .998f) * 2.f;
                previous = transistor;
                const float buffered = transistor + highFrequency;
                outputState = outputState * outputPole + buffered * (1.f - outputPole);
                const float expected = buffered - outputState;
                const float actual = outputCoupling.processSample(
                    buffer.processSample(inputCoupling.processSample(sample)));
                EXPECT_FLOAT_EQ(actual, expected) << "rate=" << rate << " sample=" << i;
                if (!pass)
                    first[i] = actual;
                else
                    EXPECT_FLOAT_EQ(actual, first[i]);
            }
        }
    }
}

TEST(EmpiricalStagesTest, VcfOutputCouplingRetainsMixedPrecisionAndResets) {
    for (const double rate : {44100., 48000., 96000.}) {
        EmpiricalVCFOutputCoupling stage;
        stage.prepare(rate);
        std::array<float, 2048> first{};
        for (int pass = 0; pass < 2; ++pass) {
            stage.reset();
            float previousInput = 0, previousOutput = 0;
            for (int i = 0; i < 2048; ++i) {
                const float input = static_cast<float>(.2 + .3 * std::sin(.081 * i));
                const float alpha = 1.f / (1.f + 2.f * std::numbers::pi_v<float> * 8.f / rate);
                previousOutput = alpha * (previousOutput + input - previousInput);
                previousInput = input;
                const float actual = stage.processSample(input);
                EXPECT_FLOAT_EQ(actual, previousOutput);
                if (!pass)
                    first[i] = actual;
                else
                    EXPECT_FLOAT_EQ(actual, first[i]);
            }
        }
    }
}

TEST(NumericalRecoveryTest, VcaResetsAllStagesAfterBadAudioEnvelopeOrGate) {
    for (double rate : {44100., 48000., 192000.}) {
        for (float bad :
             {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
              -std::numeric_limits<float>::infinity()}) {
            for (int source = 0; source < 3; ++source) {
                SCOPED_TRACE(rate);
                SCOPED_TRACE(source);
                cs01::ParameterState state;
                state.set(Param::VcaEgDepth, .75f);
                VCAProcessor vca(state), reference(state);
                vca.prepare(rate);
                reference.prepare(rate);
                // Exercise a populated chain, then compare recovery to a fresh chain.
                for (int i = 0; i < 64; ++i)
                    vca.processSample(std::sin(i * .13f), .8f);
                EXPECT_FLOAT_EQ(vca.processSample(source == 0 ? bad : .5f, source == 1 ? bad : .8f,
                                                  source == 2 ? bad : 1.f),
                                0.f);
                double energy = 0;
                for (int i = 0; i < 1024; ++i) {
                    const float input = std::sin(i * .13f);
                    const float output = vca.processSample(input, .8f);
                    ASSERT_TRUE(std::isfinite(output));
                    EXPECT_FLOAT_EQ(output, reference.processSample(input, .8f));
                    energy += output * output;
                }
                EXPECT_GT(energy, .01);
            }
        }
    }
}

TEST(NumericalRecoveryTest, OriginalFilterResetsCouplingOnSampleAndModulatedBlockPaths) {
    for (double rate : {44100., 48000., 192000.}) {
        for (float bad :
             {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
              -std::numeric_limits<float>::infinity()}) {
            for (bool block : {false, true}) {
                CS01VCFCircuit circuit(rate), reference(rate);
                circuit.prepare(rate);
                reference.prepare(rate);
                for (int i = 0; i < 64; ++i)
                    circuit.processSample(0, std::sin(i * .13f));
                float sample = bad, cutoff = 1000;
                if (block)
                    circuit.processBlock(&sample, 1, &cutoff, .1f);
                else
                    sample = circuit.processSample(0, bad);
                EXPECT_FLOAT_EQ(sample, 0.f);
                double energy = 0;
                for (int i = 0; i < 1024; ++i) {
                    const float input = std::sin(i * .13f);
                    const float output = circuit.processSample(0, input);
                    ASSERT_TRUE(std::isfinite(output));
                    EXPECT_FLOAT_EQ(output, reference.processSample(0, input));
                    energy += output * output;
                }
                EXPECT_GT(energy, .01);
            }
        }
    }
}

TEST(NumericalRecoveryTest, RejectedNonfiniteDepthLeavesVcaSmoothingUsable) {
    cs01::ParameterState state, referenceState;
    state.set(Param::VcaEgDepth, .25f);
    referenceState.set(Param::VcaEgDepth, .25f);
    VCAProcessor vca(state), reference(referenceState);
    vca.prepare(48000);
    reference.prepare(48000);
    for (float bad :
         {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        state.set(Param::VcaEgDepth, bad);
        EXPECT_FLOAT_EQ(state.get(Param::VcaEgDepth), .25f);
        for (int i = 0; i < 64; ++i) {
            const float input = std::sin(i * .1f);
            EXPECT_FLOAT_EQ(vca.processSample(input, 1.f), reference.processSample(input, 1.f));
        }
    }
    state.set(Param::VcaEgDepth, 1.f);
    referenceState.set(Param::VcaEgDepth, 1.f);
    double energy = 0;
    for (int i = 0; i < 1280; ++i) {
        const float input = std::sin(i * .1f);
        const float output = vca.processSample(input, 1.f);
        ASSERT_TRUE(std::isfinite(output));
        EXPECT_FLOAT_EQ(output, reference.processSample(input, 1.f));
        energy += output * output;
    }
    EXPECT_GT(energy, .01);
}

TEST(NumericalRecoveryTest, ModulatedOriginalFilterFallsBackAndRestoresBaseControls) {
    CS01VCFCircuit circuit(48000), reference(48000);
    circuit.prepare(48000);
    reference.prepare(48000);
    circuit.setCutoffFrequency(730.f);
    reference.setCutoffFrequency(730.f);
    circuit.setResonance(.35f);
    reference.setResonance(.35f);
    constexpr auto nan = std::numeric_limits<float>::quiet_NaN();
    const std::array<float, 8> cutoff{1000.f, nan, 450.f, nan, 1200.f, 500.f, nan, 800.f};
    std::array<float, 8> samples{}, expected{};
    for (int i = 0; i < 8; ++i) {
        const float input = std::sin(i * .13f);
        samples[i] = input;
        reference.setCutoffFrequency(std::isfinite(cutoff[i]) ? cutoff[i] : 730.f);
        expected[i] = reference.processSample(0, input);
    }
    circuit.processBlock(samples.data(), 8, cutoff.data(), nan);
    for (int i = 0; i < 8; ++i) {
        ASSERT_TRUE(std::isfinite(samples[i]));
        EXPECT_FLOAT_EQ(samples[i], expected[i]);
    }
    // Temporary per-sample controls must not become the next block's defaults.
    reference.setCutoffFrequency(730.f);
    for (int i = 0; i < 256; ++i) {
        const float input = std::sin(i * .17f);
        EXPECT_FLOAT_EQ(circuit.processSample(0, input), reference.processSample(0, input));
    }
}

TEST(NoiseRegressionTest, InitialSilenceChangedNoteAndExact49SampleRelease) {
    cs01::ParameterState state;
    NoiseGenerator noise(state);
    noise.prepare(48000);
    EXPECT_FALSE(noise.isActive());
    EXPECT_EQ(noise.currentNote(), 0);
    for (int i = 0; i < 512; ++i)
        EXPECT_FLOAT_EQ(noise.renderSample(), 0);
    noise.startNote(60, 1, 8192);
    EXPECT_TRUE(noise.isActive());
    noise.changeNote(64);
    EXPECT_EQ(noise.currentNote(), 64);
    double energy = 0;
    for (int i = 0; i < 512; ++i) {
        float x = noise.renderSample();
        energy += x * x;
    }
    EXPECT_GT(energy, 0);
    noise.stopNote(false);
    EXPECT_EQ(noise.currentNote(), 0);
    EXPECT_FLOAT_EQ(noise.renderSample(), 0);
    noise.startNote(60, 1, 8192);
    noise.stopNote(true);
    noise.setReleaseSamplesRemaining(49);
    for (int i = 0; i < 256; ++i) {
        float sample = noise.renderSample();
        if (i >= 49)
            EXPECT_FLOAT_EQ(sample, 0);
    }
    EXPECT_FALSE(noise.isActive());
}
