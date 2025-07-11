#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "../../Source/ProgramManager.h"
#include "../../Source/Parameters.h"

// Test fixture for ProgramManager tests
class ProgramManagerTest : public ::testing::Test
{
protected:
    // Simple AudioProcessor implementation for testing
    class TestAudioProcessor : public juce::AudioProcessor
    {
    public:
        TestAudioProcessor()
            : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo()))
        {
            // Minimal initialization
        }

        // Minimal implementation of AudioProcessor base methods
        const juce::String getName() const override { return "TestProcessor"; }
        void prepareToPlay(double, int) override {}
        void releaseResources() override {}
        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String&) override {}
        void getStateInformation(juce::MemoryBlock&) override {}
        void setStateInformation(const void*, int) override {}
    };
    
    // Create test parameter layout
    juce::AudioProcessorValueTreeState::ParameterLayout createTestParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        
        // Add minimal set of parameters for testing
        auto vcoGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
            "vco", "VCO", "|",
            std::make_unique<juce::AudioParameterChoice>(
                ParameterIds::waveType, "Wave Type", 
                juce::StringArray{"Triangle", "Sawtooth", "Square", "Pulse", "PWM"}, 1),
            std::make_unique<juce::AudioParameterChoice>(
                ParameterIds::feet, "Feet", 
                juce::StringArray{"32'", "16'", "8'", "4'", "WN"}, 2)
        );
        layout.add(std::move(vcoGroup));
        
        auto modGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
            "mod", "Modulation", "|",
            std::make_unique<juce::AudioParameterFloat>(
                ParameterIds::pitchBend, "Pitch Bend", 
                juce::NormalisableRange<float>(0.0f, 12.0f), 0.0f)
        );
        layout.add(std::move(modGroup));
        
        auto globalGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
            "global", "Global", "|",
            std::make_unique<juce::AudioParameterFloat>(
                ParameterIds::volume, "Volume", 
                juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f),
            std::make_unique<juce::AudioParameterFloat>(
                ParameterIds::breathInput, "Breath Input", 
                juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f)
        );
        layout.add(std::move(globalGroup));
        
        return layout;
    }

    void SetUp() override
    {
        // Test setup if needed
    }
    
    void TearDown() override
    {
        // Test cleanup if needed
    }
};

TEST_F(ProgramManagerTest, FactoryPresets)
{
    // Create mock AudioProcessor and parameters
    TestAudioProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", createTestParameterLayout());
    
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

TEST_F(ProgramManagerTest, ProgramSelection)
{
    // Create mock AudioProcessor and parameters
    TestAudioProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", createTestParameterLayout());
    
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
        EXPECT_TRUE(name1 != name2 || name1.isEmpty()) << "Different presets should have different names";
    }
}

TEST_F(ProgramManagerTest, StateManagement)
{
    // Create mock AudioProcessor and parameters
    TestAudioProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", createTestParameterLayout());
    
    // Instantiate ProgramManager
    ProgramManager programManager(apvts);
    
    // Select program
    int program = programManager.getNumPrograms() > 1 ? 1 : 0;
    programManager.setCurrentProgram(program);
    
    // Set parameter value
    apvts.getParameter(ParameterIds::waveType)->setValueNotifyingHost(0.5f); // Change waveform type
    
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

TEST_F(ProgramManagerTest, ExcludedParameters)
{
    // Create mock AudioProcessor and parameters
    TestAudioProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", createTestParameterLayout());
    
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
