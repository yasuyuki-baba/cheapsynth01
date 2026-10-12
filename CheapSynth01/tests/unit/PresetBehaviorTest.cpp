#include "ProgramManager.h"
#include "SynthEngine.h"
#include <gtest/gtest.h>
#include <tinyxml2.h>
#include <array>
#include <chrono>
#include <fstream>
#include <cmath>

namespace {
using cs01::Param;
struct PresetBehaviorTest : testing::Test {
    cs01::ParameterState state;
    ProgramManager manager{state};
    std::filesystem::path directory;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
                    ("cs01-preset-behavior-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        manager.setUserDirectory(directory);
    }
    void TearDown() override {
        std::filesystem::remove_all(directory);
    }
    int index(std::string_view filename) {
        const auto programs = manager.programs();
        for (size_t i = 0; i < programs.size(); ++i)
            if (programs[i].filename == filename && programs[i].type == PresetType::User)
                return static_cast<int>(i);
        return -1;
    }
};
const std::array performanceIds{Param::Volume, Param::BreathInput, Param::PitchBend,
                                Param::ModDepth};
const std::array performanceValues{.3f, .2f, -.5f, .4f};
}  // namespace

TEST_F(PresetBehaviorTest, EveryEmbeddedFactoryControlReloadsAndPreservesLiveInputs) {
    const auto programs = manager.programs();
    ASSERT_EQ(programs.size(), 39u);
    EXPECT_EQ(manager.getCurrentProgram(), 0);
    EXPECT_NE(programs[0].name, programs[1].name);
    for (int preset = 0; preset < static_cast<int>(programs.size()); ++preset) {
        SCOPED_TRACE(programs[preset].filename);
        EXPECT_FALSE(programs[preset].name.empty());
        tinyxml2::XMLDocument xml;
        const auto file = std::filesystem::path(CS01_SOURCE_DIR) /
                          "CheapSynth01/core/resources/presets" / programs[preset].filename;
        ASSERT_EQ(xml.LoadFile(file.string().c_str()), tinyxml2::XML_SUCCESS);
        const auto* root = xml.FirstChildElement("Parameters");
        ASSERT_NE(root, nullptr);
        std::array<float, cs01::parameterCount> expected{};
        unsigned mask = 0;
        int children = 0;
        for (const auto* child = root->FirstChildElement("PARAM"); child;
             child = child->NextSiblingElement("PARAM")) {
            ++children;
            ASSERT_NE(child->Attribute("id"), nullptr);
            float value = 0;
            ASSERT_EQ(child->QueryFloatAttribute("value", &value), tinyxml2::XML_SUCCESS);
            int id = 0;
            while (id < cs01::parameterCount &&
                   cs01::parameterDefinitions[id].id != child->Attribute("id"))
                ++id;
            ASSERT_LT(id, cs01::parameterCount);
            expected[id] = value;
            mask |= 1u << id;
            const auto& d = cs01::parameterDefinitions[id];
            state.set(static_cast<Param>(id),
                      cs01::toNormalized(static_cast<Param>(id), value) < .5f ? d.maximum
                                                                              : d.minimum);
        }
        EXPECT_EQ(children, 18);
        for (size_t i = 0; i < performanceIds.size(); ++i)
            state.set(performanceIds[i], performanceValues[i]);
        ASSERT_TRUE(manager.setCurrentProgram(preset));
        EXPECT_EQ(manager.getCurrentProgram(), preset);
        for (int id = 0; id < cs01::parameterCount; ++id)
            if (mask & (1u << id))
                EXPECT_NEAR(state.get(static_cast<Param>(id)), expected[id], .001f);
        for (size_t i = 0; i < performanceIds.size(); ++i)
            EXPECT_FLOAT_EQ(state.get(performanceIds[i]), performanceValues[i]);
    }
}

TEST_F(PresetBehaviorTest, ManualFactorySoundsMatchIndependentPanelReadings) {
    struct Reading {
        const char* filename;
        int wave, feet, resonance, target;
        std::array<float, 11> positions;
    };
    const std::array ids{Param::Cutoff,   Param::VcfEgDepth, Param::VcaEgDepth, Param::Attack,
                         Param::Decay,    Param::Sustain,    Param::Release,    Param::LfoSpeed,
                         Param::PwmSpeed, Param::BreathVcf,  Param::BreathVca};
    const Reading readings[]{
        {"Flute.xml", 0, 2, 0, 0, {.2f, .4f, 1, .2f, 0, 1, 0, 0, 0, .7f, .7f}},
        {"Violin.xml", 1, 3, 0, 0, {.75f, .45f, 1, .1f, .1f, 1, 0, .75f, 0, .7f, 0}},
        {"Trumpet.xml", 1, 2, 0, 0, {.35f, .5f, 1, .1f, .2f, .5f, 0, 0, 0, 0, 0}},
        {"Clavinet.xml", 3, 1, 1, 1, {.6f, .5f, 1, 0, .2f, 0, .65f, .75f, 0, .3f, .3f}},
        {"Solo_Synth_Lead.xml", 2, 3, 0, 0, {.35f, 1, 1, 0, .1f, .3f, .3f, .55f, .6f, 0, 0}},
        {"Synth_Bass.xml", 3, 0, 1, 1, {.4f, .25f, 1, 0, .3f, 0, .5f, .1f, 0, 0, 0}}};
    const auto programs = manager.programs();
    for (const auto& reading : readings) {
        SCOPED_TRACE(reading.filename);
        int selected = -1;
        for (size_t i = 0; i < programs.size(); ++i)
            if (programs[i].filename == reading.filename)
                selected = static_cast<int>(i);
        ASSERT_GE(selected, 0);
        ASSERT_TRUE(manager.setCurrentProgram(selected));
        EXPECT_FLOAT_EQ(state.get(Param::WaveType), reading.wave);
        EXPECT_FLOAT_EQ(state.get(Param::Feet), reading.feet);
        EXPECT_FLOAT_EQ(state.get(Param::Resonance), reading.resonance);
        EXPECT_FLOAT_EQ(state.get(Param::LfoTarget), reading.target);
        EXPECT_FLOAT_EQ(state.get(Param::FilterType), 0);
        EXPECT_NEAR(state.get(Param::Pitch), 0, .000001);
        EXPECT_FLOAT_EQ(state.get(Param::Glissando), 0);
        for (size_t i = 0; i < ids.size(); ++i) {
            SCOPED_TRACE(cs01::definition(ids[i]).id);
            EXPECT_NEAR(cs01::toNormalized(ids[i], state.get(ids[i])), reading.positions[i], .02);
        }
    }
}

TEST_F(PresetBehaviorTest, EveryFactorySoundIsFiniteAndAudibleWithoutBreath) {
    const auto programs = manager.programs();
    for (double rate : {44100., 48000., 96000.})
        for (int preset = 0; preset < static_cast<int>(programs.size()); ++preset) {
            SCOPED_TRACE(rate);
            SCOPED_TRACE(preset);
            ASSERT_TRUE(manager.setCurrentProgram(preset));
            state.set(Param::BreathInput, 0);
            SynthEngine engine(state);
            engine.prepare(rate);
            engine.handleMidi({0x90, 60, 127});
            float peak = 0;
            for (int i = 0; i < 100 * 128; ++i) {
                const float value = engine.renderSample();
                ASSERT_TRUE(std::isfinite(value));
                peak = std::max(peak, std::abs(value));
            }
            EXPECT_GT(peak, .00001);
            engine.handleMidi({0xb0, 123, 0});
            engine.renderSample();
            engine.panic();
        }
}

TEST_F(PresetBehaviorTest, UserFileRoundTripIncludesBendRangesAndExcludesLiveInputs) {
    state.set(Param::Cutoff, 2345);
    state.set(Param::BendUp, 7);
    state.set(Param::BendDown, 3);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("roundtrip"));
    tinyxml2::XMLDocument xml;
    ASSERT_EQ(xml.LoadFile((directory / "roundtrip.xml").string().c_str()), tinyxml2::XML_SUCCESS);
    const auto* root = xml.FirstChildElement("Parameters");
    ASSERT_NE(root, nullptr);
    for (const auto* child = root->FirstChildElement("PARAM"); child;
         child = child->NextSiblingElement("PARAM"))
        for (auto excluded : performanceIds)
            EXPECT_NE(std::string_view(child->Attribute("id")), cs01::definition(excluded).id);
    const int selected = index("roundtrip.xml");
    ASSERT_GE(selected, 0);
    EXPECT_EQ(manager.programs()[selected].type, PresetType::User);
    state.set(Param::Cutoff, 500);
    state.set(Param::BendUp, 12);
    state.set(Param::BendDown, 0);
    for (size_t i = 0; i < performanceIds.size(); ++i)
        state.set(performanceIds[i], performanceValues[i]);
    ASSERT_TRUE(manager.setCurrentProgram(selected));
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 2345);
    EXPECT_FLOAT_EQ(state.get(Param::BendUp), 7);
    EXPECT_FLOAT_EQ(state.get(Param::BendDown), 3);
    for (size_t i = 0; i < performanceIds.size(); ++i)
        EXPECT_FLOAT_EQ(state.get(performanceIds[i]), performanceValues[i]);
    ASSERT_TRUE(manager.deleteUserPreset(selected));
    EXPECT_FALSE(std::filesystem::exists(directory / "roundtrip.xml"));
}

TEST_F(PresetBehaviorTest, SessionRestoresAllSoundSettingsAndVolumeButKeepsLiveInputs) {
    const std::array settings{std::pair{Param::WaveType, 4.f},   std::pair{Param::Feet, 1.f},
                              std::pair{Param::FilterType, 1.f}, std::pair{Param::Cutoff, 3210.f},
                              std::pair{Param::Attack, .35f},    std::pair{Param::Release, .75f},
                              std::pair{Param::BendUp, 7.f},     std::pair{Param::BendDown, 5.f},
                              std::pair{Param::Volume, .4f}};
    for (const auto& [id, value] : settings)
        state.set(id, value);
    for (auto id : {Param::BreathInput, Param::ModDepth, Param::PitchBend})
        state.set(id, .8f);
    const auto saved = manager.getStateInformation();
    cs01::ParameterState restoredState;
    ProgramManager restored(restoredState);
    restored.setUserDirectory(directory);
    for (auto id : {Param::BreathInput, Param::ModDepth, Param::PitchBend})
        restoredState.set(id, .2f);
    ASSERT_TRUE(restored.setStateInformation(saved));
    for (const auto& [id, value] : settings)
        EXPECT_FLOAT_EQ(restoredState.get(id), state.get(id));
    for (auto id : {Param::BreathInput, Param::ModDepth, Param::PitchBend})
        EXPECT_FLOAT_EQ(restoredState.get(id), .2f);
    tinyxml2::XMLDocument xml;
    ASSERT_EQ(xml.Parse(saved.c_str()), tinyxml2::XML_SUCCESS);
    const auto* root = xml.FirstChildElement("Parameters");
    ASSERT_NE(root, nullptr);
    int children = 0;
    for (const auto* child = root->FirstChildElement("PARAM"); child;
         child = child->NextSiblingElement("PARAM")) {
        ++children;
        for (auto id : {Param::BreathInput, Param::ModDepth, Param::PitchBend})
            EXPECT_NE(std::string_view(child->Attribute("id")), cs01::definition(id).id);
    }
    EXPECT_GT(children, 0);
    const auto before = restoredState.snapshot();
    const auto serial = restoredState.stateResetSerial();
    const auto identity = restored.currentProgramIdentity();
    EXPECT_FALSE(restored.setStateInformation("not a saved session"));
    EXPECT_FALSE(restored.loadPresetFromXml("<UnrelatedDocument/>"));
    EXPECT_EQ(restoredState.snapshot(), before);
    EXPECT_EQ(restoredState.stateResetSerial(), serial);
    EXPECT_EQ(restored.currentProgramIdentity().filename, identity.filename);
}

TEST_F(PresetBehaviorTest, InsertionDeletionAndRefreshRetainSelectionAndEditedSound) {
    state.set(Param::Cutoff, 2345);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("B"));
    ASSERT_TRUE(manager.setCurrentProgram(index("B.xml")));
    state.set(Param::Cutoff, 3456);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("A"));
    EXPECT_EQ(manager.currentProgramIdentity().filename, "B.xml");
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 3456);
    ASSERT_TRUE(manager.deleteUserPreset(index("A.xml")));
    EXPECT_EQ(manager.currentProgramIdentity().filename, "B.xml");
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 3456);
    manager.refreshUserPresets();
    EXPECT_EQ(manager.currentProgramIdentity().filename, "B.xml");
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 3456);
}

TEST_F(PresetBehaviorTest, SessionIdentityTracksSortedInsertionAndKeepsSavedEdits) {
    state.set(Param::Cutoff, 2345);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("B"));
    ASSERT_TRUE(manager.setCurrentProgram(index("B.xml")));
    state.set(Param::Cutoff, 2500);
    const auto saved = manager.getStateInformation();
    state.set(Param::Cutoff, 4567);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("A"));
    ASSERT_TRUE(manager.setCurrentProgram(0));
    ASSERT_TRUE(manager.setStateInformation(saved));
    EXPECT_EQ(manager.getCurrentProgram(), index("B.xml"));
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 2500);
}

TEST_F(PresetBehaviorTest, RenameMovesSortedSelectionWithoutReloadingEditedSound) {
    state.set(Param::Cutoff, 2345);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("A"));
    state.set(Param::Cutoff, 4567);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("B"));
    ASSERT_TRUE(manager.setCurrentProgram(index("A.xml")));
    state.set(Param::Cutoff, 2500);
    ASSERT_TRUE(manager.renameUserPreset(index("A.xml"), "Z"));
    EXPECT_EQ(manager.currentProgramIdentity().filename, "Z.xml");
    EXPECT_EQ(manager.getCurrentProgram(), index("Z.xml"));
    EXPECT_EQ(manager.getCurrentProgram(), static_cast<int>(manager.programs().size()) - 1);
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 2500);
    ASSERT_TRUE(manager.deleteUserPreset(manager.getCurrentProgram()));
    EXPECT_FALSE(std::filesystem::exists(directory / "Z.xml"));
    EXPECT_EQ(manager.getCurrentProgram(), 0);
    EXPECT_EQ(manager.currentProgramIdentity().name, "Default");
    const float fallback = state.get(Param::Cutoff);
    ASSERT_TRUE(manager.setCurrentProgram(0));
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), fallback);
}

TEST_F(PresetBehaviorTest, SessionRestoresSelectedProgramAndWaveWhileKeepingPerformanceEdits) {
    ASSERT_TRUE(manager.setCurrentProgram(1));
    state.setNormalized(Param::WaveType, .5f);
    state.setNormalized(Param::Volume, .8f);
    state.setNormalized(Param::BreathInput, .3f);
    state.setNormalized(Param::PitchBend, .2f);
    const auto saved = manager.getStateInformation();
    EXPECT_FALSE(saved.empty());
    state.setNormalized(Param::WaveType, 0.f);
    state.setNormalized(Param::Volume, .1f);
    state.setNormalized(Param::BreathInput, .1f);
    state.setNormalized(Param::PitchBend, .1f);
    ASSERT_TRUE(manager.setCurrentProgram(2));
    ASSERT_TRUE(manager.setStateInformation(saved));
    EXPECT_EQ(manager.getCurrentProgram(), 1);
    EXPECT_FLOAT_EQ(cs01::toNormalized(Param::WaveType, state.get(Param::WaveType)), .5f);
    EXPECT_FLOAT_EQ(state.get(Param::Volume), .8f);
    EXPECT_FLOAT_EQ(state.get(Param::BreathInput), .1f);
    EXPECT_NEAR(cs01::toNormalized(Param::PitchBend, state.get(Param::PitchBend)), .1f, .000001);
}

TEST_F(PresetBehaviorTest, SessionSavedImmediatelyAfterMidiEditIncludesItsNewValue) {
    SynthEngine engine(state);
    engine.prepare(48000);
    engine.handleMidi({0xb0, 73, 100});
    const float changed = state.get(Param::Attack);
    const auto saved = manager.getStateInformation();
    state.set(Param::Attack, .001f);
    ASSERT_TRUE(manager.setStateInformation(saved));
    EXPECT_FLOAT_EQ(state.get(Param::Attack), changed);
}
TEST_F(PresetBehaviorTest, SessionRestoresAllRoutesAndResetSerialWithoutSerializingHeldNotes) {
    for (int filter : {0, 1})
        for (int target : {0, 1}) {
            state.set(Param::FilterType, filter);
            state.set(Param::LfoTarget, target);
            state.set(Param::Volume, 1);
            SynthEngine played(state);
            played.prepare(48000);
            played.handleMidi({0x90, 69, 127});
            for (int i = 0; i < 256; ++i)
                played.renderSample();
            const auto saved = manager.getStateInformation();
            state.set(Param::FilterType, 1 - filter);
            state.set(Param::LfoTarget, 1 - target);
            auto reset = state.stateResetSerial();
            ASSERT_TRUE(manager.setStateInformation(saved));
            EXPECT_GT(state.stateResetSerial(), reset);
            EXPECT_FLOAT_EQ(state.get(Param::FilterType), filter);
            EXPECT_FLOAT_EQ(state.get(Param::LfoTarget), target);
            SynthEngine restored(state);
            restored.prepare(48000);
            for (int i = 0; i < 256; ++i)
                EXPECT_FLOAT_EQ(restored.renderSample(), 0);
            EXPECT_TRUE(restored.externalNotes().none());
            restored.handleMidi({0x90, 69, 127});
            double energy = 0;
            for (int i = 0; i < 10240; ++i) {
                float x = restored.renderSample();
                ASSERT_TRUE(std::isfinite(x));
                energy += x * x;
            }
            EXPECT_GT(energy, 1e-6);
        }
}

TEST_F(PresetBehaviorTest, DeleteConfirmationKeepsCapturedIdentityAfterSelectionChanges) {
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("First"));
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("Second"));
    ASSERT_TRUE(manager.setCurrentProgram(index("First.xml")));
    const auto target = manager.currentProgramIdentity();
    ASSERT_TRUE(manager.setCurrentProgram(index("Second.xml")));
    ASSERT_TRUE(manager.deleteUserPreset(target));
    EXPECT_FALSE(manager.userPresetExists("First"));
    EXPECT_TRUE(manager.userPresetExists("Second"));
    EXPECT_EQ(manager.currentProgramIdentity().filename, "Second.xml");
    EXPECT_FALSE(manager.deleteUserPreset(target));
    EXPECT_FALSE(manager.deleteUserPreset(manager.programs().front()));
}

TEST_F(PresetBehaviorTest, PopupSelectionLoadsCapturedIdentityAfterListReorders) {
    state.set(Param::Cutoff, 1234.f);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("Zebra"));
    ASSERT_TRUE(manager.setCurrentProgram(index("Zebra.xml")));
    const auto target = manager.currentProgramIdentity();
    state.set(Param::Cutoff, 4321.f);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("Alpha"));
    ASSERT_TRUE(manager.loadProgramIdentity(target));
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 1234.f);
    ASSERT_TRUE(manager.deleteUserPreset(target));
    EXPECT_FALSE(manager.loadProgramIdentity(target));
}

TEST_F(PresetBehaviorTest, OverwriteQueryFindsFilesOutsideMenuAndRejectsInvalidNames) {
    std::filesystem::create_directories(directory);
    std::ofstream(directory / "External.xml") << "not a valid preset";
    EXPECT_TRUE(manager.userPresetExists("External"));
    EXPECT_FALSE(manager.userPresetExists("Missing"));
    EXPECT_FALSE(manager.userPresetExists("../External"));
    EXPECT_FALSE(manager.userPresetExists("CON"));
}

TEST_F(PresetBehaviorTest, ClavinetAndSynthBassRetainAudibleDecayBeyondInitialClick) {
    for (const char* filename : {"Clavinet.xml", "Synth_Bass.xml"}) {
        const auto programs = manager.programs();
        for (size_t i = 0; i < programs.size(); ++i)
            if (programs[i].filename == filename)
                ASSERT_TRUE(manager.setCurrentProgram(int(i)));
        state.set(Param::BreathInput, 0);
        for (double rate : {44100., 48000., 96000.}) {
            SynthEngine engine(state);
            engine.prepare(rate);
            engine.handleMidi({0x90, 60, 127});
            double energy = 0;
            for (int sample = 0; sample < int(rate * .3); ++sample) {
                const float value = engine.renderSample();
                ASSERT_TRUE(std::isfinite(value));
                if (sample >= int(rate * .1))
                    energy += value * value;
            }
            EXPECT_GT(std::sqrt(energy / (rate * .2)), .00001) << filename << " at " << rate;
        }
    }
}

TEST_F(PresetBehaviorTest, PercussionDecaysOnHeldNotesAndOpenHatLastsLonger) {
    const auto programs = manager.programs();
    double closedTailRatio = 0, openTailRatio = 0;
    for (const char* filename :
         {"Synth_Kick.xml", "Synth_Tom.xml", "Closed_Hi_Hat.xml", "Open_Hi_Hat.xml",
          "Wood_Block.xml", "Cowbell.xml", "Bubble_Drop.xml"}) {
        SCOPED_TRACE(filename);
        const auto selected = std::find_if(programs.begin(), programs.end(),
                                           [&](const auto& p) { return p.filename == filename; });
        ASSERT_NE(selected, programs.end());
        ASSERT_TRUE(manager.setCurrentProgram(static_cast<int>(selected - programs.begin())));
        SynthEngine engine(state);
        engine.prepare(48000);
        engine.handleMidi({0x90, 48, 100});
        double onset = 0, tail = 0, settled = 0;
        // Keep the key held: a percussion patch must finish its own hit.
        for (int sample = 0; sample < 72000; ++sample) {
            const float value = engine.renderSample();
            ASSERT_TRUE(std::isfinite(value));
            const double power = double(value) * value;
            if (sample >= 480 && sample < 2880)
                onset += power;
            if (sample >= 7680 && sample < 10080)
                tail += power;
            if (sample >= 69600)
                settled += power;
        }
        ASSERT_GT(onset, 1e-6);
        EXPECT_LT(settled, onset * 1e-6);
        if (std::string_view(filename) == "Closed_Hi_Hat.xml")
            closedTailRatio = tail / onset;
        if (std::string_view(filename) == "Open_Hi_Hat.xml")
            openTailRatio = tail / onset;
    }
    EXPECT_GT(openTailRatio, .001);
    EXPECT_LT(closedTailRatio, openTailRatio * .1);
}

TEST_F(PresetBehaviorTest, SequenceSoundsRetriggerThroughSixteenthNotePattern) {
    const auto programs = manager.programs();
    for (const char* filename :
         {"Chip_Sequence.xml", "Bubble_Sequence.xml", "Pulse_Sequence.xml", "Acid_Sequence.xml"}) {
        SCOPED_TRACE(filename);
        const auto selected = std::find_if(programs.begin(), programs.end(),
                                           [&](const auto& p) { return p.filename == filename; });
        ASSERT_NE(selected, programs.end());
        ASSERT_TRUE(manager.setCurrentProgram(static_cast<int>(selected - programs.begin())));
        SynthEngine engine(state);
        engine.prepare(48000);
        // Sixteenth notes at 120 BPM, with an 80 ms gate in each 125 ms step.
        for (int note : {60, 67, 72, 67, 60, 64, 69, 64}) {
            engine.handleMidi({0x90, static_cast<uint8_t>(note), 100});
            double energy = 0;
            for (int sample = 0; sample < 6000; ++sample) {
                if (sample == 3840)
                    engine.handleMidi({0x80, static_cast<uint8_t>(note), 0});
                const float value = engine.renderSample();
                ASSERT_TRUE(std::isfinite(value));
                if (sample < 3840)
                    energy += double(value) * value;
            }
            EXPECT_GT(energy, 1e-6);
        }
        double settled = 0;
        for (int sample = 0; sample < 24000; ++sample) {
            const float value = engine.renderSample();
            ASSERT_TRUE(std::isfinite(value));
            if (sample >= 23000)
                settled += double(value) * value;
        }
        EXPECT_LT(settled, 1e-8);
    }
}

TEST_F(PresetBehaviorTest, BreathLeadPlaysWithoutBreathAndRespondsToLiveBreath) {
    const auto programs = manager.programs();
    const auto selected = std::find_if(programs.begin(), programs.end(), [](const auto& p) {
        return p.filename == "Breath_Lead.xml";
    });
    ASSERT_NE(selected, programs.end());
    ASSERT_TRUE(manager.setCurrentProgram(static_cast<int>(selected - programs.begin())));
    cs01::ParameterState withBreath;
    withBreath.setPatch(state.snapshot(), (1u << cs01::parameterCount) - 1);
    state.set(Param::BreathInput, 0);
    withBreath.set(Param::BreathInput, 0);
    SynthEngine dry(state), expressive(withBreath);
    dry.prepare(48000);
    expressive.prepare(48000);
    dry.handleMidi({0x90, 60, 100});
    expressive.handleMidi({0x90, 60, 100});
    double dryEnergy = 0, breathEnergy = 0, difference = 0;
    for (int sample = 0; sample < 24000; ++sample) {
        // Change the live controller during a held note, not before preparation.
        if (sample == 12000)
            expressive.handleMidi({0xb0, 2, 127});
        const float a = dry.renderSample(), b = expressive.renderSample();
        ASSERT_TRUE(std::isfinite(a));
        ASSERT_TRUE(std::isfinite(b));
        if (sample < 12000) {
            EXPECT_FLOAT_EQ(a, b);
        } else {
            dryEnergy += double(a) * a;
            breathEnergy += double(b) * b;
            difference += double(a - b) * (a - b);
        }
    }
    EXPECT_GT(dryEnergy, 1e-6);
    EXPECT_GT(breathEnergy, 1e-6);
    EXPECT_GT(difference, dryEnergy * .01);
}
