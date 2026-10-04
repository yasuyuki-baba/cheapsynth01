#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "UI/AudioDisplayFifo.h"
#include "ProgramManager.h"
#include "CS01Synth/IFilter.h"
#include "CS01Synth/VCOProcessor.h"
#include "CS01Synth/MidiProcessor.h"
#include "CS01Synth/EGProcessor.h"
#include "CS01Synth/LFOProcessor.h"
#include "CS01Synth/VCAProcessor.h"
#include "CS01Synth/OriginalVCFProcessor.h"
#include "CS01Synth/ModernVCFProcessor.h"

class CS01AudioProcessor : public juce::AudioProcessor,
                           public juce::AudioProcessorValueTreeState::Listener,
                           private juce::AsyncUpdater {
   public:
    // Get current filter processor
    IFilter* getCurrentFilterProcessor();
    //==============================================================================
    CS01AudioProcessor();
    ~CS01AudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override {
        return "CheapSynth01";
    }
    bool acceptsMidi() const override {
        return true;
    }
    bool producesMidi() const override {
        return false;
    }
    bool isMidiEffect() const override {
        return false;
    }
    double getTailLengthSeconds() const override {
        return 0.0;
    }

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ProgramManager access
    ProgramManager& getPresetManager() {
        return presetManager;
    }

    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void processorLayoutsChanged() override;

   public:
    juce::AudioProcessorValueTreeState& getValueTreeState() {
        return apvts;
    }
    juce::MidiKeyboardState& getKeyboardState() {
        return keyboardState;
    }
    juce::MidiMessageCollector& getMidiMessageCollector() {
        return midiMessageCollector;
    }
    juce::MidiMessageCollector& getPanelBendCollector() {
        return panelBendCollector;
    }
    unsigned getExternalBendRevision() const {
        return externalBendRevision.load();
    }

    AudioDisplayFifo& getAudioDisplayFifo() {
        return audioDisplayFifo;
    }

    const juce::AudioProcessorGraph& getAudioGraphForTesting() const {
        return audioGraph;
    }
    juce::AudioProcessorGraph::NodeID getVcoNodeIdForTesting() const {
        return vcoNode->nodeID;
    }
    juce::AudioProcessorGraph::NodeID getLfoNodeIdForTesting() const {
        return lfoNode->nodeID;
    }
    juce::AudioProcessorGraph::NodeID getVcaNodeIdForTesting() const {
        return vcaNode->nodeID;
    }
    juce::AudioProcessorGraph::NodeID getOriginalFilterNodeIdForTesting() const {
        return vcfNode->nodeID;
    }
    juce::AudioProcessorGraph::NodeID getModernFilterNodeIdForTesting() const {
        return modernVcfNode->nodeID;
    }
    // Call only on the message thread; flush both routing and graph rendering updates.
    void flushPendingGraphChangesForTesting() {
        handleUpdateNowIfNeeded();
        audioGraph.rebuild();
    }

    juce::AudioProcessorValueTreeState apvts;

   private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyFilterRouting(int filterType, int lfoTarget,
                            juce::AudioProcessorGraph::UpdateKind updateKind);
    void handleAsyncUpdate() override;
    void updateVCAOutputConnections();
    void handleGeneratorTypeChanged();
    juce::MidiKeyboardState keyboardState;
    juce::MidiMessageCollector midiMessageCollector;
    juce::MidiMessageCollector panelBendCollector;
    std::atomic<unsigned> externalBendRevision{0};
    juce::AudioProcessorGraph audioGraph;
    std::unique_ptr<juce::dsp::Oversampling<float>> outputOversampling;
    juce::AudioBuffer<float> internalAudio;
    int processingCapacity = 1;
    AudioDisplayFifo audioDisplayFifo;
    juce::AudioProcessorGraph::Node::Ptr midiInputNode;
    juce::AudioProcessorGraph::Node::Ptr midiProcessorNode;
    juce::AudioProcessorGraph::Node::Ptr audioOutputNode;
    juce::AudioProcessorGraph::Node::Ptr vcoNode;
    juce::AudioProcessorGraph::Node::Ptr egNode;
    juce::AudioProcessorGraph::Node::Ptr lfoNode;
    juce::AudioProcessorGraph::Node::Ptr vcaNode;
    juce::AudioProcessorGraph::Node::Ptr vcfNode;
    juce::AudioProcessorGraph::Node::Ptr modernVcfNode;

    // プログラム管理
    ProgramManager presetManager;

    // Latest requested routing state; parameter callbacks only publish values here.
    std::atomic<int> requestedFilterType{0};
    std::atomic<int> requestedLfoTarget{0};
    std::atomic<bool> pendingRoutingChange{false};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CS01AudioProcessor)
};
