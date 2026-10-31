#include "cs01synth/MidiProcessor.h"
#include "cs01synth/ISoundGenerator.h"
#include "cs01synth/EGProcessor.h"
#include "SynthEngine.h"
#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <set>

namespace {
using cs01::Param;
cs01::MidiMessage on(int note, int velocity = 100) {
    return {0x90, static_cast<uint8_t>(note), static_cast<uint8_t>(velocity)};
}
cs01::MidiMessage off(int note) {
    return {0x80, static_cast<uint8_t>(note), 0};
}
cs01::MidiMessage cc(int number, int value) {
    return {0xb0, static_cast<uint8_t>(number), static_cast<uint8_t>(value)};
}
// Observe the MIDI-to-generator contract, independently of waveform synthesis.
class RecordingGenerator : public ISoundGenerator {
   public:
    void startNote(int n, float v, int bendValue) override {
        ++starts;
        note = n;
        velocity = v;
        pitchBendValue = bendValue;
        active = true;
    }
    void stopNote(bool tail) override {
        ++stops;
        lastTail = tail;
        active = tail;
    }
    void changeNote(int n) override {
        ++changes;
        note = n;
    }
    void setPitchBend(int bendValue) override {
        pitchBendValue = bendValue;
    }
    bool isActive() const override {
        return active;
    }
    int currentNote() const override {
        return note;
    }
    void prepare(double) override {}
    float renderSample() override {
        return 0;
    }
    int starts = 0, stops = 0, changes = 0, note = 0, pitchBendValue = 8192;
    float velocity = 0;
    bool active = false, lastTail = false;
};
struct MidiBehaviorTest : testing::Test {
    cs01::ParameterState state;
    MidiProcessor midi{state};
};
}  // namespace

TEST_F(MidiBehaviorTest, DuplicateAndUnmatchedNotesPreserveLegatoAndVelocity) {
    RecordingGenerator generator;
    midi.setSoundGenerator(&generator);
    midi.handleMidiEvent(cs01::MidiMessage::pitchBend(1, 12000));
    midi.handleMidiEvent(on(64, 23));
    midi.handleMidiEvent(on(64, 127));
    midi.handleMidiEvent(on(60));
    EXPECT_EQ(midi.getActiveNotes().count(), 2u);
    EXPECT_EQ(generator.starts, 1);
    EXPECT_FLOAT_EQ(generator.velocity, 23 / 127.f);
    EXPECT_EQ(generator.pitchBendValue, 12000);
    EXPECT_EQ(generator.note, 64);
    const int changes = generator.changes;
    midi.handleMidiEvent(off(63));
    EXPECT_EQ(generator.changes, changes);
    EXPECT_EQ(generator.stops, 0);
    midi.handleMidiEvent(off(64));
    EXPECT_EQ(generator.note, 60);
    midi.handleMidiEvent(off(64));
    EXPECT_EQ(generator.note, 60);
    midi.handleMidiEvent(off(60));
    EXPECT_EQ(generator.stops, 1);
    EXPECT_TRUE(generator.lastTail);
    midi.handleMidiEvent(off(60));
    EXPECT_EQ(generator.stops, 1);
}

TEST_F(MidiBehaviorTest, NoteSetMatchesIndependentSetAcrossEntireRange) {
    EXPECT_TRUE(midi.getActiveNotes().none());
    EXPECT_EQ(midi.currentNote(), 0);
    EXPECT_EQ(midi.getSoundGenerator(), nullptr);
    RecordingGenerator generator;
    midi.setSoundGenerator(&generator);
    std::set<int> reference;
    const auto send = [&](int note, bool held) {
        midi.handleMidiEvent(held ? on(note) : off(note));
        if (held)
            reference.insert(note);
        else
            reference.erase(note);
        EXPECT_EQ(midi.getActiveNotes().count(), reference.size());
        EXPECT_EQ(midi.currentNote(), reference.empty() ? 0 : *reference.rbegin());
        if (!reference.empty()) {
            EXPECT_TRUE(generator.isActive());
            EXPECT_EQ(generator.note, *reference.rbegin());
        }
        for (int key = 0; key < 128; ++key)
            EXPECT_EQ(midi.getActiveNotes().test(key), reference.count(key) != 0);
    };
    for (int i = 0; i < 128; ++i) {
        send(i * 37 % 128, true);
        send(i * 37 % 128, true);
    }
    for (int note = 127; note >= 0; --note) {
        send(note, false);
        send(note, false);
    }
}

TEST_F(MidiBehaviorTest, HeldKeysKeepEnvelopeGateOpenAndRestartAfterRelease) {
    state.set(Param::Attack, .1f);
    state.set(Param::Decay, .3f);
    state.set(Param::Sustain, .5f);
    state.set(Param::Release, .5f);
    RecordingGenerator generator;
    EGProcessor eg(state);
    eg.prepare(48000);
    midi.setSoundGenerator(&generator);
    midi.setEGProcessor(&eg);
    const auto advance = [&](int count) {
        float value = 0;
        while (count-- > 0)
            value = eg.processSample();
        return value;
    };
    midi.handleMidiEvent(on(64));
    ASSERT_TRUE(eg.isActive());
    EXPECT_NEAR(advance(144000), .5f, 1e-5);
    const std::array messages{on(60), on(67), off(67), off(64)};
    const std::array pitches{64, 67, 64, 60};
    for (size_t i = 0; i < messages.size(); ++i) {
        midi.handleMidiEvent(messages[i]);
        EXPECT_EQ(generator.note, pitches[i]);
        EXPECT_NEAR(advance(256), .5f, 1e-5);
        EXPECT_FLOAT_EQ(eg.getNoteGate(), 1.f);
    }
    EXPECT_EQ(generator.note, 60);
    midi.handleMidiEvent(off(60));
    EXPECT_TRUE(midi.getActiveNotes().none());
    EXPECT_LT(advance(256), .5f);
    EXPECT_FLOAT_EQ(advance(240000), 0.f);
    EXPECT_FALSE(eg.isActive());
    midi.handleMidiEvent(on(60));
    EXPECT_GT(advance(256), 0.f);
}

TEST_F(MidiBehaviorTest, PanicSeparatesReleaseFromImmediateStopAndCanRecover) {
    RecordingGenerator generator;
    EGProcessor eg(state);
    eg.prepare(48000);
    midi.setSoundGenerator(&generator);
    midi.setEGProcessor(&eg);
    midi.handleMidiEvent(on(0));
    midi.handleMidiEvent(on(127));
    for (int i = 0; i < 64; ++i)
        eg.processSample();
    midi.handleMidiEvent(cc(123, 0));
    EXPECT_TRUE(midi.getActiveNotes().none());
    EXPECT_TRUE(generator.lastTail);
    EXPECT_TRUE(eg.isActive());
    const int stops = generator.stops;
    midi.handleMidiEvent(cc(123, 0));
    EXPECT_EQ(generator.stops, stops);
    midi.handleMidiEvent(cc(120, 0));
    EXPECT_FALSE(generator.lastTail);
    EXPECT_FALSE(generator.isActive());
    EXPECT_FALSE(eg.isActive());
    for (int i = 0; i < 64; ++i)
        EXPECT_FLOAT_EQ(eg.processSample(), 0.f);
    midi.handleMidiEvent(on(60));
    EXPECT_TRUE(eg.isActive());
    midi.handleMidiEvent(cc(120, 0));
    EXPECT_TRUE(midi.getActiveNotes().none());
    EXPECT_FALSE(generator.isActive());
    EXPECT_FALSE(eg.isActive());
}

TEST_F(MidiBehaviorTest, ResetControllersPreservesWholePatchAndClearsPerformancePairState) {
    RecordingGenerator generator;
    midi.setSoundGenerator(&generator);
    midi.handleMidiEvent(on(64));
    midi.handleMidiEvent(cs01::MidiMessage::pitchBend(1, 12000));
    for (int number : {1, 33, 2, 34, 7, 39, 5, 37})
        midi.handleMidiEvent(cc(number, 127));
    const auto before = state.snapshot();
    midi.handleMidiEvent(cc(121, 0));
    EXPECT_EQ(midi.currentNote(), 64);
    EXPECT_EQ(generator.starts, 1);
    EXPECT_EQ(generator.stops, 0);
    EXPECT_EQ(generator.pitchBendValue, 8192);
    for (int i = 0; i < cs01::parameterCount; ++i) {
        auto id = static_cast<Param>(i);
        const bool performance =
            id == Param::PitchBend || id == Param::ModDepth || id == Param::BreathInput;
        EXPECT_FLOAT_EQ(state.get(id), performance ? 0.f : before[i]);
    }
    midi.handleMidiEvent(cc(33, 1));
    midi.handleMidiEvent(cc(34, 1));
    EXPECT_NEAR(state.get(Param::ModDepth), 1.f / 16383, 1e-6);
    EXPECT_NEAR(state.get(Param::BreathInput), 1.f / 16383, 1e-6);
}

TEST_F(MidiBehaviorTest, EveryControllerUsesProductionRangesAndFourteenBitPairs) {
    const std::array ids{Param::ModDepth, Param::BreathInput, Param::Volume, Param::Glissando};
    const std::array msbs{1, 2, 7, 5};
    const std::array lsbs{33, 34, 39, 37};
    for (size_t i = 0; i < ids.size(); ++i) {
        for (int value : {0, 64, 127}) {
            midi.handleMidiEvent(cc(lsbs[i], 0));
            midi.handleMidiEvent(cc(msbs[i], value));
            const float normalized = value * 128 / 16383.f;
            const auto& d = cs01::definition(ids[i]);
            const auto expected = [&](float n) {
                const float raw = d.minimum + (d.maximum - d.minimum) * std::pow(n, 1.f / d.skew);
                return raw;
            };
            EXPECT_FLOAT_EQ(state.get(ids[i]), expected(normalized));
            midi.handleMidiEvent(cc(lsbs[i], 127));
            EXPECT_FLOAT_EQ(state.get(ids[i]), expected((value * 128 + 127) / 16383.f));
        }
    }
    midi.handleMidiEvent(cc(121, 0));
    for (int pressure : {0, 32, 64, 127}) {
        midi.handleMidiEvent(cs01::MidiMessage::channelPressure(1, pressure));
        EXPECT_FLOAT_EQ(state.get(Param::BreathInput), pressure / 127.f);
    }
    midi.handleMidiEvent(cc(121, 0));
    EXPECT_FLOAT_EQ(state.get(Param::BreathInput), 0.f);
    RecordingGenerator generator;
    midi.setSoundGenerator(&generator);
    for (int pitchBendValue : {0, 4096, 8192, 12000, 16383}) {
        midi.handleMidiEvent(cs01::MidiMessage::pitchBend(1, pitchBendValue));
        EXPECT_NEAR(state.get(Param::PitchBend),
                    (pitchBendValue - 8192) / (pitchBendValue >= 8192 ? 8191.f : 8192.f), 1e-7);
        EXPECT_EQ(generator.pitchBendValue, pitchBendValue);
    }
    midi.handleMidiEvent(cc(5, 64));
    midi.handleMidiEvent(cc(37, 3));
    const auto& d = cs01::definition(Param::Glissando);
    EXPECT_NEAR(state.get(Param::Glissando),
                d.maximum * std::pow((64 * 128 + 3) / 16383.f, 1.f / d.skew), 1e-6);
}

TEST_F(MidiBehaviorTest, BreathSourcesComposeByMaximumIndependentOfEventOrder) {
    for (int msb : {0, 20, 100, 127})
        for (int lsb : {0, 64, 127})
            for (int pressure : {0, 20, 64, 127})
                for (bool pressureFirst : {false, true}) {
                    SCOPED_TRACE(msb);
                    SCOPED_TRACE(lsb);
                    SCOPED_TRACE(pressure);
                    SCOPED_TRACE(pressureFirst);
                    midi.handleMidiEvent(cc(121, 0));
                    const auto aftertouch = cs01::MidiMessage::channelPressure(1, pressure);
                    if (pressureFirst)
                        midi.handleMidiEvent(aftertouch);
                    midi.handleMidiEvent(cc(2, msb));
                    midi.handleMidiEvent(cc(34, lsb));
                    if (!pressureFirst)
                        midi.handleMidiEvent(aftertouch);
                    const float breath = (msb * 128 + lsb) / 16383.f;
                    EXPECT_FLOAT_EQ(state.get(Param::BreathInput),
                                    std::max(breath, pressure / 127.f));
                    midi.handleMidiEvent(cs01::MidiMessage::channelPressure(1, 0));
                    EXPECT_FLOAT_EQ(state.get(Param::BreathInput), breath);
                    midi.handleMidiEvent(aftertouch);
                    midi.handleMidiEvent(cc(2, 0));
                    midi.handleMidiEvent(cc(34, 0));
                    EXPECT_FLOAT_EQ(state.get(Param::BreathInput), pressure / 127.f);
                    midi.handleMidiEvent(cc(121, 0));
                    EXPECT_FLOAT_EQ(state.get(Param::BreathInput), 0.f);
                    midi.handleMidiEvent(cc(34, 1));
                    EXPECT_FLOAT_EQ(state.get(Param::BreathInput), 1 / 16383.f);
                }
}

TEST_F(MidiBehaviorTest, RemovedPanelControllersPreservePatchAndPerformanceState) {
    RecordingGenerator generator;
    midi.setSoundGenerator(&generator);
    midi.handleMidiEvent(on(60));
    midi.handleMidiEvent(on(72));
    midi.handleMidiEvent(cc(64, 127));
    midi.handleMidiEvent(off(72));
    for (float position : {.25f, .75f}) {
        for (int i = 0; i < cs01::parameterCount; ++i)
            state.setNormalized(static_cast<Param>(i), position);
        const auto before = state.snapshot();
        const auto notes = midi.getActiveNotes();
        state.takeMidiChanges();
        const int starts = generator.starts, stops = generator.stops, changes = generator.changes;
        for (int number : {9, 14, 15, 16, 70, 71, 72, 73, 74, 75, 76, 79, 82, 85, 88, 89, 90, 91}) {
            SCOPED_TRACE(number);
            for (int value = 0; value <= 127; ++value) {
                midi.handleMidiEvent(cc(number, value));
                EXPECT_EQ(state.snapshot(), before);
                EXPECT_EQ(state.takeMidiChanges(), 0u);
                EXPECT_EQ(midi.getActiveNotes(), notes);
                EXPECT_EQ(generator.starts, starts);
                EXPECT_EQ(generator.stops, stops);
                EXPECT_EQ(generator.changes, changes);
            }
        }
    }
    midi.handleMidiEvent(cc(64, 0));
    EXPECT_EQ(midi.currentNote(), 60);
    midi.handleMidiEvent(off(60));
    EXPECT_TRUE(midi.getActiveNotes().none());
}

TEST_F(MidiBehaviorTest, SustainPedalThresholdRetainsGateAndReturnsToHeldHighestNote) {
    RecordingGenerator generator;
    EGProcessor eg(state);
    eg.prepare(48000);
    midi.setSoundGenerator(&generator);
    midi.setEGProcessor(&eg);
    midi.handleMidiEvent(on(60));
    for (int i = 0; i < 64; ++i)
        eg.processSample();
    midi.handleMidiEvent(cc(64, 63));
    midi.handleMidiEvent(off(60));
    EXPECT_EQ(generator.stops, 1);
    EXPECT_GT(eg.currentReleaseSamplesRemaining(), 0);

    for (int down : {64, 127}) {
        SCOPED_TRACE(down);
        midi.handleMidiEvent(on(60));
        for (int i = 0; i < 64; ++i)
            eg.processSample();
        midi.handleMidiEvent(on(72));
        midi.handleMidiEvent(cc(64, down));
        midi.handleMidiEvent(off(72));
        const int stops = generator.stops;
        const int starts = generator.starts;
        EXPECT_EQ(midi.currentNote(), 72);
        EXPECT_EQ(generator.note, 72);
        EXPECT_FLOAT_EQ(eg.getNoteGate(), 1);
        midi.handleMidiEvent(off(72));
        midi.handleMidiEvent(on(67));
        EXPECT_EQ(generator.starts, starts);
        EXPECT_EQ(generator.note, 72);
        midi.handleMidiEvent(cc(64, 63));
        EXPECT_EQ(midi.currentNote(), 67);
        EXPECT_EQ(generator.note, 67);
        EXPECT_EQ(generator.stops, stops);
        EXPECT_FLOAT_EQ(eg.getNoteGate(), 1);
        midi.handleMidiEvent(off(67));
        EXPECT_EQ(generator.note, 60);
        midi.handleMidiEvent(off(60));
        EXPECT_EQ(generator.stops, stops + 1);
        EXPECT_TRUE(generator.lastTail);
        EXPECT_GT(eg.currentReleaseSamplesRemaining(), 0);
        midi.handleMidiEvent(cc(64, 0));
        EXPECT_EQ(generator.stops, stops + 1);
    }
}

TEST_F(MidiBehaviorTest, SustainRepeatedKeyAndAllNotesOffReleaseOnlyWhenPedalLifts) {
    RecordingGenerator generator;
    EGProcessor eg(state);
    eg.prepare(48000);
    midi.setSoundGenerator(&generator);
    midi.setEGProcessor(&eg);
    midi.handleMidiEvent(cc(64, 127));
    midi.handleMidiEvent(on(72));
    for (int i = 0; i < 64; ++i)
        eg.processSample();
    midi.handleMidiEvent(off(72));
    midi.handleMidiEvent(on(72));
    midi.handleMidiEvent(cc(64, 0));
    EXPECT_EQ(generator.stops, 0);
    EXPECT_EQ(midi.currentNote(), 72);
    midi.handleMidiEvent(cc(64, 127));
    midi.handleMidiEvent(cc(123, 0));
    midi.handleMidiEvent(cc(123, 0));
    EXPECT_EQ(generator.stops, 0);
    EXPECT_FLOAT_EQ(eg.getNoteGate(), 1);
    midi.handleMidiEvent(cc(64, 0));
    EXPECT_EQ(generator.stops, 1);
    EXPECT_TRUE(generator.lastTail);
    EXPECT_TRUE(midi.getActiveNotes().none());
    EXPECT_GT(eg.currentReleaseSamplesRemaining(), 0);
    midi.handleMidiEvent(cc(64, 0));
    midi.handleMidiEvent(off(72));
    EXPECT_EQ(generator.stops, 1);
}

TEST_F(MidiBehaviorTest, ResetControllersReleasesSustainedNotesButPreservesHeldKeys) {
    RecordingGenerator generator;
    midi.setSoundGenerator(&generator);
    midi.handleMidiEvent(on(60));
    midi.handleMidiEvent(on(72));
    midi.handleMidiEvent(cc(64, 127));
    midi.handleMidiEvent(off(72));
    midi.handleMidiEvent(cc(2, 100));
    midi.handleMidiEvent(cs01::MidiMessage::channelPressure(1, 127));
    const auto before = state.snapshot();
    midi.handleMidiEvent(cc(121, 0));
    EXPECT_EQ(midi.currentNote(), 60);
    EXPECT_EQ(generator.note, 60);
    EXPECT_EQ(generator.stops, 0);
    for (int i = 0; i < cs01::parameterCount; ++i) {
        const auto id = static_cast<Param>(i);
        const bool performance =
            id == Param::PitchBend || id == Param::ModDepth || id == Param::BreathInput;
        EXPECT_FLOAT_EQ(state.get(id), performance ? 0.f : before[i]);
    }
    midi.handleMidiEvent(off(60));
    EXPECT_EQ(generator.stops, 1);
    midi.handleMidiEvent(on(72));
    midi.handleMidiEvent(cc(64, 127));
    midi.handleMidiEvent(off(72));
    midi.handleMidiEvent(cc(121, 0));
    EXPECT_EQ(generator.stops, 2);
    EXPECT_TRUE(midi.getActiveNotes().none());
}

TEST_F(MidiBehaviorTest, PanicAndPrepareClearPedalAndSustainedNotes) {
    RecordingGenerator generator;
    midi.setSoundGenerator(&generator);
    for (bool prepare : {false, true}) {
        midi.handleMidiEvent(on(72));
        midi.handleMidiEvent(cc(64, 127));
        midi.handleMidiEvent(off(72));
        if (prepare)
            midi.prepare(48000);
        else
            midi.handleMidiEvent(cc(120, 0));
        EXPECT_TRUE(midi.getActiveNotes().none());
        EXPECT_FALSE(generator.lastTail);
        midi.handleMidiEvent(on(60));
        midi.handleMidiEvent(off(60));
        EXPECT_TRUE(midi.getActiveNotes().none());
        EXPECT_TRUE(generator.lastTail);
    }
}

TEST_F(MidiBehaviorTest, VelocityIsPassedOnFirstNoteButDoesNotChangeRealSynthAudio) {
    for (int velocity : {1, 20, 32, 64, 96, 127}) {
        RecordingGenerator generator;
        midi.setSoundGenerator(&generator);
        midi.handleMidiEvent(on(60, velocity));
        EXPECT_TRUE(generator.isActive());
        EXPECT_EQ(generator.note, 60);
        EXPECT_FLOAT_EQ(generator.velocity, velocity / 127.f);
        midi.handleMidiEvent(off(60));
    }
    midi.setSoundGenerator(nullptr);
    for (double rate : {44100., 48000., 96000.})
        for (int wave = 0; wave < 5; ++wave) {
            cs01::ParameterState lowState, highState;
            lowState.set(Param::WaveType, wave);
            highState.set(Param::WaveType, wave);
            SynthEngine low(lowState), high(highState);
            low.prepare(rate);
            high.prepare(rate);
            low.handleMidi(on(60, 20));
            high.handleMidi(on(60, 127));
            double energy = 0;
            for (int i = 0; i < 4096; ++i) {
                const float value = low.renderSample();
                ASSERT_TRUE(std::isfinite(value));
                EXPECT_FLOAT_EQ(value, high.renderSample());
                energy += value * value;
            }
            EXPECT_GT(energy, .001);
        }
}

TEST_F(MidiBehaviorTest, DirectPerformanceEditsRetainNormalizedParameterValues) {
    RecordingGenerator generator;
    midi.setSoundGenerator(&generator);
    midi.handleMidiEvent(on(60));
    state.setNormalized(Param::PitchBend, .75f);
    state.setNormalized(Param::ModDepth, .5f);
    state.setNormalized(Param::BreathInput, .25f);
    EXPECT_FLOAT_EQ(state.get(Param::PitchBend), .5f);
    EXPECT_FLOAT_EQ(state.get(Param::ModDepth), .5f);
    EXPECT_FLOAT_EQ(state.get(Param::BreathInput), .25f);
    EXPECT_EQ(midi.currentNote(), 60);
    EXPECT_EQ(generator.starts, 1);
}

TEST_F(MidiBehaviorTest, ResetDuringAttackCentersWheelWithoutRetriggeringEnvelope) {
    cs01::ParameterState referenceState;
    for (auto* parameters : {&state, &referenceState}) {
        parameters->set(Param::Attack, .1f);
        parameters->set(Param::Decay, .1f);
        parameters->set(Param::Sustain, .6f);
    }
    EGProcessor actual(state), reference(referenceState);
    actual.prepare(48000);
    reference.prepare(48000);
    RecordingGenerator generator;
    midi.setSoundGenerator(&generator);
    midi.setEGProcessor(&actual);
    midi.handleMidiEvent(on(69));
    reference.startEnvelope();
    midi.handleMidiEvent(cs01::MidiMessage::pitchBend(1, 16383));
    for (int i = 0; i < 60000; ++i) {
        if (i == 209)
            midi.handleMidiEvent(cc(121, 0));
        ASSERT_NEAR(actual.processSample(), reference.processSample(), 1e-6);
        EXPECT_EQ(actual.isActive(), reference.isActive());
    }
    EXPECT_FLOAT_EQ(state.get(Param::PitchBend), 0);
    EXPECT_EQ(generator.pitchBendValue, 8192);
    EXPECT_EQ(generator.starts, 1);
    EXPECT_NEAR(actual.getLastOutputForTesting(), .6f, 1e-5);
}
