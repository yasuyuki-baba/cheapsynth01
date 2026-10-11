#include "cs01synth/ToneGenerator.h"
#include "cs01synth/VCOProcessor.h"
#include "cs01synth/MidiProcessor.h"
#include "cs01synth/WaveformStrategies.h"
#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <vector>

namespace {
using cs01::Param;
struct ToneHarness {
    cs01::ParameterState state;
    ToneGenerator tone{state};
    ToneHarness(int wave, double rate, double frequency = 440.) {
        state.set(Param::WaveType, wave);
        state.set(Param::Feet, 2);
        tone.prepare(rate);
        tone.startNote(69, 1, 8192);
        tone.setPitchBendSemitones(static_cast<float>(12 * std::log2(frequency / 440.)));
        tone.updateBlockRateParameters();
    }
};
double measurePitch(ToneGenerator& tone, double rate) {
    for (int i = 0; i < static_cast<int>(rate * .1); ++i)
        tone.getNextSample();
    std::vector<double> crossings;
    float previous = tone.getNextSample();
    for (int i = 0; i < static_cast<int>(rate * .5) && crossings.size() < 65u; ++i) {
        const float value = tone.getNextSample();
        if (!std::isfinite(value))
            return 0;
        if (previous < 0 && value >= 0)
            crossings.push_back(i - 1. - previous / (value - previous));
        previous = value;
    }
    return crossings.size() == 65u
               ? rate * (crossings.size() - 1) / (crossings.back() - crossings.front())
               : 0;
}
void advance(VCOProcessor& vco, int count) {
    while (count-- > 0)
        vco.processSample(0);
}
}  // namespace

TEST(VcoBehaviorTest, GlissandoFollowsIndependentSemitoneScheduleAtBothProcessingRates) {
    for (double hostRate : {44100., 48000., 96000.})
        for (double multiplier : {1., 4.})
            for (int destination : {66, 72}) {
                const double rate = hostRate * multiplier;
                ToneHarness sliding(2, rate), reference(2, rate);
                sliding.state.set(Param::Glissando, .02f);
                sliding.tone.changeNote(destination);
                const int step = static_cast<int>(sliding.state.get(Param::Glissando) *
                                                  static_cast<float>(rate));
                double error = 0;
                for (int i = 0; i < step * 4; ++i) {
                    if ((i + 1) % step == 0 && (i + 1) / step <= 3)
                        reference.tone.setNote(69 + (destination > 69 ? 1 : -1) * ((i + 1) / step),
                                               false);
                    const float actual = sliding.tone.getNextSample();
                    ASSERT_TRUE(std::isfinite(actual));
                    error = std::max(error, std::abs(static_cast<double>(actual) -
                                                     reference.tone.getNextSample()));
                }
                EXPECT_NEAR(error, 0., 1e-6);
            }
}

TEST(VcoBehaviorTest, LiveGlissandoSpeedPreservesFractionalStepAndZeroJumpsToTarget) {
    for (double rate : {44100., 48000., 96000.})
        for (float duration : {.0078125f, .03125f, 0.f}) {
            ToneHarness sliding(2, rate), reference(2, rate);
            sliding.state.set(Param::Glissando, .015625f);
            sliding.tone.changeNote(72);
            const int oldStep =
                static_cast<int>(sliding.state.get(Param::Glissando) * static_cast<float>(rate));
            const int changeAt = oldStep / 2;
            const int newStep = static_cast<int>(duration * rate);
            const int remaining =
                newStep - static_cast<int>(static_cast<double>(changeAt) * newStep / oldStep);
            const int firstStep = changeAt + remaining - 1;
            double error = 0;
            for (int i = 0; i < static_cast<int>(rate * .2); ++i) {
                if (i == changeAt)
                    sliding.state.set(Param::Glissando, duration);
                if (duration == 0) {
                    if (i == changeAt)
                        reference.tone.setNote(72, false);
                } else
                    for (int step = 0; step < 3; ++step)
                        if (i == firstStep + step * newStep)
                            reference.tone.setNote(70 + step, false);
                const float actual = sliding.tone.getNextSample();
                ASSERT_TRUE(std::isfinite(actual));
                error = std::max(
                    error, std::abs(static_cast<double>(actual) - reference.tone.getNextSample()));
            }
            EXPECT_NEAR(error, 0., 1e-6);
        }
}

TEST(VcoBehaviorTest, HeldPitchBendTracksLiveRangeEdits) {
    for (double rate : {44100., 48000., 96000.})
        for (int pitchBendValue : {0, 16383}) {
            ToneHarness h(2, rate);
            h.state.set(Param::BendUp, 12);
            h.state.set(Param::BendDown, 12);
            h.tone.setPitchBend(pitchBendValue);
            for (float range : {7.f, 0.f, 12.f}) {
                h.state.set(pitchBendValue == 0 ? Param::BendDown : Param::BendUp, range);
                h.tone.updateBlockRateParameters();
                const double expected =
                    440. * std::exp2((pitchBendValue == 0 ? -range : range) / 12.);
                EXPECT_NEAR(measurePitch(h.tone, rate), expected, expected * .001);
                EXPECT_EQ(h.tone.getPlaybackState().pitchBend, pitchBendValue);
            }
        }
}

TEST(VcoBehaviorTest, ManualPitchAndMidiWheelAreAppliedOnce) {
    for (double rate : {44100., 48000., 96000.})
        for (float pitch : {-1.f, 0.f, 1.f})
            for (int pitchBendValue : {0, 8192, 16383}) {
                ToneHarness h(2, rate);
                h.state.set(Param::Pitch, pitch);
                h.state.set(Param::BendUp, 12);
                h.state.set(Param::BendDown, 12);
                MidiProcessor midi(h.state);
                midi.setSoundGenerator(&h.tone);
                midi.handleMidiEvent(cs01::MidiMessage::pitchBend(1, pitchBendValue));
                midi.handleMidiEvent({0x90, 69, 100});
                h.tone.updateBlockRateParameters();
                const float bend = pitchBendValue == 0 ? -1.f : pitchBendValue == 16383 ? 1.f : 0.f;
                EXPECT_FLOAT_EQ(h.state.get(Param::PitchBend), bend);
                const double expected = 440. * std::exp2((pitch + bend * 12) / 12.);
                EXPECT_NEAR(measurePitch(h.tone, rate), expected, expected * .001);
            }
}

TEST(VcoBehaviorTest, PwmTwoHertzRepeatsAtHalfSecondNotQuarterSecond) {
    for (double rate : {44100., 48000., 96000.}) {
        ToneHarness h(4, rate, 480);
        h.state.set(Param::PwmSpeed, 2);
        h.tone.prepare(rate);
        h.tone.startNote(69, 1, 8192);
        h.tone.setPitchBendSemitones(static_cast<float>(12 * std::log2(480. / 440.)));
        h.tone.updateBlockRateParameters();
        for (int i = 0; i < static_cast<int>(rate * 2); ++i)
            h.tone.getNextSample();
        std::vector<float> samples(static_cast<int>(rate));
        for (auto& v : samples)
            v = h.tone.getNextSample();
        std::array<double, 480> means{};
        for (int cycle = 0; cycle < 480; ++cycle) {
            const int begin = static_cast<int>(cycle * rate / 480.),
                      end = static_cast<int>((cycle + 1) * rate / 480.);
            for (int i = begin; i < end; ++i)
                means[cycle] += samples[i];
            means[cycle] /= end - begin;
            ASSERT_TRUE(std::isfinite(means[cycle]));
        }
        double repeatPower = 0, quarterPower = 0;
        for (int i = 0; i < 240; ++i) {
            repeatPower += std::pow(means[i] - means[i + 240], 2);
            quarterPower += std::pow(means[i] - means[i + 120], 2);
        }
        EXPECT_LT(repeatPower / 240, .001);
        EXPECT_GT(quarterPower / 240, .01);
    }
}

TEST(VcoBehaviorTest, PwmManualEndpointsKeepPeriodAtHostAndFourTimesRates) {
    for (double hostRate : {44100., 48000., 96000.})
        for (double multiplier : {1., 4.})
            for (float frequency : {.6f, 6.3f, 12.f}) {
                const double rate = hostRate * multiplier;
                SCOPED_TRACE(rate);
                SCOPED_TRACE(frequency);
                ToneHarness h(4, rate, 480);
                h.state.set(Param::PwmSpeed, frequency);
                h.tone.prepare(rate);
                h.tone.startNote(69, 1, 8192);
                h.tone.setPitchBendSemitones(static_cast<float>(12 * std::log2(480. / 440.)));
                h.tone.updateBlockRateParameters();
                for (int i = 0; i < static_cast<int>(rate); ++i)
                    h.tone.getNextSample();
                std::vector<double> crossings;
                double previous = 0;
                bool armed = false;
                int sampleIndex = 0;
                const int cycles = static_cast<int>(480. * 4 / frequency);
                for (int cycle = 0; cycle < cycles && crossings.size() < 3; ++cycle) {
                    const int end = static_cast<int>((cycle + 1) * rate / 480.);
                    const int count = end - sampleIndex;
                    double mean = 0;
                    while (sampleIndex < end) {
                        mean += h.tone.getNextSample();
                        ++sampleIndex;
                    }
                    mean /= count;
                    ASSERT_TRUE(std::isfinite(mean));
                    if (mean < -.1)
                        armed = true;
                    if (armed && cycle > 0 && previous < 0 && mean >= 0) {
                        crossings.push_back(cycle - 1 + (-previous / (mean - previous)));
                        armed = false;
                    }
                    previous = mean;
                }
                ASSERT_GE(crossings.size(), 3u);
                const double measured =
                    480. * (crossings.size() - 1) / (crossings.back() - crossings.front());
                EXPECT_NEAR(measured, frequency, frequency * .005);
            }
}

TEST(VcoBehaviorTest, WaveformTimeConstantsKeepEmpiricalDecayPerSecond) {
    WaveformTimeConstants constants;
    for (float rate : {44100.f, 48000.f, 96000.f}) {
        constants.update(rate);
        const auto check = [&](double actual, double reference) {
            EXPECT_NEAR(std::log(actual) * rate, std::log(reference) * 44100., .006);
        };
        check(constants.triangleLeak, static_cast<double>(.9999f));
        check(1. - constants.triangleDcAmount, 1. - static_cast<double>(.005f));
        check(constants.sawLeak, static_cast<double>(.998f));
        check(constants.pwmPole, static_cast<double>(.98f));
    }
    constants.update(44100.f);
    EXPECT_FLOAT_EQ(constants.triangleLeak, .9999f);
    EXPECT_FLOAT_EQ(constants.triangleDcAmount, .005f);
    EXPECT_FLOAT_EQ(constants.sawLeak, .998f);
    EXPECT_FLOAT_EQ(constants.pwmPole, .98f);
}

TEST(VcoBehaviorTest, HeldNoteSurvivesRepeatedNoiseRoundTrips) {
    cs01::ParameterState state;
    state.set(Param::Feet, 2);
    VCOProcessor vco(state);
    vco.prepare(48000);
    auto* original = vco.getSoundGenerator();
    original->startNote(69, 1, 8192);
    for (int selection : {4, 2, 4, 2}) {
        state.set(Param::Feet, selection);
        vco.applyPendingGeneratorChange();
        auto* selected = vco.getSoundGenerator();
        EXPECT_EQ(selected == original, selection == 2);
        EXPECT_TRUE(selected->isActive());
        EXPECT_EQ(selected->currentNote(), 69);
        double energy = 0;
        for (int i = 0; i < 256; ++i) {
            const float v = vco.processSample(0);
            ASSERT_TRUE(std::isfinite(v));
            energy += v * v;
        }
        EXPECT_GT(energy, 0);
    }
    vco.getSoundGenerator()->stopNote(false);
    EXPECT_FALSE(vco.getSoundGenerator()->isActive());
}

TEST(VcoBehaviorTest, SwitchingPreservesWheelNoteAndRemainingReleaseDeadline) {
    cs01::ParameterState state;
    state.set(Param::Feet, 2);
    state.set(Param::Release, .1f);
    VCOProcessor vco(state);
    vco.prepare(48000);
    vco.getSoundGenerator()->startNote(69, 1, 12000);
    state.set(Param::Feet, 4);
    vco.applyPendingGeneratorChange();
    EXPECT_EQ(vco.getSoundGenerator()->getPlaybackState().pitchBend, 12000);
    vco.getSoundGenerator()->setPitchBend(15000);
    vco.getSoundGenerator()->stopNote(true);
    advance(vco, 256);
    const auto before = vco.getSoundGenerator()->getPlaybackState();
    EXPECT_FALSE(before.held);
    EXPECT_NEAR(before.releaseSecondsRemaining, .1 - 256. / 48000, 1. / 48000);
    state.set(Param::Feet, 2);
    vco.applyPendingGeneratorChange();
    const auto after = vco.getSoundGenerator()->getPlaybackState();
    EXPECT_FALSE(after.held);
    EXPECT_EQ(after.note, 69);
    EXPECT_EQ(after.pitchBend, 15000);
    EXPECT_NEAR(after.releaseSecondsRemaining, before.releaseSecondsRemaining, 1. / 48000);
    advance(vco, 48000);
    EXPECT_FALSE(vco.getSoundGenerator()->isActive());
}

TEST(VcoBehaviorTest, NewNoteCancelsOldReleaseDeadlineForBothSources) {
    cs01::ParameterState state;
    state.set(Param::Release, .1f);
    VCOProcessor vco(state);
    vco.prepare(48000);
    for (int selection : {2, 4}) {
        state.set(Param::Feet, selection);
        vco.applyPendingGeneratorChange();
        auto* generator = vco.getSoundGenerator();
        generator->startNote(60, 1, 8192);
        generator->stopNote(true);
        advance(vco, 256);
        generator->startNote(69, 1, 8192);
        advance(vco, 40 * 256);
        EXPECT_TRUE(generator->isActive());
        EXPECT_EQ(generator->currentNote(), 69);
        EXPECT_TRUE(generator->getPlaybackState().held);
        generator->stopNote(true);
        advance(vco, 40 * 256);
        EXPECT_FALSE(generator->isActive());
    }
}

TEST(VcoBehaviorTest, UnmatchedNoteOffCannotRestartReleaseClock) {
    cs01::ParameterState state;
    VCOProcessor vco(state);
    vco.prepare(48000);
    MidiProcessor midi(state);
    midi.setSoundGenerator(vco.getSoundGenerator());
    midi.handleMidiEvent({0x90, 69, 100});
    midi.handleMidiEvent({0x80, 69, 0});
    advance(vco, 256);
    const auto before = vco.getSoundGenerator()->getPlaybackState();
    midi.handleMidiEvent({0x80, 60, 0});
    const auto after = vco.getSoundGenerator()->getPlaybackState();
    EXPECT_DOUBLE_EQ(after.releaseSecondsRemaining, before.releaseSecondsRemaining);
    EXPECT_FALSE(after.held);
}

TEST(VcoBehaviorTest, IdleAndExpiredSourceSwitchesKeepHeldStateCleared) {
    cs01::ParameterState state;
    state.set(Param::Feet, 2);
    state.set(Param::Release, .1f);
    VCOProcessor vco(state);
    vco.prepare(48000);
    for (bool previouslyPlayed : {false, true}) {
        if (previouslyPlayed) {
            vco.getSoundGenerator()->startNote(69, 1, 8192);
            vco.getSoundGenerator()->stopNote(true);
            advance(vco, 40 * 256);
        }
        for (int selection : {4, 2, 4, 2}) {
            state.set(Param::Feet, selection);
            vco.applyPendingGeneratorChange();
            EXPECT_FALSE(vco.getSoundGenerator()->isActive());
            EXPECT_FALSE(vco.getSoundGenerator()->getPlaybackState().held);
            EXPECT_EQ(vco.getSoundGenerator()->currentNote(), 0);
            advance(vco, 256);
            EXPECT_FALSE(vco.getSoundGenerator()->isActive());
        }
    }
}

TEST(VcoBehaviorTest, BothReleaseSwitchDirectionsExpireWithoutBecomingHeld) {
    cs01::ParameterState state;
    state.set(Param::Release, .1f);
    VCOProcessor vco(state);
    vco.prepare(48000);
    for (int initial : {2, 4}) {
        state.set(Param::Feet, initial);
        vco.applyPendingGeneratorChange();
        vco.getSoundGenerator()->startNote(69, 1, 8192);
        vco.getSoundGenerator()->stopNote(true);
        ASSERT_TRUE(vco.getSoundGenerator()->isActive());
        state.set(Param::Feet, initial == 2 ? 4 : 2);
        vco.applyPendingGeneratorChange();
        EXPECT_FALSE(vco.getSoundGenerator()->getPlaybackState().held);
        advance(vco, 48000);
        EXPECT_FALSE(vco.getSoundGenerator()->isActive());
    }
}

TEST(VcoBehaviorTest, LiveWaveformAndFeetChangesPreserveHeldNoteAndFiniteAudio) {
    ToneHarness h(0, 48000);
    for (int wave : {0, 1, 2, 3, 4, 0, 4, 1, 0})
        for (int feet : {2, 0, 3, 1, 2}) {
            h.state.set(Param::WaveType, wave);
            h.state.set(Param::Feet, feet);
            h.tone.updateBlockRateParameters();
            EXPECT_TRUE(h.tone.isActive());
            EXPECT_EQ(h.tone.currentNote(), 69);
            double energy = 0;
            for (int i = 0; i < 256; ++i) {
                const float value = h.tone.renderSample();
                ASSERT_TRUE(std::isfinite(value));
                energy += value * value;
            }
            EXPECT_GT(energy, 0);
        }
}

TEST(VcoBehaviorTest, ExplicitLfoOffsetsMovePitchAndReturnToCenter) {
    for (double rate : {44100., 48000., 96000.}) {
        ToneHarness h(2, rate);
        for (float offset : {0.f, 1.f, -1.f, 0.f}) {
            h.tone.setLfoValue(offset);
            const double expected = 440. * std::exp2(offset / 12.);
            EXPECT_NEAR(measurePitch(h.tone, rate), expected, expected * .001);
        }
    }
}
