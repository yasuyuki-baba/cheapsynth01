#include "cs01synth/EGProcessor.h"
#include "SynthEngine.h"
#include "cs01synth/IG02600BehavioralModel.h"
#include "cs01synth/ToneGenerator.h"
#include "cs01synth/NoiseGenerator.h"
#include <gtest/gtest.h>
#include <cmath>
#include <bit>
#include <vector>

namespace {
using cs01::Param;
void settings(cs01::ParameterState& state) {
    state.set(Param::Attack, .1f);
    state.set(Param::Decay, .1f);
    state.set(Param::Sustain, .4f);
    state.set(Param::Release, .1f);
}
void advance(EGProcessor& eg, int count) {
    while (count-- > 0)
        eg.processSample();
}
}  // namespace

// Assertions ported from main's EGProcessorTest. These establish software
// trajectory continuity, not calibration against the physical CS01 envelope.
TEST(EGContinuityTest, RetriggerDuringAttackKeepsTheHeldTrajectory) {
    for (double rate : {44100., 48000., 96000.}) {
        for (double fraction : {.2, .7, .95}) {
            SCOPED_TRACE(rate);
            SCOPED_TRACE(fraction);
            cs01::ParameterState state;
            settings(state);
            EGProcessor held(state), retriggered(state);
            held.prepareToPlay(rate, 1);
            retriggered.prepareToPlay(rate, 1);
            held.startEnvelope();
            retriggered.startEnvelope();
            const int boundary = static_cast<int>(fraction * 2 * state.get(Param::Attack) * rate);
            for (int i = 0; i < static_cast<int>(rate * 1.2); ++i) {
                if (i == boundary)
                    retriggered.startEnvelope();
                ASSERT_NEAR(held.processSample(), retriggered.processSample(), 2.e-6f) << i;
            }
        }
    }
}

TEST(EGContinuityTest, TimeEditsKeepIntegratedProgressInEveryMovingStage) {
    for (double rate : {44100., 48000., 96000., 192000.}) {
        for (auto id : {Param::Attack, Param::Decay, Param::Release}) {
            SCOPED_TRACE(rate);
            SCOPED_TRACE(static_cast<int>(id));
            cs01::ParameterState state;
            settings(state);
            EGProcessor eg(state);
            eg.prepareToPlay(rate, 1);
            eg.startEnvelope();
            if (id == Param::Decay)
                advance(eg,
                        static_cast<int>(std::ceil(2 * double(state.get(Param::Attack)) * rate)));
            if (id == Param::Release) {
                advance(eg, static_cast<int>(rate * .3));
                eg.releaseEnvelope();
            }
            const double initial = eg.getLastOutputForTesting();
            const double endpoint = id == Param::Attack ? 1. : id == Param::Decay ? .4 : 0.;
            const int first = static_cast<int>(rate * .04);
            const double oldTime = state.get(id);
            advance(eg, first);
            state.set(id, .2f);
            const double newTime = state.get(id);
            const int second = static_cast<int>(rate * .06);
            advance(eg, second);
            const double progress = first / (rate * oldTime) + second / (rate * newTime);
            const double target = id == Param::Attack ? 1. / (1. - std::exp(-2.)) : endpoint;
            const double expected = target + (initial - target) * std::exp(-progress);
            EXPECT_NEAR(eg.getLastOutputForTesting(), expected, 2.e-5);
            advance(eg, static_cast<int>(
                            std::ceil(((id == Param::Attack ? 2. : std::log(10000.)) - progress) *
                                      newTime * rate)) +
                            1);
            EXPECT_NEAR(eg.getLastOutputForTesting(), endpoint, .001);
            if (id == Param::Release) {
                EXPECT_FALSE(eg.isActive());
                EXPECT_FLOAT_EQ(eg.getNoteGate(), 0);
            }
        }
    }
}

TEST(EGContinuityTest, RetriggerFromReleaseUsesResidualThresholdTime) {
    for (double rate : {44100., 48000., 96000.}) {
        for (double releaseAge : {.01, .05, .15}) {
            cs01::ParameterState state;
            settings(state);
            state.set(Param::Release, .2f);
            EGProcessor eg(state);
            eg.prepareToPlay(rate, 1);
            eg.startEnvelope();
            advance(eg, static_cast<int>(rate * .3));
            eg.releaseEnvelope();
            advance(eg, static_cast<int>(rate * releaseAge));
            const float initial = eg.getLastOutputForTesting();
            ASSERT_GT(initial, 0);
            eg.startEnvelope();
            EXPECT_FLOAT_EQ(eg.getLastOutputForTesting(), initial);
            const double time = state.get(Param::Attack);
            const double target = 1. / (1. - std::exp(-2.));
            const double expected = time * std::log((target - initial) / (target - 1.));
            int count = 0;
            do {
                eg.processSample();
                ++count;
            } while (eg.getLastOutputForTesting() < 1 && count <= 2 * time * rate + 2);
            EXPECT_NEAR(count / rate, expected, 2. / rate);
            EXPECT_LT(count / rate, 2 * time);
        }
    }
}

TEST(EGContinuityTest, ZeroSustainReleaseKeepsTheIndependentGateClock) {
    for (double rate : {44100., 48000., 96000.}) {
        cs01::ParameterState state;
        settings(state);
        state.set(Param::Attack, .01f);
        state.set(Param::Decay, .01f);
        state.set(Param::Sustain, 0);
        EGProcessor eg(state);
        eg.prepareToPlay(rate, 1);
        eg.startEnvelope();
        advance(eg, static_cast<int>(rate * .12));
        ASSERT_FLOAT_EQ(eg.getLastOutputForTesting(), 0);
        ASSERT_FLOAT_EQ(eg.getNoteGate(), 1);
        eg.releaseEnvelope();
        advance(eg, static_cast<int>(rate * .04));
        ASSERT_TRUE(eg.isActive());
        EXPECT_NEAR(eg.getNoteGate(), 1 - .04 / (.1 * std::log(10000.)), .001f);
        state.set(Param::Release, .2f);
        const int remaining = static_cast<int>(eg.getReleaseSamplesRemaining());
        EXPECT_NEAR(remaining / rate, (.1 * std::log(10000.) - .04) * 2, 3. / rate);
        advance(eg, remaining - 1);
        EXPECT_TRUE(eg.isActive());
        eg.processSample();
        EXPECT_FALSE(eg.isActive());
        EXPECT_FLOAT_EQ(eg.getNoteGate(), 0);
    }
}

TEST(EGContinuityTest, AttackAndDecayChangesDoNotAlterRunningRelease) {
    for (double rate : {44100., 48000., 96000.}) {
        cs01::ParameterState original, edited;
        settings(original);
        settings(edited);
        EGProcessor a(original), b(edited);
        a.prepareToPlay(rate, 1);
        b.prepareToPlay(rate, 1);
        a.startEnvelope();
        b.startEnvelope();
        advance(a, static_cast<int>(rate * .025));
        advance(b, static_cast<int>(rate * .025));
        a.releaseEnvelope();
        b.releaseEnvelope();
        for (int i = 0; i < static_cast<int>(rate * .2); ++i) {
            if (i == static_cast<int>(rate * .05)) {
                edited.set(Param::Attack, .2f);
                edited.set(Param::Decay, .2f);
            }
            ASSERT_FLOAT_EQ(a.processSample(), b.processSample());
            ASSERT_FLOAT_EQ(a.getNoteGate(), b.getNoteGate());
        }
    }
}

TEST(EGContinuityTest, SustainEditsAndLifecycleRemainBounded) {
    cs01::ParameterState state;
    settings(state);
    EGProcessor eg(state);
    eg.prepareToPlay(48000, 1);
    eg.startEnvelope();
    advance(eg, 57600);
    for (float target : {.8f, .2f, 1.f, 0.f, .5f}) {
        const float before = eg.getLastOutputForTesting();
        state.set(Param::Sustain, target);
        EXPECT_LT(std::abs(eg.processSample() - before), .001f);
        for (int i = 0; i < 48000; ++i) {
            const float value = eg.processSample();
            ASSERT_TRUE(std::isfinite(value));
            ASSERT_GE(value, 0);
            ASSERT_LE(value, 1);
        }
        EXPECT_NEAR(eg.getLastOutputForTesting(), target, 1.e-6f);
    }
    eg.releaseEnvelope();
    advance(eg, 1000);
    eg.releaseResources();
    EXPECT_FALSE(eg.isActive());
    EXPECT_FLOAT_EQ(eg.getNoteGate(), 0);
    EXPECT_FLOAT_EQ(eg.getLastOutputForTesting(), 0);
    eg.startEnvelope();
    advance(eg, 1000);
    eg.prepareToPlay(96000, 1);
    EXPECT_FLOAT_EQ(eg.processSample(), 0);
    EXPECT_FLOAT_EQ(eg.getNoteGate(), 0);
}

TEST(VcaGateTest, GateCorrectionLeavesEgComponentAndOpenGateArithmeticIntact) {
    IG02600BehavioralModel model;
    for (float input : {-.9f, -.1f, .1f, .9f}) {
        for (float envelope : {0.f, .3f, 1.f}) {
            for (float depth : {0.f, .3f, 1.f}) {
                // Frozen pre-gate gain/saturation expression from the JUCE baseline.
                float voltage = (1.f - depth) + envelope * depth;
                voltage *= (1.f - .4f) + .7f * .4f;
                float expected = input * (voltage * .8f);
                if (std::abs(expected) > .7f) {
                    const float excess = std::abs(expected) - .7f;
                    expected = (expected > 0 ? 1.f : -1.f) * (.7f + excess / (1.f + excess * .5f));
                }
                EXPECT_EQ(std::bit_cast<uint32_t>(
                              model.processSample(input, envelope, depth, .7f, .4f, .8f, 1)),
                          std::bit_cast<uint32_t>(expected));
            }
            EXPECT_FLOAT_EQ(model.processSample(input, envelope, 1, .7f, .4f, .8f, 0),
                            model.processSample(input, envelope, 1, .7f, .4f, .8f, 1));
        }
        EXPECT_FLOAT_EQ(model.processSample(input, 0, 0, 0, 0, 1, 0), 0);
    }
}

TEST(SourceContinuityTest, NoiseProgressesAcrossIdleAndReleaseDeadline) {
    cs01::ParameterState state;
    state.set(Param::Release, .01f);
    NoiseGenerator continuous(state), lifecycle(state);
    continuous.prepare(48000);
    lifecycle.prepare(48000);
    continuous.startNote(60, 1, 8192);
    for (int i = 0; i < 2400; ++i) {
        if (i == 240)
            lifecycle.startNote(60, 1, 8192);
        if (i == 480)
            lifecycle.stopNote(true);
        if (i == 1200)
            lifecycle.startNote(60, 1, 8192);
        ASSERT_FLOAT_EQ(continuous.renderContinuousSample(), lifecycle.renderContinuousSample());
        if (i == 1100)
            EXPECT_FALSE(lifecycle.isActive());
    }
}

TEST(SourceContinuityTest, ToneProgressesAcrossIdleAndReleaseDeadline) {
    cs01::ParameterState state;
    state.set(Param::Release, .01f);
    ToneGenerator continuous(state), lifecycle(state);
    continuous.prepare(48000);
    lifecycle.prepare(48000);
    continuous.startNote(60, 1, 8192);
    for (int i = 0; i < 2400; ++i) {
        if (i == 240)
            lifecycle.startNote(60, 1, 8192);
        if (i == 480)
            lifecycle.stopNote(true);
        if (i == 1200)
            lifecycle.startNote(60, 1, 8192);
        continuous.updateBlockRateParameters();
        lifecycle.updateBlockRateParameters();
        ASSERT_FLOAT_EQ(continuous.renderContinuousSample(), lifecycle.renderContinuousSample());
        if (i == 1100)
            EXPECT_FALSE(lifecycle.isActive());
    }
}

TEST(SourceContinuityTest, ExtendedReleaseSurvivesSourceSwitchPastOriginalDeadline) {
    for (bool startNoise : {false, true}) {
        cs01::ParameterState state;
        state.set(Param::Attack, .001f);
        state.set(Param::Decay, .001f);
        state.set(Param::Sustain, 1);
        state.set(Param::Release, .1f);
        state.set(Param::VcaEgDepth, 0);
        state.set(Param::Volume, 1);
        state.set(Param::Feet, startNoise ? 4 : 2);
        SynthEngine engine(state);
        engine.prepare(48000);
        engine.handleMidi({0x90, 69, 100});
        for (int i = 0; i < 1000; ++i)
            engine.renderSample();
        engine.handleMidi({0x80, 69, 0});
        for (int i = 0; i < 1920; ++i)
            engine.renderSample();
        state.set(Param::Release, .4f);
        for (int i = 0; i < 3360; ++i)
            engine.renderSample();
        state.set(Param::Feet, startNoise ? 2 : 4);
        double energy = 0;
        for (int i = 0; i < 1024; ++i) {
            const float value = engine.renderSample();
            ASSERT_TRUE(std::isfinite(value));
            energy += value * value;
        }
        EXPECT_GT(energy, .001);
        state.set(Param::Release, .001f);
        for (int i = 0; i < 48000; ++i)
            engine.renderSample();
        energy = 0;
        for (int i = 0; i < 256; ++i) {
            const float value = engine.renderSample();
            energy += value * value;
        }
        EXPECT_LT(energy, 1.e-12);
    }
}

TEST(EGContinuityTest, LiveAttackEditPreservesBoundedMonotonicProgress) {
    for (double rate : {44100., 48000., 96000.}) {
        cs01::ParameterState state;
        settings(state);
        EGProcessor eg(state);
        eg.prepareToPlay(rate, 64);
        eg.startEnvelope();
        advance(eg, static_cast<int>(rate * .02));
        float previous = eg.getLastOutputForTesting();
        ASSERT_GT(previous, 0.f);
        state.set(Param::Attack, .5f);
        for (int i = 0; i < 256; ++i) {
            const float value = eg.processSample();
            ASSERT_TRUE(std::isfinite(value));
            EXPECT_GE(value, previous);
            EXPECT_LE(value, 1.f);
            EXPECT_LT(value - previous, .001f);
            previous = value;
        }
        EXPECT_TRUE(eg.isActive());
    }
}

TEST(EGContinuityTest, ReleaseTimeEditKeepsResidualBranchTargetWithZeroSustain) {
    for (double rate : {44100., 48000., 96000.}) {
        for (float duration : {.05f, .2f}) {
            cs01::ParameterState state;
            settings(state);
            state.set(Param::Sustain, 0.f);
            state.set(Param::Release, .5f);
            EGProcessor eg(state);
            eg.prepareToPlay(rate, 1);
            eg.startEnvelope();
            advance(eg, static_cast<int>(rate * .025));
            const double branchTarget = 0;
            eg.releaseEnvelope();
            advance(eg, static_cast<int>(rate * .05));
            const float initial = eg.getLastOutputForTesting();
            ASSERT_GT(initial, 0.f);
            state.set(Param::Release, duration);
            EXPECT_NEAR(eg.processSample(),
                        branchTarget + (initial - branchTarget) *
                                           std::exp(-1. / (state.get(Param::Release) * rate)),
                        1e-5);
            EXPECT_TRUE(eg.isActive());
            const int halfway = static_cast<int>(duration * rate * .5);
            advance(eg, halfway - 1);
            EXPECT_NEAR(eg.getLastOutputForTesting(),
                        branchTarget + (initial - branchTarget) * std::exp(-.5), initial * .003);
            advance(eg, static_cast<int>(duration * rate * 10));
            EXPECT_FALSE(eg.isActive());
            EXPECT_FLOAT_EQ(eg.getLastOutputForTesting(), 0.f);
        }
    }
}

TEST(EGContinuityTest, RetriggerRestoresEditedSustainDuringAndAfterRelease) {
    for (double rate : {44100., 48000., 96000.}) {
        for (bool finishRelease : {false, true}) {
            cs01::ParameterState state;
            settings(state);
            state.set(Param::Decay, .3f);
            state.set(Param::Sustain, .6f);
            state.set(Param::Release, .5f);
            EGProcessor eg(state);
            eg.prepareToPlay(rate, 1);
            eg.startEnvelope();
            advance(eg, static_cast<int>(rate * .025));
            eg.releaseEnvelope();
            advance(eg, static_cast<int>(rate * .05));
            state.set(Param::Release, .2f);
            advance(eg, static_cast<int>(rate * .001));
            ASSERT_TRUE(eg.isActive());
            if (finishRelease) {
                advance(eg, static_cast<int>(rate * 2.1));
                ASSERT_FALSE(eg.isActive());
            }
            state.set(Param::Sustain, .8f);
            eg.startEnvelope();
            advance(eg, static_cast<int>(rate * 3.1));
            EXPECT_TRUE(eg.isActive());
            EXPECT_NEAR(eg.getLastOutputForTesting(), .8f, 1e-6);
            eg.releaseEnvelope();
            advance(eg, static_cast<int>(rate * .1));
            const double halfway = .8 * std::exp(-.5);
            EXPECT_NEAR(eg.getLastOutputForTesting(), halfway, .002);
            advance(eg, static_cast<int>(rate * 2.));
            EXPECT_FALSE(eg.isActive());
            EXPECT_FLOAT_EQ(eg.getLastOutputForTesting(), 0.f);
        }
    }
}

TEST(EGContinuityTest, LifecycleClearsObservationGateAndRepreparesAtOtherRates) {
    for (bool duringRelease : {false, true}) {
        cs01::ParameterState state;
        settings(state);
        EGProcessor eg(state);
        eg.prepareToPlay(48000, 64);
        eg.startEnvelope();
        advance(eg, 64);
        if (duringRelease) {
            eg.releaseEnvelope();
            advance(eg, 64);
        }
        ASSERT_TRUE(eg.isActive());
        ASSERT_GT(eg.getLastOutputForTesting(), 0.f);
        eg.releaseResources();
        EXPECT_FALSE(eg.isActive());
        EXPECT_FLOAT_EQ(eg.getLastOutputForTesting(), 0.f);
        EXPECT_FLOAT_EQ(eg.getNoteGate(), 0.f);
        eg.prepareToPlay(96000, 64);
        for (int i = 0; i < 64; ++i)
            EXPECT_FLOAT_EQ(eg.processSample(), 0.f);
        eg.startEnvelope();
        advance(eg, 64);
        eg.prepareToPlay(44100, 64);
        EXPECT_FALSE(eg.isActive());
        EXPECT_FLOAT_EQ(eg.getLastOutputForTesting(), 0.f);
        EXPECT_FLOAT_EQ(eg.getNoteGate(), 0.f);
    }
}

TEST(EGContinuityTest, ProductionRangeStageDurationsIncludingMaximumInternalRate) {
    for (double rate : {44100., 48000., 96000., 192000., 384000.})
        for (float position : {0.f, .5f, 1.f}) {
            cs01::ParameterState state;
            for (auto id : {Param::Attack, Param::Decay, Param::Release})
                state.set(id, cs01::fromNormalized(id, position));
            state.set(Param::Sustain, .5f);
            EGProcessor eg(state);
            eg.prepareToPlay(rate, 256);
            eg.startEnvelope();
            auto duration = [&](auto done) {
                for (int n = 1; n <= static_cast<int>(rate * 42); ++n)
                    if (done(eg.processSample()))
                        return n;
                return static_cast<int>(rate * 42) + 1;
            };
            auto check = [&](Param id, int count) {
                double expected = state.get(id) * (id == Param::Attack ? 2. : std::log(10000.));
                EXPECT_NEAR(count / rate, expected, expected * .01 + 2 / rate);
            };
            check(Param::Attack, duration([](float x) { return x >= 1; }));
            check(Param::Decay, duration([](float x) { return x <= .5f; }));
            eg.releaseEnvelope();
            check(Param::Release, duration([](float x) { return x == 0; }));
        }
}
TEST(EGContinuityTest, RepeatedEditsRetriggersAndGateClockArePartitionIndependent) {
    for (double rate : {44100., 48000., 96000.}) {
        auto render = [&](int block, bool gate) {
            cs01::ParameterState state;
            settings(state);
            if (gate)
                state.set(Param::Release, .01f);
            EGProcessor eg(state);
            eg.prepareToPlay(rate, block);
            std::vector<float> output;
            std::vector<int> boundaries =
                gate ? std::vector<int>{0, 100, 200, 300, 400, 12000}
                     : std::vector<int>{0, 200, 400, 600, 800, 1000, 1200, 240000};
            for (size_t event = 0; event + 1 < boundaries.size(); ++event) {
                if (gate) {
                    if (event == 0 || event == 3)
                        eg.startEnvelope();
                    if (event == 1 || event == 4)
                        eg.releaseEnvelope();
                    if (event == 2)
                        state.set(Param::Release, .005f);
                } else {
                    if (event == 0 || event == 5)
                        eg.startEnvelope();
                    if (event == 3 || event == 6)
                        eg.releaseEnvelope();
                    if (event == 1 || event == 2 || event == 4)
                        state.set(event == 4 ? Param::Release : Param::Attack,
                                  event == 2 ? .05f : .2f);
                }
                for (int i = boundaries[event]; i < boundaries[event + 1];) {
                    int count = std::min(block, boundaries[event + 1] - i);
                    for (int j = 0; j < count; ++j) {
                        float value = eg.processSample();
                        output.push_back(gate ? eg.getNoteGate() : value);
                    }
                    i += count;
                }
            }
            return output;
        };
        for (bool gate : {false, true}) {
            auto expected = render(1, gate);
            for (int size : {7, 64, 256})
                EXPECT_EQ(render(size, gate), expected);
            if (gate) {
                EXPECT_GT(expected[99], .99f);
                EXPECT_GT(expected[399], expected[300]);
            } else
                EXPECT_GT(expected[599], 0);
            EXPECT_FLOAT_EQ(expected.back(), 0);
            for (float value : expected) {
                ASSERT_GE(value, 0);
                ASSERT_LE(value, 1);
            }
        }
    }
}
TEST(EGContinuityTest, EarlyReleaseUsesNoteOffLevelAndRateWithZeroSustain) {
    for (double rate : {44100., 48000., 96000.}) {
        cs01::ParameterState state;
        settings(state);
        state.set(Param::Sustain, 0);
        EGProcessor eg(state);
        eg.prepareToPlay(rate, 1);
        eg.startEnvelope();
        advance(eg, static_cast<int>(rate * .037));
        float initial = eg.getLastOutputForTesting();
        ASSERT_GT(initial, 0);
        eg.releaseEnvelope();
        EXPECT_LE(eg.processSample(), initial);
        advance(eg, static_cast<int>(rate * .05) - 1);
        double expected = initial * std::exp(-.5);
        EXPECT_NEAR(eg.getLastOutputForTesting(), expected, initial * .002);
        EXPECT_TRUE(eg.isActive());
        advance(eg, static_cast<int>(rate * 1.));
        EXPECT_FALSE(eg.isActive());
        EXPECT_FLOAT_EQ(eg.getLastOutputForTesting(), 0);
    }
}

TEST(EGContinuityTest, ContinuousAttackDecayAndReleaseRetainSlopeBoundsIncludingSettlingThreshold) {
    for (double rate : {44100., 48000., 96000.}) {
        cs01::ParameterState state;
        settings(state);
        state.set(Param::Sustain, .5f);
        state.set(Param::Release, .5f);
        EGProcessor eg(state);
        eg.prepareToPlay(rate, 1);
        eg.startEnvelope();
        float previous = 0;
        bool reachedPeak = false;
        for (int i = 0; i < static_cast<int>(rate * 1.2); ++i) {
            float value = eg.processSample();
            ASSERT_TRUE(std::isfinite(value));
            ASSERT_GE(value, 0);
            ASSERT_LE(value, 1);
            ASSERT_LE(std::abs(value - previous), 2.32 / (.1 * rate) + .00001);
            if (!reachedPeak) {
                ASSERT_GE(value + .000001f, previous);
                if (value >= 1)
                    reachedPeak = true;
            } else
                ASSERT_LE(value, previous + .000001f);
            previous = value;
        }
        EXPECT_TRUE(reachedPeak);
        EXPECT_NEAR(previous, .5f, .00001f);
        eg.releaseEnvelope();
        for (int i = 0; i < static_cast<int>(rate * 5); ++i) {
            float value = eg.processSample();
            ASSERT_GE(value, 0);
            ASSERT_LE(value, previous + .000001f);
            ASSERT_LE(std::abs(value - previous),
                      1.16 / (.5 * rate) + .5 * EGTiming::settlingRatio + .00001);
            previous = value;
        }
        EXPECT_FLOAT_EQ(previous, 0);
        EXPECT_FALSE(eg.isActive());
    }
}

TEST(EGTimingTest, AssumedATaperMatchesCircuitAndRoundTrips) {
    EXPECT_NEAR(EGTiming::timeConstant(0), .00396, 1e-12);
    EXPECT_NEAR(EGTiming::timeConstant(.5), .44396, 1e-12);
    EXPECT_NEAR(EGTiming::timeConstant(1), 4.40396, 1e-12);
    for (auto id : {Param::Attack, Param::Decay, Param::Release})
        for (int i = 0; i <= 1000; ++i) {
            const float position = i / 1000.f;
            EXPECT_NEAR(cs01::toNormalized(id, cs01::fromNormalized(id, position)), position, 1e-6);
        }
}
