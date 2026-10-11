#include "SynthEngine.h"
#include "dsp/MidiQueue.h"
#include "interaction/KeyboardInput.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <array>

namespace {
using cs01::MidiMessage;
using cs01::Param;
void patch(cs01::ParameterState& state) {
    state.set(Param::Attack, .001f);
    state.set(Param::Decay, .001f);
    state.set(Param::Sustain, .6f);
    state.set(Param::Release, .005f);
    state.set(Param::Volume, .7f);
    state.set(Param::BreathVca, 0);
}
// Device-free queue/engine composition. This does not invoke SDK callbacks.
std::vector<float> block(SynthEngine& engine, cs01::MidiQueue& queue, int frames) {
    if (queue.takeOverflow())
        engine.panic();
    queue.consumeThrough(0, [&](auto message) { engine.handleMidi(message); });
    std::vector<float> output(frames);
    for (int i = 0; i < frames; ++i) {
        queue.consumeThrough(i, [&](auto message) { engine.handleMidi(message); });
        output[i] = engine.renderSample();
    }
    queue.flush(frames);
    return output;
}
double energy(const std::vector<float>& values) {
    double result = 0;
    for (double value : values)
        result += value * value;
    return result;
}
}  // namespace
TEST(EngineTimelineTest, FreeRunningSourcesMuteAtEveryDepthAndRecoverAfterReleaseAndPanic) {
    for (double rate : {44100., 48000., 96000.})
        for (int filter : {0, 1})
            for (int feet : {2, 4})
                for (float depth : {0.f, .4f, 1.f}) {
                    cs01::ParameterState state;
                    patch(state);
                    state.set(Param::FilterType, filter);
                    state.set(Param::Feet, feet);
                    state.set(Param::WaveType, 3);
                    state.set(Param::VcaEgDepth, depth);
                    SynthEngine engine(state);
                    engine.prepare(rate);
                    cs01::MidiQueue queue;
                    EXPECT_EQ(energy(block(engine, queue, 1280)), 0);
                    for (int repeat = 0; repeat < 2; ++repeat) {
                        queue.add(17, {0x90, 45, 127});
                        auto output = block(engine, queue, 1920);
                        for (int i = 0; i < 17; ++i)
                            EXPECT_FLOAT_EQ(output[i], 0);
                        ASSERT_GT(energy(output), 1e-5);
                        queue.add(13, {0x80, 45, 0});
                        block(engine, queue, static_cast<int>(rate * (25600. / 48000.)));
                        EXPECT_LT(energy(block(engine, queue, 64)), 64 * 1e-12);
                        queue.add(0, {0xb0, 120, 0});
                        EXPECT_EQ(energy(block(engine, queue, 64)), 0);
                    }
                }
}
TEST(EngineTimelineTest, EveryWaveformRetainsUpstreamPhaseAcrossInBlockReleaseDeadline) {
    for (double rate : {44100., 48000., 96000.})
        for (int wave : {0, 1, 2, 3, 4}) {
            cs01::ParameterState state;
            state.set(Param::WaveType, wave);
            state.set(Param::Release, .005f);
            state.set(Param::ModDepth, 0);
            VCOProcessor played(state), held(state);
            played.prepareToPlay(rate * 4, 64);
            held.prepareToPlay(rate * 4, 64);
            played.getSoundGenerator()->startNote(45, 1, 8192);
            held.getSoundGenerator()->startNote(45, 1, 8192);
            double idleEnergy = 0;
            for (int i = 0; i < 12800; ++i) {
                if (i == 640)
                    played.getSoundGenerator()->stopNote(true);
                if (i == 6400) {
                    ASSERT_FALSE(played.getSoundGenerator()->isActive());
                    played.getSoundGenerator()->startNote(45, 1, 8192);
                }
                float actual = played.processSample(0), reference = held.processSample(0);
                ASSERT_FLOAT_EQ(actual, reference) << i;
                if (i >= 3200 && i < 6400)
                    idleEnergy += actual * actual;
            }
            EXPECT_GT(idleEnergy, 1);
        }
}
TEST(EngineTimelineTest, NoiseHasEnergyPastReleaseDeadlineInside512Samples) {
    for (double rate : {44100., 48000., 96000.}) {
        cs01::ParameterState state;
        state.set(Param::Feet, 4);
        state.set(Param::Release, .001f);
        VCOProcessor vco(state);
        vco.prepareToPlay(rate * 4, 512);
        vco.getSoundGenerator()->startNote(45, 1, 8192);
        vco.getSoundGenerator()->stopNote(true);
        double tail = 0, next = 0;
        for (int i = 0; i < 1024; ++i) {
            float sample = vco.processSample(0);
            if (i >= 384 && i < 512)
                tail += sample * sample;
            if (i >= 512)
                next += sample * sample;
            if (i == 511)
                EXPECT_FALSE(vco.getSoundGenerator()->isActive());
        }
        EXPECT_GT(std::sqrt(tail / 128), 1e-4);
        EXPECT_GT(std::sqrt(next / 512), 1e-4);
    }
}
TEST(EngineTimelineTest, MidBlockNoteOffAndPanicDoNotAffectEarlierSamples) {
    for (int controller : {-1, 120, 123}) {
        cs01::ParameterState a, b;
        patch(a);
        patch(b);
        SynthEngine baseline(a), event(b);
        baseline.prepare(48000);
        event.prepare(48000);
        baseline.handleMidi({0x90, 69, 127});
        event.handleMidi({0x90, 69, 127});
        for (int i = 0; i < 1000; ++i) {
            baseline.renderSample();
            event.renderSample();
        }
        cs01::MidiQueue queue;
        queue.add(37, controller < 0 ? MidiMessage{0x80, 69, 0}
                                     : MidiMessage{0xb0, static_cast<uint8_t>(controller), 0});
        auto output = block(event, queue, 128);
        for (int i = 0; i < 37; ++i)
            ASSERT_FLOAT_EQ(output[i], baseline.renderSample());
        if (controller == 120) {
            for (size_t i = 37; i < output.size(); ++i)
                EXPECT_FLOAT_EQ(output[i], 0);
        } else
            EXPECT_GT(energy(output), 0);
        queue.add(0, {0x90, 72, 127});
        EXPECT_GT(energy(block(event, queue, 1024)), 1e-5);
    }
}
TEST(EngineTimelineTest, OverflowPanicsThenEmptyBoundaryAndNewInputRecover) {
    cs01::ParameterState state;
    patch(state);
    SynthEngine engine(state);
    engine.prepare(48000);
    engine.handleMidi({0x90, 69, 127});
    cs01::MidiQueue queue;
    ASSERT_GT(energy(block(engine, queue, 1024)), 0);
    for (int i = 0; i <= cs01::MidiQueue::capacity; ++i)
        queue.add(0, {0x90, 60, 127});
    EXPECT_TRUE(block(engine, queue, 0).empty());
    EXPECT_EQ(engine.currentNote(), 0);
    EXPECT_EQ(energy(block(engine, queue, 128)), 0);
    queue.add(0, {0x90, 72, 127});
    block(engine, queue, 0);
    EXPECT_EQ(engine.currentNote(), 72);
    EXPECT_GT(energy(block(engine, queue, 1024)), 0);
}
TEST(EngineTimelineTest, LiveRoutingAndSourceChangesMatchAbsoluteEventScheduleAcrossPartitions) {
    for (double rate : {44100., 48000., 96000.}) {
        const auto render = [&](int size) {
            cs01::ParameterState state;
            patch(state);
            state.set(Param::Release, .2f);
            state.set(Param::ModDepth, .5f);
            state.set(Param::Glissando, .01f);
            SynthEngine engine(state);
            engine.prepare(rate);
            std::vector<float> output;
            const std::array boundaries{0,    137,  400,  800,  1024, 1600,
                                        2400, 2800, 3600, 5000, 60000};
            for (size_t event = 0; event + 1 < boundaries.size(); ++event) {
                state.set(Param::FilterType, event % 2);
                state.set(Param::LfoTarget, (event / 2) % 2);
                state.set(Param::Feet, event == 4 || event == 6 ? 4 : 2);
                if (event == 0 || event == 8)
                    engine.handleMidi({0x90, 60, 127});
                if (event == 2)
                    engine.handleMidi({0x90, 67, 127});
                if (event == 3)
                    engine.handleMidi({0x80, 67, 0});
                if (event == 5 || event == 9)
                    engine.handleMidi({0x80, 60, 0});
                if (event == 6)
                    state.set(Param::Release, .3f);
                if (event == 7)
                    state.set(Param::Release, .001f);
                for (int position = boundaries[event]; position < boundaries[event + 1];) {
                    int count = std::min(size, boundaries[event + 1] - position);
                    for (int i = 0; i < count; ++i)
                        output.push_back(engine.renderSample());
                    position += count;
                }
            }
            return output;
        };
        auto reference = render(1);
        ASSERT_GT(energy(reference), 1e-5);
        EXPECT_LT(std::abs(reference.back()), 1e-6);
        for (int size : {7, 64, 256})
            EXPECT_EQ(render(size), reference);
        for (float value : reference)
            ASSERT_TRUE(std::isfinite(value));
    }
}
TEST(EngineTimelineTest, BreathMidiReachesVcaAndOriginalFilterAndResetRecovers) {
    const auto render = [](float vcaDepth, float vcfDepth, int breath) {
        cs01::ParameterState state;
        patch(state);
        state.set(Param::Cutoff, 1000);
        state.set(Param::VcaEgDepth, 0);
        state.set(Param::VcfEgDepth, 0);
        state.set(Param::BreathVca, vcaDepth);
        state.set(Param::BreathVcf, vcfDepth);
        SynthEngine engine(state);
        engine.prepare(48000);
        engine.handleMidi({0x90, 93, 127});
        engine.handleMidi({0xb0, 2, static_cast<uint8_t>(breath)});
        engine.handleMidi({0xb0, 34, static_cast<uint8_t>(breath)});
        cs01::MidiQueue queue;
        block(engine, queue, 48000);
        return block(engine, queue, 48000);
    };
    auto plain = render(0, 0, 0);
    ASSERT_GT(energy(plain), 0);
    EXPECT_EQ(plain, render(0, 0, 127));
    EXPECT_EQ(energy(render(1, 0, 0)), 0);
    EXPECT_EQ(plain, render(1, 0, 127));
    EXPECT_GT(energy(render(0, 1, 127)), energy(plain));
}
TEST(EngineTimelineTest, SourceSelectionMutatesOnlyOnAudioUseWhileWorkerPublishesChoices) {
    cs01::ParameterState state;
    VCOProcessor vco(state);
    vco.prepareToPlay(48000, 64);
    auto* original = vco.getSoundGenerator();
    original->startNote(69, 1, 12000);
    std::thread worker([&] {
        for (int i = 0; i < 10000; ++i)
            state.set(Param::Feet, i % 2 ? 4 : 2);
        state.set(Param::Feet, 4);
    });
    worker.join();
    EXPECT_EQ(vco.getSoundGenerator(), original);
    vco.applyPendingGeneratorChange();
    EXPECT_NE(vco.getSoundGenerator(), original);
    EXPECT_TRUE(vco.getSoundGenerator()->getPlaybackState().held);
    EXPECT_EQ(vco.getSoundGenerator()->getCurrentlyPlayingNote(), 69);
    EXPECT_EQ(vco.getSoundGenerator()->getPlaybackState().pitchWheel, 12000);
    EXPECT_FALSE(original->isActive());
}

TEST(EngineTimelineTest, EveryStationaryRouteMatchesIndependentConnectedSignalChain) {
    for (double rate : {44100., 48000., 96000.})
        for (int filter : {0, 1})
            for (int target : {0, 1})
                for (int feet : {2, 4}) {
                    cs01::ParameterState state;
                    patch(state);
                    state.set(Param::FilterType, filter);
                    state.set(Param::LfoTarget, target);
                    state.set(Param::Feet, feet);
                    state.set(Param::Cutoff, 1000);
                    state.set(Param::VcfEgDepth, .5f);
                    state.set(Param::ModDepth, .5f);
                    SynthEngine engine(state);
                    engine.prepare(rate);
                    engine.handleMidi({0x90, 69, 127});
                    double internal = rate * 4;
                    ToneGenerator tone(state);
                    NoiseGenerator noise(state);
                    tone.prepare(internal);
                    noise.prepare(internal);
                    tone.startNote(69, 1, 8192);
                    noise.startNote(69, 1, 8192);
                    EGProcessor eg(state);
                    LFOProcessor lfo(state);
                    OriginalVCFProcessor original(state);
                    ModernVCFProcessor modern(state);
                    VCAProcessor vca(state);
                    eg.prepareToPlay(internal, 1);
                    lfo.prepareToPlay(internal, 1);
                    original.prepareToPlay(internal, 1);
                    modern.prepareToPlay(internal, 1);
                    vca.prepareToPlay(internal, 1);
                    eg.startEnvelope();
                    cs01::OutputConverter output;
                    double total = 0;
                    for (int i = 0; i < 4096; ++i) {
                        float expected = 0;
                        for (int j = 0; j < 4; ++j) {
                            float modulation = lfo.processSample(), envelope = eg.processSample();
                            tone.updateBlockRateParameters();
                            tone.setLfoValue(target == 0 ? modulation * .5f : 0);
                            float source = feet == 4 ? noise.renderContinuousSample()
                                                     : tone.renderContinuousSample();
                            float filtered =
                                filter == 0 ? original.processSample(source, envelope,
                                                                     target == 1 ? modulation : 0)
                                            : modern.processSample(source, envelope,
                                                                   target == 1 ? modulation : 0);
                            output.push(vca.processSample(filtered, envelope, eg.getNoteGate()));
                            if (j == 0)
                                expected = output.output();
                        }
                        float actual = engine.renderSample();
                        ASSERT_FLOAT_EQ(actual, expected) << i;
                        total += actual * actual;
                    }
                    EXPECT_GT(total, 1e-5);
                }
}
TEST(EngineTimelineTest, SameTimestampPanicAndNoteArrivalOrderIsRetainedAcrossSourceSwitches) {
    for (int feet : {2, 4})
        for (bool panicLast : {false, true}) {
            cs01::ParameterState state;
            patch(state);
            state.set(Param::Feet, feet);
            SynthEngine engine(state);
            engine.prepare(48000);
            cs01::MidiQueue queue;
            queue.add(0, panicLast ? MidiMessage{0x90, 72, 127} : MidiMessage{0xb0, 120, 0});
            queue.add(0, panicLast ? MidiMessage{0xb0, 120, 0} : MidiMessage{0x90, 72, 127});
            block(engine, queue, 0);
            auto output = block(engine, queue, 1280);
            if (panicLast)
                EXPECT_EQ(energy(output), 0);
            else
                EXPECT_GT(energy(output), 1e-4);
            state.set(Param::Feet, feet == 2 ? 4 : 2);
            queue.add(0, {0x80, 72, 0});
            block(engine, queue, 25600);
            EXPECT_LT(energy(block(engine, queue, 64)), 64e-10);
        }
}

TEST(EngineTimelineTest, LiveGlissandoEditsMatchFullEngineTimelineAcrossPartitions) {
    for (double rate : {44100., 48000., 96000.}) {
        auto render = [&](int size) {
            cs01::ParameterState state;
            patch(state);
            state.set(Param::Sustain, 1);
            state.set(Param::WaveType, 2);
            state.set(Param::Glissando, .1f);
            SynthEngine engine(state);
            engine.prepare(rate);
            std::vector<float> values;
            int interval = static_cast<int>(rate * .02);
            for (int segment = 0; segment < 8; ++segment) {
                if (segment == 2)
                    state.set(Param::Glissando, .05f);
                if (segment == 3)
                    state.set(Param::Glissando, .15f);
                if (segment == 4)
                    state.set(Param::Glissando, .01f);
                if (segment == 5)
                    state.set(Param::Glissando, 0);
                if (segment == 0 || segment == 1)
                    engine.handleMidi({0x90, static_cast<uint8_t>(segment == 0 ? 69 : 76), 127});
                for (int position = 0; position < interval;) {
                    int count = std::min(size, interval - position);
                    for (int i = 0; i < count; ++i)
                        values.push_back(engine.renderSample());
                    position += count;
                }
            }
            return values;
        };
        auto reference = render(1);
        EXPECT_GT(energy(reference), 1e-6);
        for (int size : {64, 256}) {
            auto actual = render(size);
            ASSERT_EQ(actual.size(), reference.size());
            for (size_t i = 0; i < actual.size(); ++i) {
                ASSERT_TRUE(std::isfinite(actual[i]));
                ASSERT_NEAR(actual[i], reference[i], 1e-5);
            }
        }
    }
}

TEST(EngineTimelineTest, ConcurrentChoicePublicationKeepsAudioOwnedNoteAndFiniteSamples) {
    cs01::ParameterState state;
    VCOProcessor vco(state);
    vco.prepareToPlay(48000, 64);
    vco.getSoundGenerator()->startNote(69, 1, 12000);
    std::atomic<bool> done{false};
    std::thread worker([&] {
        for (int i = 0; i < 10000; ++i)
            state.set(Param::Feet, i % 2 ? 4 : 2);
        state.set(Param::Feet, 4);
        done.store(true);
    });
    bool held = true, finite = true;
    do {
        vco.applyPendingGeneratorChange();
        held &= vco.getSoundGenerator()->getPlaybackState().held;
        finite &= std::isfinite(vco.processSample(0));
    } while (!done.load());
    worker.join();
    vco.applyPendingGeneratorChange();
    EXPECT_TRUE(held);
    EXPECT_TRUE(finite);
    EXPECT_EQ(vco.getSoundGenerator()->getCurrentlyPlayingNote(), 69);
    EXPECT_EQ(vco.getSoundGenerator()->getPlaybackState().pitchWheel, 12000);
}

TEST(EngineTimelineTest, BreathMidiLiveMuteOpenAndReturnRecoverWithinOnePercent) {
    for (bool vcaControl : {false, true}) {
        cs01::ParameterState expectedState, controlledState;
        for (auto* state : {&expectedState, &controlledState}) {
            patch(*state);
            state->set(Param::Cutoff, 1000);
            state->set(Param::VcaEgDepth, 0);
            state->set(Param::VcfEgDepth, 0);
            state->set(Param::BreathVcf, 0);
        }
        controlledState.set(vcaControl ? Param::BreathVca : Param::BreathVcf, 1);
        SynthEngine expected(expectedState), controlled(controlledState);
        expected.prepare(48000);
        controlled.prepare(48000);
        for (auto* engine : {&expected, &controlled}) {
            engine->handleMidi({0x90, 93, 127});
            engine->handleMidi({0xb0, 2, static_cast<uint8_t>(vcaControl ? 127 : 0)});
            engine->handleMidi({0xb0, 34, static_cast<uint8_t>(vcaControl ? 127 : 0)});
        }
        double baseline = 0, altered = 0, referenceReturn = 0, returned = 0;
        for (int i = 0; i < 96000; ++i) {
            if (i == 24000 || i == 48000) {
                int value = i == 24000 ? (vcaControl ? 0 : 127) : (vcaControl ? 127 : 0);
                controlled.handleMidi({0xb0, 2, static_cast<uint8_t>(value)});
                controlled.handleMidi({0xb0, 34, static_cast<uint8_t>(value)});
            }
            float a = expected.renderSample(), b = controlled.renderSample();
            ASSERT_TRUE(std::isfinite(b));
            if (i >= 12000 && i < 20000) {
                ASSERT_NEAR(b, a, 1e-6);
                baseline += a * a;
            }
            if (i >= 36000 && i < 44000)
                altered += b * b;
            if (i >= 72000) {
                referenceReturn += a * a;
                returned += b * b;
            }
        }
        ASSERT_GT(baseline, 1e-10);
        ASSERT_GT(referenceReturn, 1e-10);
        if (vcaControl)
            EXPECT_LT(altered, baseline * 1e-8);
        else
            EXPECT_GT(altered, baseline);
        EXPECT_NEAR(returned / referenceReturn, 1, .01);
    }
}
TEST(EngineTimelineTest, Observation_WholeOutputRetainsHighNoteFundamentalAfterFirConversion) {
    for (double rate : {44100., 48000., 96000.})
        for (int waveform : {1, 2}) {
            cs01::ParameterState state;
            patch(state);
            state.set(Param::WaveType, waveform);
            state.set(Param::Feet, 2);
            state.set(Param::Cutoff, 15000);
            state.set(Param::Resonance, 0);
            state.set(Param::VcfEgDepth, 0);
            state.set(Param::BreathVcf, 0);
            state.set(Param::Sustain, 1);
            state.set(Param::Volume, 1);
            state.set(Param::ModDepth, 0);
            SynthEngine engine(state);
            engine.prepare(rate);
            engine.handleMidi({0x90, 117, 127});
            double sine = 0, cosine = 0;
            for (int i = 0; i < static_cast<int>(rate * 2); ++i) {
                float value = engine.renderSample();
                ASSERT_TRUE(std::isfinite(value));
                if (i >= rate) {
                    double phase = 2 * std::numbers::pi * 7040 * i / rate;
                    sine += value * std::sin(phase);
                    cosine += value * std::cos(phase);
                }
            }
            ASSERT_GT(2 * std::hypot(sine, cosine) / rate, 1e-6);
        }
}
