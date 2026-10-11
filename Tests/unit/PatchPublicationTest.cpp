#include "Parameters.h"
#include "ProgramManager.h"
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>

namespace {
constexpr unsigned allParameters = (1u << cs01::parameterCount) - 1;
cs01::ParameterState::Values patch(float fraction) {
    cs01::ParameterState::Values result;
    for (int i = 0; i < cs01::parameterCount; ++i)
        result[i] = cs01::fromNormalized(static_cast<cs01::Param>(i), fraction);
    return result;
}
bool waitFor(const std::atomic<bool>& flag) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!flag.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    return flag.load();
}
}  // namespace

TEST(PatchPublicationTest, InFlightWriterNeverBlocksOrPartiallyChangesAudioSnapshot) {
    cs01::ParameterState control, audio;
    const auto before = patch(.2f), after = patch(.8f);
    ASSERT_TRUE(control.setPatch(before, allParameters));
    ASSERT_TRUE(control.tryCopyTo(audio));
    std::atomic<bool> entered{false}, resume{false};
    bool committed = false;
    std::thread writer([&] {
        committed = control.trySetPatchIf(
            after, allParameters,
            [&] {
                entered.store(true);
                while (!resume.load())
                    std::this_thread::yield();
                return true;
            },
            true);
    });
    const bool started = waitFor(entered);
    auto untouched = before;
    const bool captured = control.trySnapshot(untouched);
    const bool copied = control.tryCopyTo(audio);
    const bool competing = control.trySetPatch(before, allParameters);
    const auto during = audio.snapshot();
    const auto duringReset = audio.stateResetSerial();
    resume.store(true);
    writer.join();
    ASSERT_TRUE(started);
    EXPECT_FALSE(captured);
    EXPECT_EQ(untouched, before);
    EXPECT_FALSE(copied);
    EXPECT_FALSE(competing);
    EXPECT_EQ(during, before);
    EXPECT_EQ(duringReset, 0u);
    ASSERT_TRUE(committed);
    ASSERT_TRUE(control.tryCopyTo(audio));
    EXPECT_EQ(audio.snapshot(), after);
    EXPECT_EQ(audio.stateResetSerial(), 1u);
}

TEST(PatchPublicationTest, WholePatchAndNoteResetSerialStayTogetherAcrossConcurrentPublication) {
    cs01::ParameterState control, audio;
    const auto a = patch(.2f), b = patch(.8f);
    ASSERT_TRUE(control.setPatch(a, allParameters, true));
    ASSERT_TRUE(control.tryCopyTo(audio));
    std::atomic<bool> done{false};
    std::thread writer([&] {
        for (int i = 0; i < 20000; ++i)
            control.setPatch(i % 2 ? a : b, allParameters, true);
        done.store(true);
    });
    unsigned mixed = 0, successful = 0;
    do {
        if (!control.tryCopyTo(audio))
            continue;
        ++successful;
        const auto values = audio.snapshot();
        const auto reset = audio.stateResetSerial();
        // First A is serial 1, next B is 2, next A is 3 ...
        if (values != (reset % 2 ? a : b))
            ++mixed;
    } while (!done.load());
    writer.join();
    ASSERT_TRUE(control.tryCopyTo(audio));
    EXPECT_GT(successful, 0u);
    EXPECT_EQ(mixed, 0u);
    EXPECT_EQ(audio.snapshot(), a);
    EXPECT_EQ(audio.stateResetSerial(), 20001u);
}

TEST(PatchPublicationTest, CancellationIsRecheckedInsideWriterOwnership) {
    cs01::ParameterState control;
    const auto before = control.snapshot(), after = patch(.8f);
    std::atomic<bool> entered{false}, resume{false}, cancelled{false};
    bool committed = true;
    std::thread writer([&] {
        committed = control.trySetPatchIf(
            after, allParameters,
            [&] {
                entered.store(true);
                while (!resume.load())
                    std::this_thread::yield();
                return !cancelled.load();
            },
            true);
    });
    const bool started = waitFor(entered);
    cancelled.store(true);
    resume.store(true);
    writer.join();
    ASSERT_TRUE(started);
    EXPECT_FALSE(committed);
    EXPECT_EQ(control.snapshot(), before);
    EXPECT_EQ(control.stateResetSerial(), 0u);
    ASSERT_TRUE(control.setPatch(after, allParameters, true));
    EXPECT_EQ(control.snapshot(), after);
}

TEST(PatchPublicationTest, InvalidPatchLeavesValuesAndResetSerialUntouched) {
    cs01::ParameterState control;
    const auto before = control.snapshot();
    auto invalid = patch(.8f);
    invalid[static_cast<int>(cs01::Param::Cutoff)] = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(control.trySetPatch(invalid, allParameters, true));
    EXPECT_FALSE(control.setPatch(invalid, allParameters, true));
    EXPECT_FALSE(control.trySetPatch(before, 1u << cs01::parameterCount, true));
    EXPECT_EQ(control.snapshot(), before);
    EXPECT_EQ(control.stateResetSerial(), 0u);
}

TEST(PatchPublicationTest, PresetMaskPreservesPerformanceAndOnlySessionsRequestNoteReset) {
    cs01::ParameterState control, audio;
    ProgramManager programs(control);
    control.set(cs01::Param::Volume, .31f);
    control.set(cs01::Param::ModDepth, .6f);
    control.set(cs01::Param::PitchBend, -.4f);
    control.set(cs01::Param::BreathInput, .8f);
    ASSERT_TRUE(programs.setCurrentProgram(2));
    ASSERT_TRUE(control.tryCopyTo(audio));
    EXPECT_FLOAT_EQ(audio.get(cs01::Param::Volume), .31f);
    EXPECT_FLOAT_EQ(audio.get(cs01::Param::ModDepth), .6f);
    EXPECT_FLOAT_EQ(audio.get(cs01::Param::PitchBend), -.4f);
    EXPECT_FLOAT_EQ(audio.get(cs01::Param::BreathInput), .8f);
    EXPECT_EQ(audio.stateResetSerial(), 0u);
    const auto saved = programs.getStateInformation();
    control.set(cs01::Param::Volume, .9f);
    ASSERT_TRUE(programs.setStateInformation(saved));
    // Audio owns the previously copied values until its next boundary.
    EXPECT_EQ(audio.stateResetSerial(), 0u);
    ASSERT_TRUE(control.tryCopyTo(audio));
    EXPECT_FLOAT_EQ(audio.get(cs01::Param::Volume), .31f);
    EXPECT_FLOAT_EQ(audio.get(cs01::Param::ModDepth), .6f);
    EXPECT_FLOAT_EQ(audio.get(cs01::Param::PitchBend), -.4f);
    EXPECT_FLOAT_EQ(audio.get(cs01::Param::BreathInput), .8f);
    EXPECT_EQ(audio.stateResetSerial(), 1u);
}

TEST(PatchPublicationTest, SessionSerializationCapturesOneCompleteConcurrentPatch) {
    cs01::ParameterState control, restored;
    ProgramManager programs(control), restoredPrograms(restored);
    const auto a = patch(.2f), b = patch(.8f);
    ASSERT_TRUE(control.setPatch(a, allParameters));
    std::atomic<bool> done{false};
    std::thread writer([&] {
        while (!done.load()) {
            control.setPatch(b, allParameters);
            control.setPatch(a, allParameters);
        }
    });
    unsigned invalid = 0;
    for (int sample = 0; sample < 100; ++sample) {
        const auto state = programs.getStateInformation();
        if (!restoredPrograms.setStateInformation(state)) {
            ++invalid;
            continue;
        }
        bool matchesA = true, matchesB = true;
        for (int i = 0; i < cs01::parameterCount; ++i)
            if (!cs01::parameterDefinitions[i].transient) {
                const auto value = restored.get(static_cast<cs01::Param>(i));
                matchesA &= std::abs(value - a[i]) < 1.e-5f;
                matchesB &= std::abs(value - b[i]) < 1.e-5f;
            }
        invalid += !matchesA && !matchesB;
    }
    done.store(true);
    writer.join();
    EXPECT_EQ(invalid, 0u);
}
