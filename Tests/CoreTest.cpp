#include "SynthEngine.h"
#include "ProgramManager.h"
#include "HostProgramBank.h"
#include "DSP/MidiQueue.h"
#include "UI/KeyboardInput.h"
#include <gtest/gtest.h>
#include <fstream>
#include <iterator>
#include <numeric>
#include <vector>
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <thread>

namespace {
std::atomic<bool> countAllocations{false};
std::atomic<int> allocations{0};
}  // namespace
void* operator new(std::size_t size) {
    if (countAllocations.load())
        allocations.fetch_add(1);
    if (void* p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}
void* operator new[](std::size_t size) {
    return ::operator new(size);
}
void operator delete[](void* p) noexcept {
    std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept {
    std::free(p);
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t size, const std::nothrow_t& tag) noexcept {
    return ::operator new(size, tag);
}
void operator delete(void* p, const std::nothrow_t&) noexcept {
    std::free(p);
}
void operator delete[](void* p, const std::nothrow_t&) noexcept {
    std::free(p);
}
namespace {
using cs01::Param;
cs01::MidiMessage noteOn(int note = 69) {
    return {0x90, static_cast<uint8_t>(note), 100};
}
cs01::MidiMessage noteOff(int note = 69) {
    return {0x80, static_cast<uint8_t>(note), 0};
}
cs01::MidiMessage cc(int number, int value) {
    return {0xb0, static_cast<uint8_t>(number), static_cast<uint8_t>(value)};
}
std::vector<float> render(SynthEngine& engine, int frames) {
    std::vector<float> output(frames);
    for (auto& value : output)
        value = engine.renderSample();
    return output;
}
double energy(const std::vector<float>& data) {
    return std::inner_product(data.begin(), data.end(), data.begin(), 0.0);
}
struct EngineTest : testing::Test {
    cs01::ParameterState parameters;
    SynthEngine engine{parameters};
    void SetUp() override {
        parameters.set(Param::Attack, .001f);
        parameters.set(Param::Decay, .001f);
        parameters.set(Param::Sustain, 1);
        parameters.set(Param::Volume, 1);
        engine.prepare(48000);
    }
};
struct StateTest : testing::Test {
    cs01::ParameterState parameters;
    ProgramManager manager{parameters};
    std::filesystem::path directory;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
                    ("cs01-state-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        manager.setUserDirectory(directory);
    }
    void TearDown() override {
        std::filesystem::remove_all(directory);
    }
};
}  // namespace
TEST(ParametersTest, NormalizedMappingMatchesExistingRanges) {
    EXPECT_NEAR(cs01::fromNormalized(Param::Cutoff, .5f), 2002.f, 1.f);
    for (int i = 0; i < cs01::parameterCount; ++i) {
        const auto id = static_cast<Param>(i);
        for (float position : {0.f, .25f, .5f, .75f, 1.f}) {
            const float value = cs01::fromNormalized(id, position);
            if (!cs01::definition(id).integer)
                EXPECT_NEAR(cs01::toNormalized(id, value), position, 1.e-5);
        }
    }
    EXPECT_FLOAT_EQ(cs01::definition(Param::Glissando).maximum, .208f);
}
TEST(ParametersTest, RejectsNonFiniteAndClampsValues) {
    cs01::ParameterState p;
    p.set(Param::Volume, std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(p.get(Param::Volume), .7f);
    p.set(Param::Feet, 8);
    EXPECT_FLOAT_EQ(p.get(Param::Feet), 4);
    p.setNormalized(Param::Volume, .3f, true);
    EXPECT_EQ(p.takeMidiChanges(), 1u << static_cast<int>(Param::Volume));
    EXPECT_EQ(p.takeMidiChanges(), 0u);
}
TEST_F(EngineTest, SilentBeforeNoteAndAudibleAfterNote) {
    EXPECT_EQ(energy(render(engine, 128)), 0);
    engine.handleMidi(noteOn());
    EXPECT_GT(energy(render(engine, 1024)), .001);
}
TEST_F(EngineTest, HighestNotePriorityAndUnmatchedNoteOff) {
    engine.handleMidi(noteOn(60));
    engine.handleMidi(noteOn(72));
    engine.handleMidi(noteOn(65));
    EXPECT_EQ(engine.currentNote(), 72);
    engine.handleMidi(noteOff(72));
    EXPECT_EQ(engine.currentNote(), 65);
    engine.handleMidi(noteOff(40));
    EXPECT_EQ(engine.currentNote(), 65);
    engine.handleMidi(noteOff(65));
    EXPECT_EQ(engine.currentNote(), 60);
}
TEST_F(EngineTest, VelocityZeroIsNoteOff) {
    engine.handleMidi(noteOn());
    engine.handleMidi({0x90, 69, 0});
    EXPECT_EQ(engine.currentNote(), 0);
}
TEST_F(EngineTest, AllNotesOffReleasesAndAllSoundOffClearsResidualAudio) {
    parameters.set(Param::Release, .2f);
    engine.handleMidi(noteOn());
    render(engine, 2048);
    engine.handleMidi(cc(123, 0));
    EXPECT_EQ(engine.currentNote(), 0);
    EXPECT_GT(energy(render(engine, 128)), 0);
    engine.handleMidi(cc(120, 0));
    EXPECT_EQ(energy(render(engine, 128)), 0);
}
TEST_F(EngineTest, NoteReleaseEventuallyBecomesSilent) {
    parameters.set(Param::Release, .01f);
    engine.handleMidi(noteOn());
    render(engine, 1024);
    engine.handleMidi(noteOff());
    render(engine, 48000);
    EXPECT_LT(energy(render(engine, 256)), 1.e-12);
}
TEST_F(EngineTest, ResetControllersPreservesHeldNoteAndPatch) {
    engine.handleMidi(noteOn());
    engine.handleMidi(cc(1, 100));
    engine.handleMidi(cc(2, 100));
    engine.handleMidi(cs01::MidiMessage::pitchWheel(1, 16383));
    engine.handleMidi(cc(121, 0));
    EXPECT_EQ(engine.currentNote(), 69);
    EXPECT_FLOAT_EQ(parameters.get(Param::ModDepth), 0);
    EXPECT_FLOAT_EQ(parameters.get(Param::BreathInput), 0);
    EXPECT_FLOAT_EQ(parameters.get(Param::PitchBend), 0);
    EXPECT_FLOAT_EQ(parameters.get(Param::Volume), 1);
    EXPECT_GT(energy(render(engine, 1024)), 0);
}
TEST_F(EngineTest, FourteenBitControlAndSkewedCutoffMappings) {
    engine.handleMidi(cc(7, 127));
    engine.handleMidi(cc(39, 127));
    EXPECT_FLOAT_EQ(parameters.get(Param::Volume), 1);
    engine.handleMidi(cc(2, 64));
    engine.handleMidi(cc(34, 9));
    EXPECT_NEAR(parameters.get(Param::BreathInput), ((64 << 7) | 9) / 16383.f, 1.e-6);
    engine.handleMidi(cc(74, 64));
    EXPECT_NEAR(parameters.get(Param::Cutoff), cs01::fromNormalized(Param::Cutoff, 64 / 127.f),
                1.e-3);
}
TEST_F(EngineTest, SourceSwitchPreservesHeldAndReleasingNote) {
    engine.handleMidi(noteOn());
    render(engine, 512);
    parameters.set(Param::Feet, 4);
    EXPECT_GT(energy(render(engine, 512)), 0);
    EXPECT_EQ(engine.currentNote(), 69);
    parameters.set(Param::Release, .1f);
    engine.handleMidi(noteOff());
    parameters.set(Param::Feet, 2);
    EXPECT_EQ(engine.currentNote(), 0);
    EXPECT_GT(energy(render(engine, 128)), 0);
}
TEST_F(EngineTest, ReprepareMatchesFreshEngineAtEachRate) {
    for (double rate : {44100., 48000., 96000.}) {
        cs01::ParameterState freshParams;
        SynthEngine fresh(freshParams);
        for (int i = 0; i < cs01::parameterCount; ++i)
            freshParams.set(static_cast<Param>(i), parameters.get(static_cast<Param>(i)));
        fresh.prepare(rate);
        engine.prepare(rate);
        engine.handleMidi(noteOn());
        fresh.handleMidi(noteOn());
        auto a = render(engine, 2048), b = render(fresh, 2048);
        EXPECT_EQ(a, b);
        render(engine, 256);
    }
}
TEST_F(EngineTest, AllWaveformsAndFiltersProduceFiniteAudioAtAllRates) {
    for (double rate : {44100., 48000., 96000.})
        for (int filter : {0, 1})
            for (int wave = 0; wave < 5; ++wave) {
                parameters.set(Param::FilterType, static_cast<float>(filter));
                parameters.set(Param::WaveType, static_cast<float>(wave));
                parameters.set(Param::VcfEgDepth, .6f);
                parameters.set(Param::Resonance, 1);
                engine.prepare(rate);
                engine.handleMidi(noteOn(93));
                auto output = render(engine, 2048);
                for (float sample : output)
                    ASSERT_TRUE(std::isfinite(sample));
                EXPECT_GT(energy(output), 1.e-8);
            }
}
TEST_F(EngineTest, RenderingAndMidiDoNotAllocate) {
    engine.handleMidi(noteOn());
    render(engine, 32);
    allocations.store(0);
    countAllocations.store(true);
    for (int i = 0; i < 2048; ++i) {
        if (i == 128)
            engine.handleMidi(cc(74, 80));
        if (i == 256)
            parameters.set(Param::Feet, 4);
        if (i == 512)
            engine.handleMidi(noteOff());
        engine.renderSample();
    }
    countAllocations.store(false);
    EXPECT_EQ(allocations.load(), 0);
}
TEST(MidiQueueTest, StableOffsetsAndBlockBoundaryCarry) {
    cs01::MidiQueue queue;
    queue.add(64, noteOff());
    queue.add(17, noteOn(60));
    queue.add(17, noteOn(72));
    EXPECT_EQ(queue.peek().offset, 17);
    EXPECT_EQ(queue.peek().message.data1, 60);
    queue.remove();
    EXPECT_EQ(queue.peek().message.data1, 72);
    queue.remove();
    queue.flush(64);
    EXPECT_EQ(queue.peek().offset, 0);
    EXPECT_TRUE(queue.peek().message.isNoteOff());
}
TEST(MidiQueueTest, NegativeOffsetsKeepArrivalOrderAtTheClampedBoundary) {
    cs01::MidiQueue queue;
    queue.add(0, noteOn(60));
    queue.add(-10, noteOn(64));
    queue.add(0, noteOn(67));
    for (const int expected : {60, 64, 67}) {
        ASSERT_FALSE(queue.empty());
        EXPECT_EQ(queue.peek().offset, 0);
        EXPECT_EQ(queue.peek().message.data1, expected);
        queue.remove();
    }
    EXPECT_TRUE(queue.empty());
}
TEST(MidiQueueTest, EmptyBoundaryAppliesDueNotesAndPanicWithoutAdvancingFutureEvents) {
    cs01::ParameterState parameters;
    SynthEngine engine(parameters);
    engine.prepare(48000);
    cs01::MidiQueue queue;
    queue.add(0, noteOn(60));
    queue.add(1, noteOn(72));
    queue.consumeThrough(0, [&](auto message) { engine.handleMidi(message); });
    queue.flush(0);
    EXPECT_EQ(engine.currentNote(), 60);
    ASSERT_FALSE(queue.empty());
    EXPECT_EQ(queue.peek().offset, 1);
    queue.add(0, cs01::MidiMessage{0xb0, 120, 0});
    queue.consumeThrough(0, [&](auto message) { engine.handleMidi(message); });
    EXPECT_FALSE(engine.externalNotes().any());
    queue.consumeThrough(1, [&](auto message) { engine.handleMidi(message); });
    EXPECT_EQ(engine.currentNote(), 72);
    EXPECT_TRUE(queue.empty());
}
TEST(MidiQueueTest, OverflowIsReportedWithoutAllocation) {
    cs01::MidiQueue queue;
    for (int i = 0; i <= cs01::MidiQueue::capacity; ++i)
        queue.add(i, noteOn());
    EXPECT_TRUE(queue.takeOverflow());
    EXPECT_TRUE(queue.empty());
    EXPECT_FALSE(queue.takeOverflow());
}
TEST_F(EngineTest, EventOffsetsAndPartitionedRenderingAgree) {
    for (int partition : {1, 7, 64, 256}) {
        engine.prepare(48000);
        cs01::MidiQueue queue;
        queue.add(17, noteOn());
        queue.add(777, noteOff());
        std::vector<float> output(2048);
        for (int start = 0; start < 2048; start += partition) {
            const int frames = std::min(partition, 2048 - start);
            for (int s = 0; s < frames; ++s) {
                while (!queue.empty() && queue.peek().offset <= s) {
                    engine.handleMidi(queue.peek().message);
                    queue.remove();
                }
                output[start + s] = engine.renderSample();
            }
            queue.flush(frames);
        }
        EXPECT_EQ(energy({output.begin(), output.begin() + 17}), 0);
        EXPECT_GT(energy({output.begin() + 17, output.begin() + 777}), 0);
        static std::vector<float> reference;
        if (partition == 1)
            reference = output;
        else
            EXPECT_EQ(output, reference);
    }
}
TEST(OutputConverterTest, ImpulseDelayAndUnityGain) {
    cs01::OutputConverter converter;
    std::array<float, 64> response{};
    for (int i = 0; i < 256; ++i) {
        converter.push(i == 0 ? 1.f : 0.f);
        if (i % 4 == 0)
            response[i / 4] = converter.output();
    }
    EXPECT_EQ(std::max_element(response.begin(), response.end()) - response.begin(),
              SynthEngine::latency);
    converter.reset();
    for (int i = 0; i < 512; ++i)
        converter.push(1);
    EXPECT_NEAR(converter.output(), 1, 1.e-6);
}
TEST(OutputConverterTest, RejectsAboveHostNyquist) {
    for (double frequency : {.025, .25}) {
        cs01::OutputConverter converter;
        double power = 0;
        for (int i = 0; i < 4096; ++i) {
            converter.push(static_cast<float>(std::sin(2 * std::numbers::pi * frequency * i)));
            if (i > 256 && i % 4 == 0)
                power += std::pow(converter.output(), 2);
        }
        if (frequency < .1)
            EXPECT_GT(power, 400);
        else
            EXPECT_LT(power, 1.e-6);
    }
}
TEST_F(StateTest, LoadsAllExistingFactoryPresets) {
    EXPECT_EQ(manager.programs().size(), 7u);
    for (int i = 0; i < 7; ++i)
        EXPECT_TRUE(manager.setCurrentProgram(i));
    for (const auto& file : std::filesystem::directory_iterator(
             std::filesystem::path(CS01_SOURCE_DIR) / "Source/Resources/Presets"))
        if (file.path().extension() == ".xml")
            EXPECT_TRUE(manager.loadPresetFile(file.path()));
}
TEST_F(StateTest, PresetsPreservePerformanceAndVolume) {
    parameters.set(Param::Volume, .4f);
    parameters.set(Param::BreathInput, .8f);
    parameters.set(Param::ModDepth, .6f);
    parameters.set(Param::PitchBend, .3f);
    ASSERT_TRUE(manager.setCurrentProgram(2));
    EXPECT_FLOAT_EQ(parameters.get(Param::Volume), .4f);
    EXPECT_FLOAT_EQ(parameters.get(Param::BreathInput), .8f);
    EXPECT_FLOAT_EQ(parameters.get(Param::ModDepth), .6f);
    EXPECT_FLOAT_EQ(parameters.get(Param::PitchBend), .3f);
}
TEST_F(StateTest, SessionRestoresVolumeAndPatchButPreservesPerformance) {
    parameters.set(Param::Volume, .4f);
    parameters.set(Param::FilterType, 1);
    parameters.set(Param::Cutoff, 1234);
    ASSERT_TRUE(manager.setCurrentProgram(3));
    parameters.set(Param::Cutoff, 1234);
    const auto state = manager.getStateInformation();
    parameters.set(Param::Volume, .9f);
    parameters.set(Param::Cutoff, 500);
    parameters.set(Param::BreathInput, .8f);
    parameters.set(Param::ModDepth, .6f);
    parameters.set(Param::PitchBend, .3f);
    ASSERT_TRUE(manager.setStateInformation(state));
    EXPECT_FLOAT_EQ(parameters.get(Param::Volume), .4f);
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 1234);
    EXPECT_EQ(manager.getCurrentProgram(), 3);
    EXPECT_FLOAT_EQ(parameters.get(Param::BreathInput), .8f);
    EXPECT_FLOAT_EQ(parameters.get(Param::ModDepth), .6f);
    EXPECT_FLOAT_EQ(parameters.get(Param::PitchBend), .3f);
}
TEST_F(StateTest, TruncatedXmlCannotPartiallyChangePatch) {
    parameters.set(Param::Cutoff, 1200);
    const auto state = manager.getStateInformation();
    parameters.set(Param::Cutoff, 500);
    const auto reset = parameters.stateResetSerial();
    ASSERT_NE(state.rfind("</Parameters>"), std::string::npos);
    EXPECT_FALSE(manager.setStateInformation(state.substr(0, state.rfind("</Parameters>"))));
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 500);
    EXPECT_EQ(parameters.stateResetSerial(), reset);
    ASSERT_TRUE(manager.setStateInformation(state));
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 1200);
}
TEST_F(StateTest, MalformedStateCannotPartiallyChangePatch) {
    const float cutoff = parameters.get(Param::Cutoff);
    EXPECT_FALSE(
        manager.setStateInformation("<Parameters><PARAM id=\"CUTOFF\" value=\"1200\"/><PARAM "
                                    "id=\"VOLUME\" value=\"nan\"/></Parameters>"));
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), cutoff);
    EXPECT_FALSE(manager.setStateInformation("<Wrong/>"));
    EXPECT_FALSE(manager.setStateInformation("<Parameters/>"));
    EXPECT_FALSE(manager.setStateInformation(
        "<Parameters><PARAM id=\"CUTOFF\" value=\"-1\"/></Parameters>"));
}
TEST_F(StateTest, UserPresetRenameRetainsSelectionAndDeletionLoadsDefault) {
    parameters.set(Param::Cutoff, 321);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("Bass & Lead"));
    ASSERT_TRUE(manager.setCurrentProgram(7));
    parameters.set(Param::Cutoff, 444);
    ASSERT_TRUE(manager.renameUserPreset(7, "Renamed"));
    EXPECT_EQ(manager.getCurrentProgram(), 7);
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 444);
    EXPECT_EQ(manager.programs()[7].name, "Renamed");
    ASSERT_TRUE(manager.deleteUserPreset(7));
    EXPECT_EQ(manager.getCurrentProgram(), 0);
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 20000);
}
TEST_F(StateTest, FactoryPresetsAndPathsAreProtected) {
    EXPECT_FALSE(manager.deleteUserPreset(0));
    EXPECT_FALSE(manager.renameUserPreset(0, "Default2"));
    EXPECT_FALSE(manager.saveCurrentStateAsPreset("../escape"));
    EXPECT_FALSE(manager.saveCurrentStateAsPreset(""));
    EXPECT_FALSE(manager.saveCurrentStateAsPreset("bad:name"));
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("Patch"));
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("Patch"));
    EXPECT_TRUE(std::filesystem::exists(directory / "Patch.xml"));
    EXPECT_FALSE(std::filesystem::exists(directory / "Patch (1).xml"));
}
TEST(EnvelopeTest, AttackDecayReleaseDurationsHoldAtAllRates) {
    for (double rate : {44100., 48000., 96000.}) {
        cs01::ParameterState p;
        p.set(Param::Attack, .01f);
        p.set(Param::Decay, .02f);
        p.set(Param::Sustain, .4f);
        p.set(Param::Release, .03f);
        EGProcessor eg(p);
        const double internal = rate * 4;
        eg.prepareToPlay(internal, 1);
        eg.startEnvelope();
        const int attack =
            static_cast<int>(std::ceil(static_cast<double>(p.get(Param::Attack)) * internal));
        for (int i = 0; i < attack; ++i)
            eg.processSample();
        EXPECT_NEAR(eg.getLastOutputForTesting(), 1, 1.e-6);
        const int decay =
            static_cast<int>(std::ceil(static_cast<double>(p.get(Param::Decay)) * internal));
        for (int i = 0; i < decay; ++i)
            eg.processSample();
        EXPECT_NEAR(eg.getLastOutputForTesting(), .4f, 1.e-6);
        eg.releaseEnvelope();
        const int release =
            static_cast<int>(std::ceil(static_cast<double>(p.get(Param::Release)) * internal));
        for (int i = 0; i < release; ++i)
            eg.processSample();
        EXPECT_FALSE(eg.isActive());
        EXPECT_FLOAT_EQ(eg.getLastOutputForTesting(), 0);
    }
}
TEST(ToneTest, SquarePitchAndFeetHoldAtAllRates) {
    for (double rate : {44100., 48000., 96000.})
        for (int feet = 0; feet < 4; ++feet) {
            cs01::ParameterState p;
            p.set(Param::WaveType, 2);
            p.set(Param::Feet, static_cast<float>(feet));
            ToneGenerator tone(p);
            tone.prepare(rate * 4);
            tone.startNote(69, 1, 8192);
            tone.updateBlockRateParameters();
            std::vector<int> crossings;
            float previous = 0;
            const int samples = static_cast<int>(rate * 4);
            for (int i = 0; i < samples; ++i) {
                const float sample = tone.renderSample();
                if (i > samples / 10 && previous < 0 && sample >= 0)
                    crossings.push_back(i);
                previous = sample;
            }
            ASSERT_GT(crossings.size(), 2u);
            const double measured =
                (crossings.size() - 1) * rate * 4 / (crossings.back() - crossings.front());
            const double expected = 440 * std::exp2((feet - 2) * 1.0);
            EXPECT_NEAR(measured, expected, expected * .001);
        }
}
TEST_F(StateTest, UnicodeUserPresetsRoundTrip) {
    parameters.set(Param::Cutoff, 456);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("音色"));
    ASSERT_TRUE(manager.setCurrentProgram(7));
    EXPECT_EQ(manager.programs()[7].name, "音色");
    ASSERT_TRUE(manager.renameUserPreset(7, "フルート"));
    EXPECT_EQ(manager.programs()[7].name, "フルート");
    ASSERT_TRUE(manager.setCurrentProgram(7));
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 456);
}

TEST(PresetSafetyTest, PortableNameValidation) {
    for (const auto* name : {"", "../escape", "a/b", "a\\b", ".", "..", " a", "a ", "a.", "CON",
                             "nul.txt", "COM1", "LPT9", "a:b", "a?b", "a\nb"})
        EXPECT_FALSE(ProgramManager::isValidPresetName(name)) << name;
    for (const auto* name : {"My Preset", "音色", "lead.v2", "COM10"})
        EXPECT_TRUE(ProgramManager::isValidPresetName(name)) << name;
}
TEST_F(StateTest, SavingSameNameOverwritesSoundWithoutCreatingAnotherPreset) {
    parameters.set(Param::Cutoff, 250);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("lead"));
    parameters.set(Param::Cutoff, 500);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("lead"));
    EXPECT_EQ(manager.programs().size(), 8u);
    EXPECT_FALSE(std::filesystem::exists(directory / "lead (1).xml"));
    parameters.set(Param::Cutoff, 1000);
    ASSERT_TRUE(manager.loadPresetFile(directory / "lead.xml"));
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 500);
}
TEST_F(StateTest, RenameUsesCapturedIdentityAndCannotReplaceAnotherPreset) {
    parameters.set(Param::Cutoff, 250);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("z-target"));
    ASSERT_TRUE(manager.setCurrentProgram(7));
    const auto target = manager.currentProgramIdentity();
    parameters.set(Param::Cutoff, 800);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("a-other"));
    ASSERT_TRUE(manager.setCurrentProgram(7));
    const auto other = manager.currentProgramIdentity();
    ASSERT_TRUE(manager.renameUserPreset(target, "renamed"));
    EXPECT_EQ(manager.currentProgramIdentity().filename, other.filename);
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 800);
    EXPECT_TRUE(std::filesystem::is_regular_file(directory / "renamed.xml"));
    EXPECT_FALSE(std::filesystem::exists(directory / "z-target.xml"));
    EXPECT_FALSE(manager.renameUserPreset(target, "stale"));
    ASSERT_TRUE(manager.renameUserPreset(other, "renamed"));
    EXPECT_TRUE(std::filesystem::is_regular_file(directory / "renamed (1).xml"));
    ASSERT_TRUE(manager.loadPresetFile(directory / "renamed.xml"));
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 250);
    ASSERT_TRUE(manager.loadPresetFile(directory / "renamed (1).xml"));
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 800);
}
TEST_F(StateTest, FailedLoadsAndWritesPreserveSelectionAndExistingFile) {
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("good"));
    ASSERT_TRUE(manager.setCurrentProgram(1));
    const float cutoff = parameters.get(Param::Cutoff);
    const auto file = directory / "good.xml";
    std::ofstream(file) << "broken XML";
    EXPECT_FALSE(manager.setCurrentProgram(7));
    EXPECT_EQ(manager.getCurrentProgram(), 1);
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), cutoff);
    std::filesystem::remove(file);
    EXPECT_FALSE(manager.setCurrentProgram(7));
    EXPECT_EQ(manager.getCurrentProgram(), 1);
    std::filesystem::create_directory(file);
    std::ofstream(file / "keep") << "preserve";
    EXPECT_FALSE(manager.saveCurrentStateAsPreset("good"));
    EXPECT_TRUE(std::filesystem::is_regular_file(file / "keep"));
    EXPECT_EQ(manager.getCurrentProgram(), 1);
    EXPECT_FALSE(manager.saveCurrentStateAsPreset("../escape"));
    for (const auto& entry : std::filesystem::directory_iterator(directory))
        EXPECT_NE(entry.path().extension(), ".tmp");
}
TEST_F(StateTest, RemovingSelectedFileDuringRefreshLoadsDefault) {
    parameters.set(Param::Cutoff, 250);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("selected"));
    ASSERT_TRUE(manager.setCurrentProgram(7));
    std::filesystem::remove(directory / "selected.xml");
    manager.refreshUserPresets();
    EXPECT_EQ(manager.getCurrentProgram(), 0);
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 20000);
}

TEST_F(StateTest, PreparedHostRequestsApplyOnlyAtBoundaryAndLatestWins) {
    parameters.set(Param::Cutoff, 1234);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("host"));
    cs01::HostProgramBank bank(manager.preparePrograms());
    parameters.set(Param::Cutoff, 5678);
    parameters.set(Param::Volume, .31f);
    parameters.set(Param::PitchBend, .4f);
    parameters.set(Param::ModDepth, .6f);
    parameters.set(Param::BreathInput, .8f);
    ASSERT_TRUE(bank.request(1));
    ASSERT_TRUE(bank.request(7));
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 5678);
    EXPECT_EQ(bank.selected()->identity.filename, "host.xml");
    EXPECT_TRUE(bank.pending());
    EXPECT_EQ(bank.applyPending(parameters), 7);
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 1234);
    EXPECT_FLOAT_EQ(parameters.get(Param::Volume), .31f);
    EXPECT_FLOAT_EQ(parameters.get(Param::PitchBend), .4f);
    EXPECT_FLOAT_EQ(parameters.get(Param::ModDepth), .6f);
    EXPECT_FLOAT_EQ(parameters.get(Param::BreathInput), .8f);
    EXPECT_FALSE(bank.pending());
    EXPECT_EQ(bank.applyPending(parameters), -1);
    // Selecting the same program again must undo subsequent edits.
    parameters.set(Param::Cutoff, 9999);
    ASSERT_TRUE(bank.request(7));
    EXPECT_EQ(bank.applyPending(parameters), 7);
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 1234);
}
TEST_F(StateTest, PreparedHostSelectionUsesCachedSoundAfterFileChanges) {
    parameters.set(Param::Cutoff, 987);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("cached"));
    cs01::HostProgramBank bank(manager.preparePrograms());
    parameters.set(Param::Cutoff, 4567);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("cached"));
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("aaa"));
    std::filesystem::remove(directory / "cached.xml");
    ASSERT_TRUE(bank.request(7));
    EXPECT_EQ(bank.applyPending(parameters), 7);
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 987);
    EXPECT_EQ(bank.programs().size(), 8u);
    EXPECT_EQ(bank.selected()->identity.filename, "cached.xml");
}
TEST_F(StateTest, PendingHostSessionSavesRequestedIdentityAndSoundBeforeRender) {
    parameters.set(Param::Cutoff, 2100);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("requested"));
    cs01::HostProgramBank bank(manager.preparePrograms());
    parameters.set(Param::Cutoff, 18000);
    parameters.set(Param::Volume, .23f);
    ASSERT_TRUE(bank.request(7));
    const auto state = bank.serializeSession(manager);
    // An earlier sorting insertion must not turn the saved program number into
    // another sound; restore matches filename and retains the requested patch.
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("aaa"));
    cs01::ParameterState restored;
    ProgramManager restoredManager(restored);
    restoredManager.setUserDirectory(directory);
    ASSERT_TRUE(restoredManager.setStateInformation(state));
    EXPECT_FLOAT_EQ(restored.get(Param::Cutoff), 2100);
    EXPECT_FLOAT_EQ(restored.get(Param::Volume), .23f);
    EXPECT_EQ(restoredManager.getCurrentProgram(), 8);
    EXPECT_EQ(bank.applyPending(parameters), 7);
    parameters.set(Param::Cutoff, 3300);
    const auto edited = bank.serializeSession(manager);
    ASSERT_TRUE(restoredManager.setStateInformation(edited));
    EXPECT_FLOAT_EQ(restored.get(Param::Cutoff), 3300);
}
TEST_F(StateTest, InvalidPreparedHostSelectionLeavesEarlierRequestIntact) {
    std::ofstream(directory / "broken.xml")
        << "<Parameters><PARAM id=\"CUTOFF\" value=\"NaN\"/></Parameters>";
    manager.refreshUserPresets();
    cs01::HostProgramBank bank(manager.preparePrograms());
    ASSERT_EQ(bank.programs().size(), 8u);
    EXPECT_FALSE(bank.programs()[7].valid);
    ASSERT_TRUE(bank.request(2));
    EXPECT_FALSE(bank.request(-1));
    EXPECT_FALSE(bank.request(8));
    EXPECT_FALSE(bank.request(7));
    EXPECT_EQ(bank.selected()->identity.filename, "Violin.xml");
    EXPECT_EQ(bank.applyPending(parameters), 2);
}
TEST_F(StateTest, HostRequestChunksValidateVersionLengthAndIndexWithoutAllocation) {
    cs01::HostProgramBank bank(manager.preparePrograms());
    const auto valid = cs01::HostProgramBank::requestChunk(3);
    auto badVersion = valid;
    badVersion[4] = 2;
    const auto badIndex = cs01::HostProgramBank::requestChunk(0xffffffffu);
    auto view = [](const auto& bytes) { return std::string_view(bytes.data(), bytes.size()); };
    ASSERT_TRUE(bank.requestChunk(view(valid)));
    EXPECT_FALSE(bank.requestChunk(view(badVersion)));
    EXPECT_FALSE(bank.requestChunk(view(valid).substr(0, 11)));
    EXPECT_FALSE(bank.requestChunk(view(badIndex)));
    allocations.store(0);
    countAllocations.store(true);
    bool accepted = true;
    for (int i = 0; i < 4096; ++i) {
        accepted &= bank.requestChunk(view(valid));
        accepted &= bank.applyPending(parameters) == 3;
    }
    countAllocations.store(false);
    EXPECT_TRUE(accepted);
    EXPECT_EQ(allocations.load(), 0);
}
TEST_F(StateTest, ExplicitStateRestoreCancelsUnappliedHostSelection) {
    const auto state = manager.getStateInformation();
    cs01::HostProgramBank bank(manager.preparePrograms());
    ASSERT_TRUE(bank.request(4));
    manager.setBeforePatchCommit([&] { bank.cancel(); });
    EXPECT_FALSE(manager.setStateInformation("<Wrong/>"));
    ASSERT_NE(bank.selected(), nullptr);
    ASSERT_TRUE(manager.setStateInformation(state));
    EXPECT_EQ(bank.selected(), nullptr);
    EXPECT_FALSE(bank.pending());
    EXPECT_EQ(bank.applyPending(parameters), -1);
}
TEST_F(StateTest, HostIdentityRefreshKeepsEditedSoundAndTracksSorting) {
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("zzz"));
    cs01::HostProgramBank bank(manager.preparePrograms());
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("aaa"));
    ASSERT_TRUE(bank.request(7));
    ASSERT_EQ(bank.applyPending(parameters), 7);
    parameters.set(Param::Cutoff, 4321);
    ASSERT_TRUE(manager.selectProgramIdentity(bank.selected()->identity));
    EXPECT_EQ(manager.getCurrentProgram(), 8);
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 4321);
}

TEST_F(StateTest, PreparedFactoryChangesMatchDirectLoadsIncludingHeldNoteAudio) {
    const auto catalogue = manager.preparePrograms();
    for (int index = 0; index < 7; ++index) {
        cs01::ParameterState direct, deferred;
        ProgramManager directManager(direct);
        cs01::HostProgramBank bank(catalogue);
        SynthEngine directEngine(direct), deferredEngine(deferred);
        directEngine.prepare(48000);
        deferredEngine.prepare(48000);
        directEngine.handleMidi(noteOn(60));
        deferredEngine.handleMidi(noteOn(60));
        for (int i = 0; i < 300; ++i)
            ASSERT_FLOAT_EQ(directEngine.renderSample(), deferredEngine.renderSample());
        ASSERT_TRUE(directManager.setCurrentProgram(index));
        bool requested = false;
        std::thread host([&] { requested = bank.request(index); });
        host.join();
        ASSERT_TRUE(requested);
        ASSERT_EQ(bank.applyPending(deferred), index);
        for (int i = 0; i < cs01::parameterCount; ++i)
            EXPECT_FLOAT_EQ(direct.get(static_cast<Param>(i)), deferred.get(static_cast<Param>(i)));
        // Preserve held notes/source phase through a preset change, as panel
        // selection does; do not introduce a panic on the host request path.
        for (int i = 0; i < 500; ++i)
            EXPECT_FLOAT_EQ(directEngine.renderSample(), deferredEngine.renderSample());
    }
}
TEST_F(StateTest, HostRequestsStayIndependentOfConcurrentPanelRefreshAndOtherInstances) {
    parameters.set(Param::Cutoff, 2100);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("user"));
    cs01::HostProgramBank bank(manager.preparePrograms());
    cs01::ParameterState audio, otherAudio;
    cs01::HostProgramBank otherBank(manager.preparePrograms());
    std::atomic<bool> done{false};
    std::thread host([&] {
        for (int i = 0; i < 2000; ++i) {
            bank.request(7);
            bank.applyPending(audio);
        }
        done.store(true);
    });
    int refreshes = 0;
    do {
        manager.refreshUserPresets();
        ++refreshes;
    } while (!done.load());
    host.join();
    EXPECT_GT(refreshes, 0);
    EXPECT_FLOAT_EQ(audio.get(Param::Cutoff), 2100);
    EXPECT_EQ(bank.programs().size(), 8u);
    EXPECT_EQ(otherBank.selected(), nullptr);
    EXPECT_EQ(otherBank.applyPending(otherAudio), -1);
    EXPECT_FLOAT_EQ(otherAudio.get(Param::Cutoff), 20000);
}

TEST(ParametersTest, PatchSnapshotAndBoundaryCopyDoNotAllocate) {
    cs01::ParameterState control, audio;
    const auto values = control.snapshot();
    allocations.store(0);
    countAllocations.store(true);
    bool successful = true;
    for (int i = 0; i < 4096; ++i) {
        successful &= control.trySetPatch(values, (1u << cs01::parameterCount) - 1, true);
        successful &= control.tryCopyTo(audio);
    }
    countAllocations.store(false);
    EXPECT_TRUE(successful);
    EXPECT_EQ(allocations.load(), 0);
    EXPECT_EQ(audio.stateResetSerial(), 4096u);
}

TEST(KeyboardInputTest, AudioDrainAndSourceOwnershipDoNotAllocate) {
    cs01::ParameterState parameters;
    SynthEngine engine(parameters);
    engine.prepare(48000);
    cs01::UiNoteBridge bridge;
    allocations.store(0);
    countAllocations.store(true);
    for (int i = 0; i < 4096; ++i) {
        const int note = 36 + i % 48;
        engine.handleMidi(noteOn(note));
        bridge.set(note, 100, cs01::UiNoteBridge::PC);
        bridge.consume([&](auto message) { engine.handleUiMidi(message); });
        bridge.releaseAll();
        bridge.consume([&](auto message) { engine.handleUiMidi(message); });
        bridge.publishExternal(engine.externalNotes());
        engine.renderSample();
        engine.handleMidi(noteOff(note));
    }
    countAllocations.store(false);
    EXPECT_EQ(allocations.load(), 0);
}
