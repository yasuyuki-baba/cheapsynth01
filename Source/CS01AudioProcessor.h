#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include "CS01Synth/VCOProcessor.h"
#include "CS01Synth/MidiProcessor.h"
#include "CS01Synth/EGProcessor.h"
#include "CS01Synth/LFOProcessor.h"
#include "CS01Synth/VCAProcessor.h"
#include "CS01Synth/CS01VCFProcessor.h"
#include "CS01Synth/ModernVCFProcessor.h"
#include "CS01Synth/NoiseProcessor.h"

//==============================================================================
struct Preset
{
    juce::String name;
    juce::String filename;
};

class CS01AudioProcessor  : public juce::AudioProcessor,
                            public juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    CS01AudioProcessor();
    ~CS01AudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override { return "CS01 Emulator"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void processorLayoutsChanged() override;
    
public:
    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    juce::MidiKeyboardState& getKeyboardState() { return keyboardState; }
    juce::MidiMessageCollector& getMidiMessageCollector() { return midiMessageCollector; }
    
    juce::AudioProcessorValueTreeState apvts;

private:
    void initializePresets();
    void loadPresetFromBinaryData(const juce::String& filename);
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateVCAOutputConnections();

    juce::MidiKeyboardState keyboardState;
    juce::MidiMessageCollector midiMessageCollector;
    juce::AudioProcessorGraph audioGraph;
    juce::AudioProcessorGraph::Node::Ptr midiInputNode;
    juce::AudioProcessorGraph::Node::Ptr midiProcessorNode;
    juce::AudioProcessorGraph::Node::Ptr audioOutputNode;
    juce::AudioProcessorGraph::Node::Ptr vcoNode;
    juce::AudioProcessorGraph::Node::Ptr egNode;
    juce::AudioProcessorGraph::Node::Ptr lfoNode;
    juce::AudioProcessorGraph::Node::Ptr vcaNode;
    juce::AudioProcessorGraph::Node::Ptr vcfNode;
    juce::AudioProcessorGraph::Node::Ptr modernVcfNode;
    juce::AudioProcessorGraph::Node::Ptr noiseNode;
    std::vector<Preset> factoryPresets;
    int currentProgram = 0;
    
    // List of parameters that retain their values during preset changes or state save/restore
    const std::vector<juce::String> stateExcludedParameters = {
        ParameterIds::breathInput,
        ParameterIds::volume,
        ParameterIds::modDepth,
        ParameterIds::pitchBend
    };
    
    // Helper method to check if a parameter is excluded from state changes
    bool isStateExcludedParameter(const juce::String& paramId) const
    {
        return std::find(stateExcludedParameters.begin(), 
                         stateExcludedParameters.end(), 
                         paramId) != stateExcludedParameters.end();
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CS01AudioProcessor)
};
