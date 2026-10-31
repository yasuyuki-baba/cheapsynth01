#include "ProgramManager.h"
#include "SynthEngine.h"
#include "HostProgramBank.h"
#include "../../config.h"
#include <gtest/gtest.h>
#include <tinyxml2.h>
#include <array>
#include <chrono>
#include <fstream>
#include <cmath>
#include <set>

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
        EXPECT_EQ(programs[preset].category, root->Attribute("category"));
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

TEST_F(PresetBehaviorTest, SaveAsCreatesNumberedCopiesAndPreservesOriginalSound) {
    state.set(Param::Cutoff, 2345);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("Lead"));
    ASSERT_TRUE(manager.setCurrentProgram(index("Lead.xml")));
    state.set(Param::Cutoff, 3456);
    state.set(Param::PitchBend, .5f);
    const auto edited = state.snapshot();
    ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("Lead"));
    EXPECT_EQ(manager.currentProgramIdentity().filename, "Lead (1).xml");
    EXPECT_EQ(state.snapshot(), edited);
    ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("Lead"));
    EXPECT_EQ(manager.currentProgramIdentity().filename, "Lead (2).xml");
    EXPECT_EQ(state.snapshot(), edited);
    cs01::ParameterState restoredState;
    ProgramManager restored(restoredState);
    restored.setUserDirectory(directory);
    ASSERT_TRUE(restored.setStateInformation(manager.getStateInformation()));
    EXPECT_EQ(restored.currentProgramIdentity().filename, "Lead (2).xml");
    EXPECT_FLOAT_EQ(restoredState.get(Param::Cutoff), 3456);
    ASSERT_TRUE(manager.setCurrentProgram(index("Lead.xml")));
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 2345);
    for (const char* filename : {"Lead (1).xml", "Lead (2).xml"}) {
        ASSERT_TRUE(manager.setCurrentProgram(index(filename)));
        EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 3456);
    }
}

TEST_F(PresetBehaviorTest, SaveAsCopiesEditedFactoryPatchAndRejectsInvalidNames) {
    ASSERT_TRUE(manager.setCurrentProgram(1));
    state.set(Param::Cutoff, 4567);
    const auto edited = state.snapshot();
    ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("My Flute"));
    EXPECT_EQ(manager.currentProgramIdentity().filename, "My Flute.xml");
    EXPECT_EQ(manager.currentProgramIdentity().type, PresetType::User);
    EXPECT_EQ(state.snapshot(), edited);
    EXPECT_FALSE(manager.saveCurrentStateAsNewPreset("../invalid"));
    EXPECT_EQ(manager.currentProgramIdentity().filename, "My Flute.xml");
    EXPECT_EQ(state.snapshot(), edited);
    ASSERT_TRUE(manager.setCurrentProgram(1));
    ASSERT_TRUE(manager.setCurrentProgram(index("My Flute.xml")));
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 4567);
}

TEST_F(PresetBehaviorTest, SaveAsKeepsSoundWhenSourceWasRemovedExternally) {
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("Source"));
    ASSERT_TRUE(manager.setCurrentProgram(index("Source.xml")));
    state.set(Param::Cutoff, 5678);
    const auto edited = state.snapshot();
    ASSERT_TRUE(std::filesystem::remove(directory / "Source.xml"));
    ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("Copy"));
    EXPECT_EQ(manager.currentProgramIdentity().filename, "Copy.xml");
    EXPECT_EQ(state.snapshot(), edited);
    ASSERT_TRUE(manager.setCurrentProgram(0));
    ASSERT_TRUE(manager.setCurrentProgram(index("Copy.xml")));
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 5678);
}

TEST_F(PresetBehaviorTest, ModifiedMarkerTracksSoundEditsAndRevertingButIgnoresLiveInputs) {
    ASSERT_TRUE(manager.setCurrentProgram(0));
    EXPECT_FALSE(manager.isCurrentProgramModified());
    const auto original = state.get(Param::Cutoff);
    state.set(Param::Cutoff, 1234);
    EXPECT_TRUE(manager.isCurrentProgramModified());
    state.set(Param::Cutoff, original);
    EXPECT_FALSE(manager.isCurrentProgramModified());
    for (size_t i = 0; i < performanceIds.size(); ++i)
        state.set(performanceIds[i], performanceValues[i]);
    EXPECT_FALSE(manager.isCurrentProgramModified());
    state.set(Param::BendUp, 7);
    EXPECT_TRUE(manager.isCurrentProgramModified());
    ASSERT_TRUE(manager.setCurrentProgram(1));
    EXPECT_FALSE(manager.isCurrentProgramModified());
}

TEST_F(PresetBehaviorTest, SaveOverwritesSelectedUserPatchAndClearsModifiedMarker) {
    const auto factoryCount = manager.programs().size();
    state.set(Param::Cutoff, 1234);
    ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("Lead"));
    EXPECT_FALSE(manager.isCurrentProgramModified());
    state.set(Param::Cutoff, 2345);
    const auto edited = state.snapshot();
    EXPECT_TRUE(manager.isCurrentProgramModified());
    ASSERT_TRUE(manager.saveCurrentUserPreset());
    EXPECT_EQ(manager.currentProgramIdentity().filename, "Lead.xml");
    EXPECT_EQ(state.snapshot(), edited);
    EXPECT_FALSE(manager.isCurrentProgramModified());
    EXPECT_EQ(manager.programs().size(), factoryCount + 1);
    ASSERT_TRUE(manager.setCurrentProgram(0));
    EXPECT_FALSE(manager.saveCurrentUserPreset());
    ASSERT_TRUE(manager.setCurrentProgram(index("Lead.xml")));
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 2345);
    EXPECT_FALSE(manager.isCurrentProgramModified());
}

TEST_F(PresetBehaviorTest, FailedSaveKeepsEditedSoundSelectionAndModifiedMarker) {
    ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("Lead"));
    state.set(Param::Cutoff, 2345);
    const auto edited = state.snapshot();
    ASSERT_TRUE(std::filesystem::remove(directory / "Lead.xml"));
    ASSERT_TRUE(std::filesystem::create_directory(directory / "Lead.xml"));
    EXPECT_FALSE(manager.saveCurrentUserPreset());
    EXPECT_EQ(manager.currentProgramIdentity().filename, "Lead.xml");
    EXPECT_EQ(state.snapshot(), edited);
    EXPECT_TRUE(manager.isCurrentProgramModified());
}

TEST_F(PresetBehaviorTest, RefreshAndRenameKeepModifiedMarkerAndSaveAsClearsIt) {
    ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("Lead"));
    state.set(Param::Cutoff, 2345);
    manager.refreshUserPresets();
    EXPECT_TRUE(manager.isCurrentProgramModified());
    ASSERT_TRUE(manager.renameUserPreset(manager.currentProgramIdentity(), "Renamed"));
    EXPECT_TRUE(manager.isCurrentProgramModified());
    ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("Copy"));
    EXPECT_FALSE(manager.isCurrentProgramModified());
    state.set(Param::Cutoff, 3456);
    ASSERT_TRUE(manager.deleteUserPreset(manager.currentProgramIdentity()));
    EXPECT_EQ(manager.currentProgramIdentity().type, PresetType::Factory);
    EXPECT_FALSE(manager.isCurrentProgramModified());
}

TEST_F(PresetBehaviorTest, SessionRestorePreservesEditedMarkerRelativeToSavedPatch) {
    ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("Lead"));
    const auto cleanSession = manager.getStateInformation();
    state.set(Param::Cutoff, 2345);
    const auto editedSession = manager.getStateInformation();
    cs01::ParameterState restoredState;
    ProgramManager restored(restoredState);
    restored.setUserDirectory(directory);
    ASSERT_TRUE(restored.setStateInformation(editedSession));
    EXPECT_EQ(restored.currentProgramIdentity().filename, "Lead.xml");
    EXPECT_TRUE(restored.isCurrentProgramModified());
    ASSERT_TRUE(restored.setStateInformation(cleanSession));
    EXPECT_FALSE(restored.isCurrentProgramModified());
}

TEST_F(PresetBehaviorTest, ImportMarksCurrentSoundAsEdited) {
    ASSERT_TRUE(manager.setCurrentProgram(0));
    const auto identicalXml = manager.getStateInformation();
    ASSERT_TRUE(manager.loadPresetFromXml(identicalXml));
    EXPECT_TRUE(manager.isCurrentProgramModified());
    ASSERT_TRUE(manager.setCurrentProgram(0));
    ASSERT_TRUE(manager.loadPresetFromXml(
        "<Parameters><PARAM id=\"CUTOFF\" value=\"2345\"/></Parameters>"));
    EXPECT_TRUE(manager.isCurrentProgramModified());
}

TEST_F(PresetBehaviorTest, HostProgramSelectionWithoutAudioCallbackUpdatesParametersCleanly) {
    cs01::HostProgramBank bank(manager.preparePrograms());
    ASSERT_TRUE(bank.request(1));
    EXPECT_EQ(bank.applyPending(state), 1);
    const auto selection = bank.selection();
    ASSERT_NE(selection.program, nullptr);
    EXPECT_FALSE(selection.pending);
    EXPECT_TRUE(manager.selectPreparedProgram(*selection.program));
    EXPECT_FALSE(manager.isCurrentProgramModified());
}

TEST_F(PresetBehaviorTest, SavedPatchAndSessionRecordPluginVersionAndLoadLegacyXml) {
    ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("Versioned"));
    tinyxml2::XMLDocument patch;
    ASSERT_EQ(patch.LoadFile((directory / "Versioned.xml").string().c_str()),
              tinyxml2::XML_SUCCESS);
    const auto* root = patch.FirstChildElement("Parameters");
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->IntAttribute("schema"), 1);
    EXPECT_STREQ(root->Attribute("pluginVersion"), PLUG_VERSION_STR);
    tinyxml2::XMLDocument session;
    ASSERT_EQ(session.Parse(manager.getStateInformation().c_str()), tinyxml2::XML_SUCCESS);
    EXPECT_STREQ(session.FirstChildElement("Parameters")->Attribute("pluginVersion"),
                 PLUG_VERSION_STR);
    ASSERT_TRUE(manager.loadPresetFromXml(
        "<Parameters><PARAM id=\"CUTOFF\" value=\"2345\"/></Parameters>"));
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 2345);
}

TEST_F(PresetBehaviorTest, UnsupportedOrMalformedSchemaLeavesPatchAndSelectionUntouched) {
    ASSERT_TRUE(manager.setCurrentProgram(1));
    state.set(Param::Cutoff, 1234);
    const auto before = state.snapshot();
    const auto reset = state.stateResetSerial();
    const auto selected = manager.getCurrentProgram();
    for (const char* schema : {"0", "2", "-1", "", "invalid", "1junk", "1.0"}) {
        SCOPED_TRACE(schema);
        const auto xml = std::string("<Parameters schema=\"") + schema +
                         "\" program=\"0\"><PARAM id=\"CUTOFF\" value=\"2345\"/></Parameters>";
        EXPECT_FALSE(manager.loadPresetFromXml(xml));
        EXPECT_FALSE(manager.setStateInformation(xml));
        EXPECT_EQ(state.snapshot(), before);
        EXPECT_EQ(state.stateResetSerial(), reset);
        EXPECT_EQ(manager.getCurrentProgram(), selected);
    }
}

TEST_F(PresetBehaviorTest, LegacyUserPatchLoadsShortEnvelopeAndMissingBendValues) {
    const auto file = directory / "Legacy.xml";
    {
        std::ofstream output(file);
        output << "<Parameters><PARAM id=\"CUTOFF\" value=\"1713\"/>"
                  "<PARAM id=\"ATTACK\" value=\"0.001\"/>"
                  "<PARAM id=\"RELEASE\" value=\"0.003\"/>"
                  "<PARAM id=\"PITCH_BEND_UP_RANGE\"/>"
                  "<PARAM id=\"PITCH_BEND_DOWN_RANGE\"/></Parameters>";
    }
    manager.refreshUserPresets();
    ASSERT_GE(index("Legacy.xml"), 0);
    ASSERT_TRUE(manager.setCurrentProgram(index("Legacy.xml")));
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 1713);
    EXPECT_FLOAT_EQ(state.get(Param::Attack), cs01::definition(Param::Attack).minimum);
    EXPECT_FLOAT_EQ(state.get(Param::Release), cs01::definition(Param::Release).minimum);
    EXPECT_FLOAT_EQ(state.get(Param::BendUp), cs01::definition(Param::BendUp).initial);
    EXPECT_FLOAT_EQ(state.get(Param::BendDown), cs01::definition(Param::BendDown).initial);
}

TEST_F(PresetBehaviorTest, LegacyCompatibilityDoesNotAcceptOtherMalformedValues) {
    const auto before = state.snapshot();
    for (const char* xml :
         {"<Parameters schema=\"1\"><PARAM id=\"ATTACK\" value=\"0.001\"/></Parameters>",
          "<Parameters schema=\"1\"><PARAM id=\"PITCH_BEND_UP_RANGE\"/></Parameters>",
          "<Parameters><PARAM id=\"CUTOFF\"/></Parameters>",
          "<Parameters><PARAM id=\"PITCH_BEND_UP_RANGE\" value=\"bad\"/></Parameters>",
          "<Parameters><PARAM id=\"ATTACK\" value=\"-1\"/></Parameters>"}) {
        SCOPED_TRACE(xml);
        EXPECT_FALSE(manager.loadPresetFromXml(xml));
        EXPECT_EQ(state.snapshot(), before);
    }
}

TEST_F(PresetBehaviorTest, OldDecayFilesAndSessionsClampOnlyBelowNewFloorAndRoundTrip) {
    for (const char* schema : {"", " schema=\"1\""})
        for (const char* seconds : {"0.00396", "0.006", "0.008579", "0.00858", "0.1", "4.40396"}) {
            SCOPED_TRACE(schema);
            SCOPED_TRACE(seconds);
            const float expected = std::max(std::stof(seconds), .00858f);
            const auto xml = std::string("<Parameters") + schema + "><PARAM id=\"DECAY\" value=\"" +
                             seconds + "\"/><PARAM id=\"CUTOFF\" value=\"2345\"/></Parameters>";
            std::filesystem::create_directories(directory);
            const auto file = directory / "OldDecay.xml";
            {
                std::ofstream output(file);
                ASSERT_TRUE(output.is_open());
                output << xml;
            }
            ASSERT_TRUE(manager.loadPresetFile(file));
            EXPECT_FLOAT_EQ(state.get(Param::Decay), expected);
            EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 2345);
            ASSERT_TRUE(manager.saveCurrentStateAsPreset("MigratedDecay"));
            state.set(Param::Decay, 1);
            ASSERT_TRUE(manager.loadPresetFile(directory / "MigratedDecay.xml"));
            EXPECT_FLOAT_EQ(state.get(Param::Decay), expected);

            ASSERT_TRUE(manager.setStateInformation(xml));
            EXPECT_FLOAT_EQ(state.get(Param::Decay), expected);
            const auto snapshot = state.snapshot();
            const auto saved = manager.getStateInformation();
            cs01::ParameterState restoredState;
            ProgramManager restored(restoredState);
            restored.setUserDirectory(directory);
            ASSERT_TRUE(restored.setStateInformation(saved));
            EXPECT_EQ(restoredState.snapshot(), snapshot);
            ASSERT_TRUE(manager.setStateInformation(restored.getStateInformation()));
            EXPECT_EQ(state.snapshot(), snapshot);
        }
}

TEST_F(PresetBehaviorTest, SchemaOneDecayMigrationRejectsInvalidValuesAtomically) {
    state.set(Param::Decay, .1f);
    const auto snapshot = state.snapshot();
    const auto serial = state.stateResetSerial();
    const auto selected = manager.currentProgramIdentity();
    for (const char* seconds : {"-1", "0", "0.003959", "4.404", "nan", "inf", "bad"}) {
        SCOPED_TRACE(seconds);
        const auto xml =
            std::string("<Parameters schema=\"1\"><PARAM id=\"CUTOFF\" value=\"2345\"/>") +
            "<PARAM id=\"DECAY\" value=\"" + seconds + "\"/></Parameters>";
        EXPECT_FALSE(manager.loadPresetFromXml(xml));
        EXPECT_FALSE(manager.setStateInformation(xml));
        EXPECT_EQ(state.snapshot(), snapshot);
        EXPECT_EQ(state.stateResetSerial(), serial);
        EXPECT_EQ(manager.currentProgramIdentity().filename, selected.filename);
    }
}

TEST_F(PresetBehaviorTest, IncompleteXmlRestoresDefaultsIndependentOfPreviousPatch) {
    // Both legacy layouts and current schema XML have deterministic missing values.
    for (const char* xml :
         {"<Parameters><PARAM id=\"CUTOFF\" value=\"2345\"/></Parameters>",
          "<Parameters><PARAMETERS><PARAM id=\"CUTOFF\" value=\"2345\"/>"
          "</PARAMETERS></Parameters>",
          "<Parameters schema=\"1\"><PARAM id=\"CUTOFF\" value=\"2345\"/></Parameters>"}) {
        SCOPED_TRACE(xml);
        for (bool session : {false, true}) {
            for (bool maximum : {false, true}) {
                for (int i = 0; i < cs01::parameterCount; ++i) {
                    const auto& d = cs01::parameterDefinitions[i];
                    state.set(static_cast<Param>(i), maximum ? d.maximum : d.minimum);
                }
                const auto before = state.snapshot();
                ASSERT_TRUE(session ? manager.setStateInformation(xml)
                                    : manager.loadPresetFromXml(xml));
                for (int i = 0; i < cs01::parameterCount; ++i) {
                    const auto id = static_cast<Param>(i);
                    const auto& d = cs01::parameterDefinitions[i];
                    const bool retained =
                        d.transient ||
                        (!session && (id == Param::Volume || id == Param::Oversampling));
                    const float expected = retained              ? before[i]
                                           : id == Param::Cutoff ? 2345.f
                                                                 : d.initial;
                    EXPECT_FLOAT_EQ(state.get(id), expected) << d.id;
                }
            }
        }
    }
}

TEST_F(PresetBehaviorTest, LegacyFactoryPresetResetsMissingBendRanges) {
    for (int preset = 0; preset < static_cast<int>(manager.programs().size()); ++preset) {
        SCOPED_TRACE(preset);
        state.set(Param::BendUp, 3);
        state.set(Param::BendDown, 7);
        ASSERT_TRUE(manager.setCurrentProgram(preset));
        EXPECT_FLOAT_EQ(state.get(Param::BendUp), cs01::definition(Param::BendUp).initial);
        EXPECT_FLOAT_EQ(state.get(Param::BendDown), cs01::definition(Param::BendDown).initial);
    }
}

TEST_F(PresetBehaviorTest, MetadataAndTagsSurviveFileAndSessionRoundTrips) {
    const char* xml =
        "<Parameters schema=\"1\" id=\"imported-patch\" author=\"作者 &amp; Co.\" "
        "category=\"Bass\" description=\"Warm &lt;bass&gt; &quot;lead&quot;&#10;Use wheel.\">"
        "<TAGS><TAG>暖かい</TAG><TAG>PWM, slow</TAG><TAG>A &amp; B</TAG></TAGS>"
        "<PARAM id=\"CUTOFF\" value=\"2345\"/></Parameters>";
    ASSERT_TRUE(manager.loadPresetFromXml(xml));
    auto expected = manager.currentPresetMetadata();
    EXPECT_EQ(expected.id, "imported-patch");
    EXPECT_EQ(expected.author, "作者 & Co.");
    EXPECT_EQ(expected.category, "Bass");
    EXPECT_EQ(expected.description, "Warm <bass> \"lead\"\nUse wheel.");
    EXPECT_EQ(expected.tags, (std::vector<std::string>{"暖かい", "PWM, slow", "A & B"}));
    ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("Metadata"));
    EXPECT_EQ(manager.programs()[index("Metadata.xml")].category, "Bass");
    EXPECT_EQ(manager.currentProgramIdentity().category, "Bass");
    expected.id = manager.currentPresetMetadata().id;
    EXPECT_NE(expected.id, "imported-patch");
    ASSERT_TRUE(manager.setCurrentProgram(0));
    ASSERT_TRUE(manager.loadPresetFile(directory / "Metadata.xml"));
    EXPECT_EQ(manager.currentPresetMetadata(), expected);
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 2345);
    ASSERT_TRUE(manager.setCurrentProgram(index("Metadata.xml")));
    const auto session = manager.getStateInformation();
    ASSERT_TRUE(manager.setCurrentProgram(1));
    ASSERT_TRUE(manager.setStateInformation(session));
    EXPECT_EQ(manager.currentPresetMetadata(), expected);
    EXPECT_FALSE(manager.isCurrentProgramModified());
}

TEST_F(PresetBehaviorTest, LegacyXmlClearsMetadataAndInvalidXmlKeepsIt) {
    ASSERT_TRUE(manager.loadPresetFromXml(
        "<Parameters id=\"test\" author=\"Author\" category=\"Bass\" description=\"Hint\">"
        "<TAGS><TAG>Warm</TAG></TAGS><PARAM id=\"CUTOFF\" value=\"2345\"/></Parameters>"));
    const auto before = manager.currentPresetMetadata();
    EXPECT_FALSE(manager.loadPresetFromXml(
        "<Parameters schema=\"2\" author=\"Other\"><PARAM id=\"CUTOFF\" value=\"3456\"/>"
        "</Parameters>"));
    EXPECT_FALSE(manager.setStateInformation("<Parameters author=\"Other\">"));
    EXPECT_EQ(manager.currentPresetMetadata(), before);
    ASSERT_TRUE(
        manager.loadPresetFromXml("<Parameters><PARAMETERS><PARAM id=\"CUTOFF\" value=\"3456\"/>"
                                  "</PARAMETERS></Parameters>"));
    EXPECT_EQ(manager.currentPresetMetadata(), PresetMetadata{});
}

TEST_F(PresetBehaviorTest, SaveOverwriteAndRenameKeepIdWhileSaveAsCreatesNewIds) {
    ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("Original"));
    const auto originalId = manager.currentPresetMetadata().id;
    ASSERT_EQ(originalId.size(), 36u);
    EXPECT_EQ(originalId[14], '4');
    EXPECT_NE(std::string_view("89ab").find(originalId[19]), std::string_view::npos);
    for (size_t i = 0; i < originalId.size(); ++i)
        if (i == 8 || i == 13 || i == 18 || i == 23)
            EXPECT_EQ(originalId[i], '-');
        else
            EXPECT_NE(std::string_view("0123456789abcdef").find(originalId[i]),
                      std::string_view::npos);
    state.set(Param::Cutoff, 1234);
    ASSERT_TRUE(manager.saveCurrentUserPreset());
    EXPECT_EQ(manager.currentPresetMetadata().id, originalId);
    ASSERT_TRUE(manager.renameUserPreset(manager.currentProgramIdentity(), "Renamed"));
    EXPECT_EQ(manager.currentPresetMetadata().id, originalId);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("Renamed"));
    EXPECT_EQ(manager.currentPresetMetadata().id, originalId);
    std::set<std::string> ids{originalId};
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("Copy"));
        EXPECT_TRUE(ids.insert(manager.currentPresetMetadata().id).second);
        EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 1234);
    }
    ASSERT_TRUE(manager.setCurrentProgram(index("Renamed.xml")));
    EXPECT_EQ(manager.currentPresetMetadata().id, originalId);
}

TEST_F(PresetBehaviorTest, LegacyUserFileGetsIdOnFirstSaveAndFailedSaveKeepsMetadata) {
    std::ofstream(directory / "Legacy.xml")
        << "<Parameters author=\"Legacy author\"><PARAM id=\"CUTOFF\" value=\"2345\"/>"
           "</Parameters>";
    manager.refreshUserPresets();
    ASSERT_TRUE(manager.setCurrentProgram(index("Legacy.xml")));
    EXPECT_TRUE(manager.currentPresetMetadata().id.empty());
    ASSERT_TRUE(manager.saveCurrentUserPreset());
    const auto before = manager.currentPresetMetadata();
    EXPECT_FALSE(before.id.empty());
    EXPECT_EQ(before.author, "Legacy author");
    std::filesystem::remove(directory / "Legacy.xml");
    std::filesystem::create_directory(directory / "Legacy.xml");
    EXPECT_FALSE(manager.saveCurrentUserPreset());
    EXPECT_FALSE(manager.saveCurrentStateAsNewPreset("../Invalid"));
    EXPECT_EQ(manager.currentPresetMetadata(), before);
    EXPECT_EQ(manager.currentProgramIdentity().filename, "Legacy.xml");
}

TEST_F(PresetBehaviorTest, SessionAndRefreshFindRenamedPresetById) {
    ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("Original"));
    const auto originalId = manager.currentPresetMetadata().id;
    state.set(Param::Cutoff, 1234);
    const auto editedSession = manager.getStateInformation();
    std::filesystem::rename(directory / "Original.xml", directory / "Renamed.xml");
    manager.refreshUserPresets();
    EXPECT_EQ(manager.currentProgramIdentity().filename, "Renamed.xml");
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 1234);
    EXPECT_EQ(manager.currentPresetMetadata().id, originalId);
    ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("Original"));
    EXPECT_NE(manager.currentPresetMetadata().id, originalId);
    cs01::ParameterState restoredState;
    ProgramManager restored(restoredState);
    restored.setUserDirectory(directory);
    ASSERT_TRUE(restored.setStateInformation(editedSession));
    EXPECT_EQ(restored.currentProgramIdentity().filename, "Renamed.xml");
    EXPECT_EQ(restored.currentPresetMetadata().id, originalId);
    EXPECT_FLOAT_EQ(restoredState.get(Param::Cutoff), 1234);
    EXPECT_TRUE(restored.isCurrentProgramModified());
}

TEST_F(PresetBehaviorTest, DuplicateIdsUseFilenameAndDoNotSelectAnArbitraryCopy) {
    ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("Original"));
    const auto session = manager.getStateInformation();
    std::filesystem::copy_file(directory / "Original.xml", directory / "Copy.xml");
    manager.refreshUserPresets();
    ASSERT_TRUE(manager.setStateInformation(session));
    EXPECT_EQ(manager.currentProgramIdentity().filename, "Original.xml");
    std::filesystem::rename(directory / "Original.xml", directory / "Renamed.xml");
    cs01::ParameterState restoredState;
    ProgramManager restored(restoredState);
    restored.setUserDirectory(directory);
    ASSERT_TRUE(restored.setStateInformation(session));
    EXPECT_EQ(restored.getCurrentProgram(), 0);
    EXPECT_TRUE(restored.isCurrentProgramModified());
}

TEST_F(PresetBehaviorTest, PendingHostSessionKeepsCachedMetadataAfterRename) {
    ASSERT_TRUE(manager.loadPresetFromXml(
        "<Parameters author=\"Author\" category=\"Bass\"><TAGS><TAG>Warm</TAG></TAGS>"
        "<PARAM id=\"CUTOFF\" value=\"2345\"/></Parameters>"));
    ASSERT_TRUE(manager.saveCurrentStateAsNewPreset("Host"));
    const auto expected = manager.currentPresetMetadata();
    cs01::HostProgramBank bank(manager.preparePrograms());
    const auto hostIndex = manager.getCurrentProgram();
    ASSERT_TRUE(manager.setCurrentProgram(0));
    ASSERT_TRUE(bank.request(hostIndex));
    const auto session = bank.serializeSession(manager);
    ASSERT_TRUE(manager.renameUserPreset(index("Host.xml"), "Renamed"));
    ASSERT_TRUE(manager.setStateInformation(session));
    EXPECT_EQ(manager.currentProgramIdentity().filename, "Renamed.xml");
    EXPECT_EQ(manager.currentPresetMetadata(), expected);
    EXPECT_FLOAT_EQ(state.get(Param::Cutoff), 2345);
    ASSERT_EQ(bank.applyPending(state), hostIndex);
    ASSERT_TRUE(manager.selectPreparedProgram(*bank.selected()));
    EXPECT_EQ(manager.currentProgramIdentity().filename, "Renamed.xml");
    EXPECT_EQ(manager.currentPresetMetadata(), expected);
}

TEST_F(PresetBehaviorTest, EveryFactoryPresetHasAUniqueStableId) {
    std::set<std::string> ids;
    for (const auto& program : manager.preparePrograms()) {
        SCOPED_TRACE(program.identity.filename);
        EXPECT_TRUE(program.valid);
        EXPECT_FALSE(program.metadata.id.empty());
        EXPECT_TRUE(ids.insert(program.metadata.id).second);
    }
    const auto legacySession = manager.getStateInformation();
    tinyxml2::XMLDocument doc;
    ASSERT_EQ(doc.Parse(legacySession.c_str()), tinyxml2::XML_SUCCESS);
    doc.FirstChildElement("Parameters")->DeleteAttribute("id");
    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    ASSERT_TRUE(manager.setStateInformation(printer.CStr()));
    EXPECT_FALSE(manager.isCurrentProgramModified());
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
    engine.handleMidi({0xb0, 7, 100});
    const float changed = state.get(Param::Volume);
    EXPECT_NEAR(changed, 12800 / 16383.f, 1e-6);
    const auto saved = manager.getStateInformation();
    state.set(Param::Volume, .1f);
    ASSERT_TRUE(manager.setStateInformation(saved));
    EXPECT_FLOAT_EQ(state.get(Param::Volume), changed);
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
