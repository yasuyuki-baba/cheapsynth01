#pragma once

#include <JuceHeader.h>

#include "ProgramManager.h"
#include "RealtimeMidiQueue.h"
#include "UI/AudioDisplayFifo.h"

#include <atomic>
#include <memory>

class IFilter;

class CS01AudioProcessor : public juce::AudioProcessor,
                           public juce::AudioProcessorValueTreeState::Listener,
                           private juce::Timer,
                           private juce::MidiKeyboardStateListener {
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
    // Host requests, on any thread, are applied at the next processBlock.
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
    RealtimeMidiQueue& getMidiMessageCollector() {
        return midiMessageCollector;
    }
    RealtimeMidiQueue& getPanelBendCollector() {
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
#if defined(CHEAPSYNTH_ROUTING_REFERENCE)
    // Non-RT test references only; set before prepareToPlay.
    void useLegacyRoutingForTesting() {
        legacyRoutingForTesting = true;
    }
    void useDualFilterInputForTesting(bool enabled) {
        dualFilterInputForTesting = enabled;
    }
#endif
    // Call only on the message thread; flush reference routing and graph rendering updates.
    void flushPendingGraphChangesForTesting() {
        timerCallback();
        audioGraph.rebuild();
    }

    int getAppliedFilterTypeForTesting() const {
        return requestedFilterType.load();
    }
    int getAppliedLfoTargetForTesting() const {
        return requestedLfoTarget.load();
    }

    juce::AudioProcessorValueTreeState apvts;

   private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyFilterRouting(int filterType, int lfoTarget,
                            juce::AudioProcessorGraph::UpdateKind updateKind);
    void applyAudioRouting();
    bool usesLegacyRouting() const {
#if defined(CHEAPSYNTH_ROUTING_REFERENCE)
        return legacyRoutingForTesting;
#else
        return false;
#endif
    }
    void timerCallback() override;
    void updateVCAOutputConnections();
    void handleGeneratorTypeChanged();
    juce::MidiKeyboardState keyboardState;
    RealtimeMidiQueue midiMessageCollector;
    RealtimeMidiQueue panelBendCollector;
    juce::MidiBuffer panelBendMidi, queuedMidi, keyboardMirrorMidi;
    RealtimeMidiQueue keyboardMirror;
    std::atomic<bool> mirroringKeyboard{false};
    void handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
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

    juce::AudioParameterChoice* filterChoice = nullptr;
    juce::AudioParameterChoice* lfoChoice = nullptr;
#if defined(CHEAPSYNTH_ROUTING_REFERENCE)
    bool legacyRoutingForTesting = false, dualFilterInputForTesting = false;
#endif
    // Applied audio snapshot; reference mode retains the old message-thread snapshot.
    std::atomic<int> requestedFilterType{0};
    std::atomic<int> requestedLfoTarget{0};
    std::atomic<bool> pendingRoutingChange{false};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CS01AudioProcessor)
};
