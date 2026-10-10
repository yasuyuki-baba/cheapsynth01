#include <JuceHeader.h>

#include "BinaryData.h"
#include "CS01AudioProcessor.h"
#include "Parameters.h"
#include "ProgramManager.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>

TEST(ParameterVersionTest, ProductionParametersHaveStableVersionHints) {
    CS01AudioProcessor processor;
    ASSERT_EQ(processor.getParameters().size(), 24);
    for (auto* parameter : processor.getParameters()) {
        EXPECT_EQ(parameter->getVersionHint(), 1);
    }
}

TEST(ProductionStateTest, EveryFactoryPresetLoadsEmbeddedSoundAndPreservesLiveControls) {
    CS01AudioProcessor host;
    auto& state = host.getValueTreeState();
    auto& manager = host.getPresetManager();
    const std::vector<std::pair<juce::String, float>> liveControls{
        {ParameterIds::volume, 0.3f},
        {ParameterIds::breathInput, 0.2f},
        {ParameterIds::pitchBend, -0.5f},
        {ParameterIds::modDepth, 0.4f}};
    int factoryCount = 0;
    for (int index = 0; index < manager.getNumPrograms(); ++index) {
        if (manager.isUserPreset(index))
            continue;
        ++factoryCount;
        SCOPED_TRACE(manager.getProgramName(index).toStdString());
        int size = 0;
        const auto resource = manager.getProgramFilename(index).replace(".", "_");
        const auto* data = BinaryData::getNamedResource(resource.toRawUTF8(), size);
        ASSERT_NE(data, nullptr);
        ASSERT_GT(size, 0);
        auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(data, size));
        ASSERT_NE(xml, nullptr);
        ASSERT_TRUE(xml->hasTagName(state.state.getType()));
        ASSERT_EQ(xml->getNumChildElements(), 18);
        // Perturb each sound control to prove selection actually reloads it.
        for (auto* child : xml->getChildIterator()) {
            auto* parameter = state.getParameter(child->getStringAttribute("id"));
            ASSERT_NE(parameter, nullptr);
            const auto expected = static_cast<float>(child->getDoubleAttribute("value"));
            const auto normalized = parameter->convertTo0to1(expected);
            parameter->setValueNotifyingHost(normalized < 0.5f ? 1.0f : 0.0f);
        }
        for (const auto& [id, value] : liveControls) {
            auto* parameter = state.getParameter(id);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        }
        manager.setCurrentProgram(index);
        for (auto* child : xml->getChildIterator()) {
            const auto id = child->getStringAttribute("id");
            SCOPED_TRACE(id.toStdString());
            auto* parameter = state.getParameter(id);
            const auto expected = static_cast<float>(child->getDoubleAttribute("value"));
            EXPECT_NEAR(parameter->convertFrom0to1(parameter->getValue()), expected, 0.001f);
        }
        for (const auto& [id, value] : liveControls)
            EXPECT_NEAR(state.getRawParameterValue(id)->load(), value, 0.001f);
    }
    EXPECT_EQ(factoryCount, 7);
}

TEST(ProductionStateTest, ManualFactoryPresetsMatchDocumentedPanelReadings) {
    CS01AudioProcessor host;
    auto& state = host.getValueTreeState();
    auto& manager = host.getPresetManager();
    // Independent visual readings from printed pages 28-33. See Factory-presets.md.
    struct Reading {
        const char* filename;
        int wave, feet, resonance, target;
        std::array<float, 11> positions;
    };
    const std::array<juce::String, 11> ids{
        ParameterIds::cutoff,    ParameterIds::vcfEgDepth, ParameterIds::vcaEgDepth,
        ParameterIds::attack,    ParameterIds::decay,      ParameterIds::sustain,
        ParameterIds::release,   ParameterIds::lfoSpeed,   ParameterIds::pwmSpeed,
        ParameterIds::breathVcf, ParameterIds::breathVca};
    const Reading readings[]{
        {"Flute.xml", 0, 2, 0, 0, {.2f, .4f, 1, .2f, 0, 1, 0, 0, 0, .7f, .7f}},
        {"Violin.xml", 1, 3, 0, 0, {.75f, .45f, 1, .1f, .1f, 1, 0, .75f, 0, .7f, 0}},
        {"Trumpet.xml", 1, 2, 0, 0, {.35f, .5f, 1, .1f, .2f, .5f, 0, 0, 0, 0, 0}},
        {"Clavinet.xml", 3, 1, 1, 1, {.6f, .5f, 1, 0, .2f, 0, .65f, .75f, 0, .3f, .3f}},
        {"Solo_Synth_Lead.xml", 2, 3, 0, 0, {.35f, 1, 1, 0, .1f, .3f, .3f, .55f, .6f, 0, 0}},
        {"Synth_Bass.xml", 3, 0, 1, 1, {.4f, .25f, 1, 0, .3f, 0, .5f, .1f, 0, 0, 0}}};
    for (const auto& reading : readings) {
        SCOPED_TRACE(reading.filename);
        const auto index = manager.findProgram(reading.filename, PresetType::Factory);
        ASSERT_GE(index, 0);
        manager.setCurrentProgram(index);
        EXPECT_FLOAT_EQ(state.getRawParameterValue(ParameterIds::waveType)->load(), reading.wave);
        EXPECT_FLOAT_EQ(state.getRawParameterValue(ParameterIds::feet)->load(), reading.feet);
        EXPECT_FLOAT_EQ(state.getRawParameterValue(ParameterIds::resonance)->load(),
                        reading.resonance);
        EXPECT_FLOAT_EQ(state.getRawParameterValue(ParameterIds::lfoTarget)->load(),
                        reading.target);
        EXPECT_FLOAT_EQ(state.getRawParameterValue(ParameterIds::filterType)->load(), 0);
        // Bipolar range conversion can leave a rounding residual at the centre on ARM.
        EXPECT_NEAR(state.getRawParameterValue(ParameterIds::pitch)->load(), 0.0f, 0.000001f);
        EXPECT_FLOAT_EQ(state.getRawParameterValue(ParameterIds::glissando)->load(), 0);
        for (size_t i = 0; i < ids.size(); ++i) {
            SCOPED_TRACE(ids[i].toStdString());
            // Low EG positions lose precision when rounded to 1 ms in the XML.
            EXPECT_NEAR(state.getParameter(ids[i])->getValue(), reading.positions[i], .02f);
        }
    }
}

TEST(ProductionStateTest, FactorySoundsRenderFiniteAudibleNotesWithoutBreathInput) {
    CS01AudioProcessor host;
    auto& manager = host.getPresetManager();
    for (int index = 0; index < manager.getNumPrograms(); ++index) {
        if (manager.isUserPreset(index))
            continue;
        SCOPED_TRACE(manager.getProgramName(index).toStdString());
        manager.setCurrentProgram(index);
        host.prepareToPlay(48000, 128);
        juce::AudioBuffer<float> buffer(2, 128);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
        float peak = 0;
        for (int block = 0; block < 100; ++block) {
            buffer.clear();
            host.processBlock(buffer, midi);
            midi.clear();
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
                    const auto value = buffer.getSample(channel, sample);
                    ASSERT_TRUE(std::isfinite(value));
                    peak = juce::jmax(peak, std::abs(value));
                }
            }
        }
        EXPECT_GT(peak, 0.00001f);
        midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
        host.processBlock(buffer, midi);
        host.releaseResources();
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
