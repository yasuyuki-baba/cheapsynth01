#include "SynthEngine.h"
#include "ProgramManager.h"
#include "DSP/MidiQueue.h"
#include <gtest/gtest.h>
#include <fstream>
#include <iterator>
#include <numeric>
#include <vector>
#include <chrono>
#include <atomic>
#include <cstdlib>

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
TEST_F(StateTest, LoadsLegacyJuceBinaryXmlAndRejectsTruncation) {
    const std::string xml = "<Parameters><PARAM id=\"CUTOFF\" value=\"1200\"/></Parameters>";
    std::string binary = "VC2!";
    for (unsigned i = 0; i < 4; ++i)
        binary.push_back(static_cast<char>((xml.size() >> (8 * i)) & 255));
    binary += xml;
    binary.push_back(0);
    ASSERT_TRUE(manager.setStateInformation(binary));
    EXPECT_FLOAT_EQ(parameters.get(Param::Cutoff), 1200);
    EXPECT_FALSE(manager.setStateInformation(binary.substr(0, 12)));
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
    EXPECT_TRUE(std::filesystem::exists(directory / "Patch (1).xml"));
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
