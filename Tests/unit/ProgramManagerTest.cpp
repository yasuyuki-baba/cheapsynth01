#include <JuceHeader.h>

#include "CS01AudioProcessor.h"
#include "Parameters.h"
#include "ProgramManager.h"

#include <gtest/gtest.h>

TEST(ParameterVersionTest, ProductionParametersHaveStableVersionHints) {
    CS01AudioProcessor processor;
    ASSERT_EQ(processor.getParameters().size(), 24);
    for (auto* parameter : processor.getParameters()) {
        EXPECT_EQ(parameter->getVersionHint(), 1);
    }
}

TEST(ProductionStateTest, UserPresetFileRoundTrip) {
    CS01AudioProcessor host;
    auto& state = host.getValueTreeState();
    ProgramManager manager(state);
    const auto name = "Test-" + juce::Uuid().toString();
    const auto file = manager.getUserPresetsDirectory().getChildFile(name + ".xml");
    struct Cleanup {
        juce::File file;
        ~Cleanup() {
            file.deleteFile();
        }
    } cleanup{file};
    ASSERT_FALSE(file.exists());
    auto set = [&](const juce::String& id, float value) {
        auto* parameter = state.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    set(ParameterIds::cutoff, 2345);
    set(ParameterIds::pitchBendUpRange, 7);
    set(ParameterIds::pitchBendDownRange, 3);
    manager.saveCurrentStateAsPreset(name);
    ASSERT_TRUE(file.existsAsFile());
    auto xml = juce::XmlDocument::parse(file);
    ASSERT_NE(xml, nullptr);
    for (auto* child : xml->getChildIterator()) {
        const auto id = child->getStringAttribute("id");
        for (const auto& excluded : {ParameterIds::volume, ParameterIds::breathInput,
                                     ParameterIds::pitchBend, ParameterIds::modDepth})
            EXPECT_NE(id, excluded);
    }
    int index = -1;
    for (int i = 0; i < manager.getNumPrograms(); ++i)
        if (manager.getProgramName(i) == name)
            index = i;
    ASSERT_GE(index, 0);
    ASSERT_TRUE(manager.isUserPreset(index));
    set(ParameterIds::cutoff, 500);
    set(ParameterIds::pitchBendUpRange, 12);
    set(ParameterIds::pitchBendDownRange, 0);
    set(ParameterIds::volume, 0.3f);
    set(ParameterIds::breathInput, 0.2f);
    set(ParameterIds::pitchBend, -0.5f);
    set(ParameterIds::modDepth, 0.4f);
    manager.setCurrentProgram(index);
    EXPECT_FLOAT_EQ(state.getRawParameterValue(ParameterIds::cutoff)->load(), 2345);
    EXPECT_FLOAT_EQ(state.getRawParameterValue(ParameterIds::pitchBendUpRange)->load(), 7);
    EXPECT_FLOAT_EQ(state.getRawParameterValue(ParameterIds::pitchBendDownRange)->load(), 3);
    EXPECT_NEAR(state.getRawParameterValue(ParameterIds::volume)->load(), 0.3f, 0.001f);
    EXPECT_NEAR(state.getRawParameterValue(ParameterIds::breathInput)->load(), 0.2f, 0.001f);
    EXPECT_NEAR(state.getRawParameterValue(ParameterIds::pitchBend)->load(), -0.5f, 0.001f);
    EXPECT_NEAR(state.getRawParameterValue(ParameterIds::modDepth)->load(), 0.4f, 0.001f);
    EXPECT_TRUE(manager.deleteUserPreset(index));
    EXPECT_FALSE(file.exists());
}

TEST(ProductionStateTest, SessionRestoresSoundSettingsButPreservesLiveInputs) {
    CS01AudioProcessor source, restored;
    auto set = [](CS01AudioProcessor& processor, const juce::String& id, float value) {
        auto* parameter = processor.getValueTreeState().getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    const std::vector<std::pair<juce::String, float>> settings{
        {ParameterIds::waveType, 4},         {ParameterIds::feet, 1},
        {ParameterIds::filterType, 1},       {ParameterIds::cutoff, 3210},
        {ParameterIds::attack, 0.35f},       {ParameterIds::release, 0.75f},
        {ParameterIds::pitchBendUpRange, 7}, {ParameterIds::pitchBendDownRange, 5},
        {ParameterIds::volume, 0.4f}};
    for (const auto& [id, value] : settings)
        set(source, id, value);
    for (const auto& id :
         {ParameterIds::breathInput, ParameterIds::modDepth, ParameterIds::pitchBend}) {
        set(source, id, 0.8f);
        set(restored, id, 0.2f);
    }
    juce::MemoryBlock data;
    source.getStateInformation(data);
    restored.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
    for (const auto& [id, value] : settings) {
        SCOPED_TRACE(id.toStdString());
        EXPECT_FLOAT_EQ(restored.getValueTreeState().getRawParameterValue(id)->load(),
                        source.getValueTreeState().getRawParameterValue(id)->load());
    }
    for (const auto& id :
         {ParameterIds::breathInput, ParameterIds::modDepth, ParameterIds::pitchBend})
        EXPECT_NEAR(restored.getValueTreeState().getRawParameterValue(id)->load(), 0.2f, 0.001f);
    const auto before = restored.getValueTreeState().copyState().toXmlString();
    const char invalid[] = "not a saved session";
    restored.setStateInformation(invalid, sizeof(invalid));
    EXPECT_EQ(restored.getValueTreeState().copyState().toXmlString(), before);
}

class PresetSelectionTest : public ::testing::Test {
   protected:
    void SetUp() override {
        prefix = "Selection-" + juce::Uuid().toString() + "-";
    }
    void TearDown() override {
        for (const auto& file : files)
            file.deleteFile();
    }
    juce::String save(const juce::String& suffix, float cutoff) {
        const auto name = prefix + suffix;
        auto* parameter = host.getValueTreeState().getParameter(ParameterIds::cutoff);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(cutoff));
        auto& manager = host.getPresetManager();
        files.push_back(manager.getUserPresetsDirectory().getChildFile(name + ".xml"));
        manager.saveCurrentStateAsPreset(name);
        return name;
    }
    int index(const juce::String& name) {
        return host.getPresetManager().findProgram(name + ".xml", PresetType::User);
    }
    float cutoff() {
        return host.getValueTreeState().getRawParameterValue(ParameterIds::cutoff)->load();
    }
    CS01AudioProcessor host;
    juce::String prefix;
    std::vector<juce::File> files;
};

TEST_F(PresetSelectionTest, InsertionDeletionAndRefreshPreserveSelectedSound) {
    auto& manager = host.getPresetManager();
    const auto selected = save("B", 2345);
    manager.setCurrentProgram(index(selected));
    const auto inserted = save("A", 3456);
    EXPECT_EQ(manager.getProgramName(manager.getCurrentProgram()), selected);
    EXPECT_FLOAT_EQ(cutoff(), 3456);  // List changes must not erase current edits.
    ASSERT_TRUE(manager.deleteUserPreset(index(inserted)));
    EXPECT_EQ(manager.getProgramName(manager.getCurrentProgram()), selected);
    EXPECT_FLOAT_EQ(cutoff(), 3456);
    manager.refreshUserPresets();
    EXPECT_EQ(manager.getProgramName(manager.getCurrentProgram()), selected);
    EXPECT_FLOAT_EQ(cutoff(), 3456);
}

TEST_F(PresetSelectionTest, RenameMovesSelectionWithFileWithoutReloadingSound) {
    auto& manager = host.getPresetManager();
    const auto selected = save("A", 2345);
    save("B", 4567);
    manager.setCurrentProgram(index(selected));
    const auto renamed = prefix + "Z";
    files.push_back(manager.getUserPresetsDirectory().getChildFile(renamed + ".xml"));
    ASSERT_TRUE(manager.renameUserPreset(index(selected), renamed));
    EXPECT_EQ(manager.getProgramName(manager.getCurrentProgram()), renamed);
    EXPECT_EQ(manager.getCurrentProgram(), index(renamed));
    EXPECT_FLOAT_EQ(cutoff(), 2345);
    ASSERT_TRUE(manager.deleteUserPreset(manager.getCurrentProgram()));
    EXPECT_FALSE(files.back().exists());
    EXPECT_EQ(manager.getCurrentProgram(), 0);
    EXPECT_EQ(manager.getProgramName(0), "Default");
    const float fallbackCutoff = cutoff();
    manager.setCurrentProgram(0);
    EXPECT_FLOAT_EQ(cutoff(), fallbackCutoff);
}

TEST_F(PresetSelectionTest, RemovingSelectedFileDuringRefreshLoadsDefault) {
    auto& manager = host.getPresetManager();
    const auto selected = save("A", 2345);
    manager.setCurrentProgram(index(selected));
    ASSERT_TRUE(files.back().deleteFile());
    manager.refreshUserPresets();
    EXPECT_EQ(manager.getCurrentProgram(), 0);
    const float fallbackCutoff = cutoff();
    manager.setCurrentProgram(0);
    EXPECT_FLOAT_EQ(cutoff(), fallbackCutoff);
}

TEST_F(PresetSelectionTest, SessionIdentitySurvivesInsertionAndKeepsSavedSound) {
    auto& manager = host.getPresetManager();
    const auto selected = save("B", 2345);
    manager.setCurrentProgram(index(selected));
    juce::MemoryBlock state;
    manager.getStateInformation(state);
    save("A", 4567);
    manager.setCurrentProgram(0);
    manager.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    EXPECT_EQ(manager.getCurrentProgram(), index(selected));
    EXPECT_FLOAT_EQ(cutoff(), 2345);
}

TEST_F(PresetSelectionTest, LegacySessionNumberIsSupportedAndClamped) {
    auto& manager = host.getPresetManager();
    auto xml = host.getValueTreeState().copyState().createXml();
    juce::MemoryBlock state;
    xml->setAttribute("program", 2);
    juce::AudioProcessor::copyXmlToBinary(*xml, state);
    manager.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    EXPECT_EQ(manager.getCurrentProgram(), 2);
    for (int invalid : {-1, 999999}) {
        xml->setAttribute("program", invalid);
        juce::AudioProcessor::copyXmlToBinary(*xml, state);
        manager.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        EXPECT_GE(manager.getCurrentProgram(), 0);
        EXPECT_LT(manager.getCurrentProgram(), manager.getNumPrograms());
    }
}

// Test fixture for ProgramManager tests
class ProgramManagerTest : public ::testing::Test {
   protected:
    // Simple AudioProcessor implementation for testing
    class TestAudioProcessor : public juce::AudioProcessor {
       public:
        TestAudioProcessor()
            : AudioProcessor(
                  BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo())) {
            // Minimal initialization
        }

        // Minimal implementation of AudioProcessor base methods
        const juce::String getName() const override {
            return "TestProcessor";
        }
        void prepareToPlay(double, int) override {}
        void releaseResources() override {}
        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override {
            return 0.0;
        }
        bool acceptsMidi() const override {
            return false;
        }
        bool producesMidi() const override {
            return false;
        }
        juce::AudioProcessorEditor* createEditor() override {
            return nullptr;
        }
        bool hasEditor() const override {
            return false;
        }
        int getNumPrograms() override {
            return 1;
        }
        int getCurrentProgram() override {
            return 0;
        }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override {
            return {};
        }
        void changeProgramName(int, const juce::String&) override {}
        void getStateInformation(juce::MemoryBlock&) override {}
        void setStateInformation(const void*, int) override {}
    };

    // Create test parameter layout
    juce::AudioProcessorValueTreeState::ParameterLayout createTestParameterLayout() {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // Add minimal set of parameters for testing
        auto vcoGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
            "vco", "VCO", "|",
            std::make_unique<juce::AudioParameterChoice>(
                ParameterIds::waveType, "Wave Type",
                juce::StringArray{"Triangle", "Sawtooth", "Square", "Pulse", "PWM"}, 1),
            std::make_unique<juce::AudioParameterChoice>(
                ParameterIds::feet, "Feet", juce::StringArray{"32'", "16'", "8'", "4'", "WN"}, 2));
        layout.add(std::move(vcoGroup));

        auto modGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
            "mod", "Modulation", "|",
            std::make_unique<juce::AudioParameterFloat>(ParameterIds::pitchBend, "Pitch Bend",
                                                        juce::NormalisableRange<float>(0.0f, 12.0f),
                                                        0.0f));
        layout.add(std::move(modGroup));

        auto globalGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
            "global", "Global", "|",
            std::make_unique<juce::AudioParameterFloat>(
                ParameterIds::volume, "Volume", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f),
            std::make_unique<juce::AudioParameterFloat>(ParameterIds::breathInput, "Breath Input",
                                                        juce::NormalisableRange<float>(0.0f, 1.0f),
                                                        0.0f));
        layout.add(std::move(globalGroup));

        return layout;
    }

    void SetUp() override {
        // Test setup if needed
    }

    void TearDown() override {
        // Test cleanup if needed
    }
};

TEST_F(ProgramManagerTest, FactoryPresets) {
    // Create mock AudioProcessor and parameters
    TestAudioProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters",
                                             createTestParameterLayout());

    // Instantiate ProgramManager
    ProgramManager programManager(apvts);

    // Verify program count (factory presets)
    EXPECT_GT(programManager.getNumPrograms(), 0) << "Should have at least one factory preset";

    // Verify program names
    for (int i = 0; i < programManager.getNumPrograms(); ++i) {
        EXPECT_FALSE(programManager.getProgramName(i).isEmpty())
            << "Program name should not be empty for index " << i;
    }

    // Verify default initial program number
    EXPECT_EQ(programManager.getCurrentProgram(), 0) << "Initial program should be 0";
}

TEST_F(ProgramManagerTest, ProgramSelection) {
    // Create mock AudioProcessor and parameters
    TestAudioProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters",
                                             createTestParameterLayout());

    // Instantiate ProgramManager
    ProgramManager programManager(apvts);

    // Get program count
    int numPrograms = programManager.getNumPrograms();
    EXPECT_GT(numPrograms, 1) << "Need at least 2 programs for selection test";

    if (numPrograms > 1) {
        // Store initial program
        int initialProgram = programManager.getCurrentProgram();

        // Change to a different program
        int newProgram = (initialProgram + 1) % numPrograms;
        programManager.setCurrentProgram(newProgram);

        // Verify program was changed
        EXPECT_EQ(programManager.getCurrentProgram(), newProgram)
            << "Current program should be updated after setCurrentProgram";

        // Verify different program names can be retrieved
        juce::String name1 = programManager.getProgramName(initialProgram);
        juce::String name2 = programManager.getProgramName(newProgram);
        EXPECT_TRUE(name1 != name2 || name1.isEmpty())
            << "Different presets should have different names";
    }
}

TEST_F(ProgramManagerTest, SerializedSessionOmitsPerformanceInputs) {
    TestAudioProcessor processor;
    juce::AudioProcessorValueTreeState state(processor, nullptr, "Parameters",
                                             createTestParameterLayout());
    ProgramManager manager(state);
    juce::MemoryBlock saved;
    manager.getStateInformation(saved);
    auto xml =
        juce::AudioProcessor::getXmlFromBinary(saved.getData(), static_cast<int>(saved.getSize()));
    ASSERT_NE(xml, nullptr);
    for (auto* child : xml->getChildIterator()) {
        const auto id = child->getStringAttribute("id");
        EXPECT_NE(id, ParameterIds::breathInput);
        EXPECT_NE(id, ParameterIds::pitchBend);
        EXPECT_NE(id, ParameterIds::modDepth);
    }
    EXPECT_GT(xml->getNumChildElements(), 0);
    const auto before = state.copyState().toXmlString();
    juce::XmlElement invalid("UnrelatedDocument");
    manager.loadPresetFromXml(&invalid);
    EXPECT_EQ(state.copyState().toXmlString(), before);
}

TEST_F(ProgramManagerTest, StateManagement) {
    // Create mock AudioProcessor and parameters
    TestAudioProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters",
                                             createTestParameterLayout());

    // Instantiate ProgramManager
    ProgramManager programManager(apvts);

    // Select program
    int program = programManager.getNumPrograms() > 1 ? 1 : 0;
    programManager.setCurrentProgram(program);

    // Set parameter value
    apvts.getParameter(ParameterIds::waveType)
        ->setValueNotifyingHost(0.5f);  // Change waveform type

    // Save state
    juce::MemoryBlock savedState;
    programManager.getStateInformation(savedState);

    // Verify state has valid size
    EXPECT_GT(savedState.getSize(), 0) << "Saved state should have data";

    // Change parameter value
    apvts.getParameter(ParameterIds::waveType)->setValueNotifyingHost(0.0f);

    // Restore state
    programManager.setStateInformation(savedState.getData(), (int)savedState.getSize());

    // Verify program number is restored
    EXPECT_EQ(programManager.getCurrentProgram(), program) << "Current program should be restored";

    // Verify parameter value is restored
    float restoredValue = apvts.getParameter(ParameterIds::waveType)->getValue();
    EXPECT_NEAR(restoredValue, 0.5f, 0.01f) << "Parameter value should be restored";
}

TEST_F(ProgramManagerTest, ExcludedParameters) {
    // Create mock AudioProcessor and parameters
    TestAudioProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters",
                                             createTestParameterLayout());

    // Instantiate ProgramManager
    ProgramManager programManager(apvts);

    // Set initial values for excluded parameters
    apvts.getParameter(ParameterIds::volume)->setValueNotifyingHost(0.8f);
    apvts.getParameter(ParameterIds::breathInput)->setValueNotifyingHost(0.3f);
    apvts.getParameter(ParameterIds::pitchBend)->setValueNotifyingHost(0.2f);

    // Set value for normal parameter
    apvts.getParameter(ParameterIds::waveType)->setValueNotifyingHost(0.5f);

    // Save state
    juce::MemoryBlock savedState;
    programManager.getStateInformation(savedState);

    // Change parameter values
    apvts.getParameter(ParameterIds::waveType)->setValueNotifyingHost(0.0f);
    apvts.getParameter(ParameterIds::volume)->setValueNotifyingHost(0.1f);
    apvts.getParameter(ParameterIds::breathInput)->setValueNotifyingHost(0.1f);
    apvts.getParameter(ParameterIds::pitchBend)->setValueNotifyingHost(0.1f);

    // Restore state
    programManager.setStateInformation(savedState.getData(), (int)savedState.getSize());

    // Verify normal parameter is restored
    float restoredNormalParam = apvts.getParameter(ParameterIds::waveType)->getValue();
    EXPECT_NEAR(restoredNormalParam, 0.5f, 0.01f) << "Normal parameter should be restored";

    // Verify session excluded parameters are not restored (should remain at 0.1f)
    // Volume is now included in DAW session state, so it should be restored to 0.8f
    float restoredVolume = apvts.getParameter(ParameterIds::volume)->getValue();
    float restoredBreath = apvts.getParameter(ParameterIds::breathInput)->getValue();
    float restoredPitchBend = apvts.getParameter(ParameterIds::pitchBend)->getValue();

    EXPECT_NEAR(restoredVolume, 0.8f, 0.01f) << "Volume should be restored in DAW session state";
    EXPECT_NEAR(restoredBreath, 0.1f, 0.01f) << "Breath input should not be restored";
    EXPECT_NEAR(restoredPitchBend, 0.1f, 0.01f) << "Pitch bend should not be restored";
}
