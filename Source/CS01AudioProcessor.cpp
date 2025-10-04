#include "CS01AudioProcessor.h"
#include "CS01AudioProcessorEditor.h"
#include "Parameters.h"
#include "CS01Synth/VCOProcessor.h"
#include "CS01Synth/MidiProcessor.h"
#include "CS01Synth/IFilter.h"  // Explicit include

//==============================================================================
CS01AudioProcessor::CS01AudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()),
      presetManager(apvts) {
    apvts.addParameterListener(ParameterIds::lfoTarget, this);
    apvts.addParameterListener(ParameterIds::filterType, this);
    apvts.addParameterListener(ParameterIds::feet, this);
}

CS01AudioProcessor::~CS01AudioProcessor() {
    apvts.removeParameterListener(ParameterIds::lfoTarget, this);
    apvts.removeParameterListener(ParameterIds::filterType, this);
    apvts.removeParameterListener(ParameterIds::feet, this);
}

//==============================================================================
void CS01AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    midiMessageCollector.reset(sampleRate);

    audioGraph.clear();

    // 1. Add nodes
    midiInputNode =
        audioGraph.addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
            juce::AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode));
    audioOutputNode =
        audioGraph.addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
            juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    midiProcessorNode = audioGraph.addNode(std::make_unique<MidiProcessor>(apvts));
    vcoNode =
        audioGraph.addNode(std::make_unique<VCOProcessor>(apvts));  // Default is ToneGenerator
    egNode = audioGraph.addNode(std::make_unique<EGProcessor>(apvts));
    lfoNode = audioGraph.addNode(std::make_unique<LFOProcessor>(apvts));
    vcaNode = audioGraph.addNode(std::make_unique<VCAProcessor>(apvts));
    vcfNode = audioGraph.addNode(std::make_unique<OriginalVCFProcessor>(apvts));
    modernVcfNode = audioGraph.addNode(std::make_unique<ModernVCFProcessor>(apvts));

    // 2. Set bus layouts
    audioOutputNode->getProcessor()->enableAllBuses();
    // midiProcessorNode has no audio buses
    vcoNode->getProcessor()->enableAllBuses();
    egNode->getProcessor()->enableAllBuses();
    lfoNode->getProcessor()->enableAllBuses();
    vcaNode->getProcessor()->enableAllBuses();
    vcfNode->getProcessor()->enableAllBuses();
    modernVcfNode->getProcessor()->enableAllBuses();

    // 3. Connect nodes
    // Configure connections based on filter type
    auto filterType =
        static_cast<int>(apvts.getRawParameterValue(ParameterIds::filterType)->load());

    // Audio Path: vco -> vcf -> vca -> output
    // Connect only the mono channel (ch = 0)
    if (filterType == 0)  // Original
    {
        audioGraph.addConnection({{vcoNode->nodeID, 0}, {vcfNode->nodeID, 0}});
        audioGraph.addConnection({{vcfNode->nodeID, 0}, {vcaNode->nodeID, 0}});
    } else  // Modern
    {
        audioGraph.addConnection({{vcoNode->nodeID, 0}, {modernVcfNode->nodeID, 0}});
        audioGraph.addConnection({{modernVcfNode->nodeID, 0}, {vcaNode->nodeID, 0}});
    }
    // Connection from VCA to audioOutputNode (automatically configured based on output bus layout)
    updateVCAOutputConnections();

    // Sidechain Paths
    // EG -> VCA (Sidechain)
    audioGraph.addConnection({{egNode->nodeID, 0}, {vcaNode->nodeID, 1}});
    // EG -> VCF (Sidechain)
    audioGraph.addConnection({{egNode->nodeID, 0}, {vcfNode->nodeID, 1}});
    // EG -> ModernVCF (Sidechain)
    audioGraph.addConnection({{egNode->nodeID, 0}, {modernVcfNode->nodeID, 1}});

    // LFO Path is connected dynamically via parameterChanged listener

    // MIDI Path - Simplified: only midiInput -> midiProcessor
    // No other MIDI connections needed as MidiProcessor directly controls ToneGenerator and
    // EGProcessor
    audioGraph.addConnection(
        {{midiInputNode->nodeID, juce::AudioProcessorGraph::midiChannelIndex},
         {midiProcessorNode->nodeID, juce::AudioProcessorGraph::midiChannelIndex}});

    // Set references to SoundGenerator and EGProcessor in MidiProcessor
    auto* midiProcessor = static_cast<MidiProcessor*>(midiProcessorNode->getProcessor());
    auto* vcoProcessor = static_cast<VCOProcessor*>(vcoNode->getProcessor());
    auto* egProcessor = static_cast<EGProcessor*>(egNode->getProcessor());

    if (midiProcessor != nullptr && vcoProcessor != nullptr && egProcessor != nullptr) {
        // Set the sound generator
        midiProcessor->setSoundGenerator(vcoProcessor->getSoundGenerator());
        midiProcessor->setEGProcessor(egProcessor);

        // Set up VCO generator type change callback
        vcoProcessor->onGeneratorTypeChanged = [this]() { handleGeneratorTypeChanged(); };
    }
    // 4. Set graph's main bus layout and prepare
    audioGraph.setPlayConfigDetails(getMainBusNumInputChannels(), getMainBusNumOutputChannels(),
                                    sampleRate, samplesPerBlock);
    audioGraph.prepareToPlay(sampleRate, samplesPerBlock);

    // Set initial LFO routing
    parameterChanged(ParameterIds::lfoTarget,
                     apvts.getRawParameterValue(ParameterIds::lfoTarget)->load());
}

// Apply pending graph changes on the audio thread
void CS01AudioProcessor::applyPendingGraphChanges() {
    // Apply filter type change if requested
    if (pendingFilterTypeChange.exchange(false)) {
        int newType = requestedFilterType.load();

        // If nodes are not ready yet, re-request for next block
        if (vcoNode == nullptr || vcfNode == nullptr || modernVcfNode == nullptr || vcaNode == nullptr) {
            requestedFilterType.store(newType);
            pendingFilterTypeChange.store(true);
        } else {
            // Remove existing connections (safe to call even if absent)
            audioGraph.removeConnection({{vcoNode->nodeID, 0}, {vcfNode->nodeID, 0}});
            audioGraph.removeConnection({{vcfNode->nodeID, 0}, {vcaNode->nodeID, 0}});
            audioGraph.removeConnection({{vcoNode->nodeID, 0}, {modernVcfNode->nodeID, 0}});
            audioGraph.removeConnection({{modernVcfNode->nodeID, 0}, {vcaNode->nodeID, 0}});

            if (newType == 0) {
                audioGraph.addConnection({{vcoNode->nodeID, 0}, {vcfNode->nodeID, 0}});
                audioGraph.addConnection({{vcfNode->nodeID, 0}, {vcaNode->nodeID, 0}});
            } else {
                audioGraph.addConnection({{vcoNode->nodeID, 0}, {modernVcfNode->nodeID, 0}});
                audioGraph.addConnection({{modernVcfNode->nodeID, 0}, {vcaNode->nodeID, 0}});
            }

            // Notify UI on message thread
            juce::MessageManager::callAsync([this]() {
                if (auto* editor = dynamic_cast<CS01AudioProcessorEditor*>(getActiveEditor())) {
                    editor->filterTypeChanged(getCurrentFilterProcessor());
                }
            });
        }
    }

    // Apply LFO target change if requested
    if (pendingLfoTargetChange.exchange(false)) {
        int newTarget = requestedLfoTarget.load();

        if (lfoNode == nullptr || vcfNode == nullptr || modernVcfNode == nullptr || vcoNode == nullptr) {
            requestedLfoTarget.store(newTarget);
            pendingLfoTargetChange.store(true);
        } else {
            // Disconnect existing LFO connections
            audioGraph.removeConnection({{lfoNode->nodeID, 0}, {vcfNode->nodeID, 2}});
            audioGraph.removeConnection({{lfoNode->nodeID, 0}, {modernVcfNode->nodeID, 2}});
            audioGraph.removeConnection({{lfoNode->nodeID, 0}, {vcoNode->nodeID, 0}});

            if (newTarget == 0) {
                audioGraph.addConnection({{lfoNode->nodeID, 0}, {vcoNode->nodeID, 0}});
            } else {
                int filterType = static_cast<int>(apvts.getRawParameterValue(ParameterIds::filterType)->load());
                if (filterType == 0) {
                    audioGraph.addConnection({{lfoNode->nodeID, 0}, {vcfNode->nodeID, 2}});
                } else {
                    audioGraph.addConnection({{lfoNode->nodeID, 0}, {modernVcfNode->nodeID, 2}});
                }
            }
        }
    }
}

void CS01AudioProcessor::releaseResources() {
    audioGraph.releaseResources();
}

bool CS01AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    return true;
}

void CS01AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    // Apply pending graph changes requested from other threads (atomic flags)
    applyPendingGraphChanges();
    midiMessageCollector.removeNextBlockOfMessages(midiMessages, buffer.getNumSamples());

    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
    audioGraph.processBlock(buffer, midiMessages);

    if (auto* editor = dynamic_cast<CS01AudioProcessorEditor*>(getActiveEditor())) {
        editor->getOscilloscope().pushBuffer(buffer);
        editor->getAudioVisualiser().pushBuffer(buffer);
    }
}

//==============================================================================
//==============================================================================
int CS01AudioProcessor::getNumPrograms() {
    return presetManager.getNumPrograms();
}

int CS01AudioProcessor::getCurrentProgram() {
    return presetManager.getCurrentProgram();
}

void CS01AudioProcessor::setCurrentProgram(int index) {
    presetManager.setCurrentProgram(index);
}

const juce::String CS01AudioProcessor::getProgramName(int index) {
    return presetManager.getProgramName(index);
}

void CS01AudioProcessor::changeProgramName(int index, const juce::String& newName) {}

//==============================================================================
void CS01AudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    presetManager.getStateInformation(destData);
}

void CS01AudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    presetManager.setStateInformation(data, sizeInBytes);
}

//==============================================================================
juce::AudioProcessorEditor* CS01AudioProcessor::createEditor() {
    return new CS01AudioProcessorEditor(*this);
}
bool CS01AudioProcessor::hasEditor() const {
    return true;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout CS01AudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto vcoGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "vco", "VCO", "|",
        std::make_unique<juce::AudioParameterChoice>(
            ParameterIds::waveType, "Wave Type",
            juce::StringArray{"Triangle", "Sawtooth", "Square", "Pulse", "PWM"}, 1),
        std::make_unique<juce::AudioParameterChoice>(
            ParameterIds::feet, "Feet", juce::StringArray{"32'", "16'", "8'", "4'", "WN"}, 2),
        std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::pwmSpeed, "PWM Speed",
            juce::NormalisableRange<float>(0.0f, 60.0f, 0.01f, 0.25f), 2.0f),
        std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::pitch, "Pitch", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f),
            0.0f),
        std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::glissando, "Glissando",
            juce::NormalisableRange<float>(0.0f, Constants::maxGlissandoPerSemitoneSeconds, 0.001f,
                                           0.5f),
            0.0f));
    layout.add(std::move(vcoGroup));

    auto vcfGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "vcf", "VCF", "|",
        std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::cutoff, "Cutoff",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 20000.0f),
        std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::resonance, "Resonance", juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::vcfEgDepth, "VCF EG Depth",
                                                    juce::NormalisableRange<float>(0.0f, 1.0f),
                                                    0.0f));
    layout.add(std::move(vcfGroup));

    auto vcaGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "vca", "VCA", "|",
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::vcaEgDepth, "VCA EG Depth",
                                                    juce::NormalisableRange<float>(0.0f, 1.0f),
                                                    1.0f));
    layout.add(std::move(vcaGroup));

    auto egGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "eg", "EG", "|",
        // Attack time: Range based on A2M potentiometer (0-2MΩ) with exponential curve
        std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::attack, "Attack",
            juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.3f), 0.1f),
        // Decay time: Range based on A2M potentiometer (0-2MΩ) with exponential curve
        std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::decay, "Decay",
            juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.3f), 0.1f),
        // Sustain level: Range based on B1M potentiometer (0-1MΩ) with linear response
        std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::sustain, "Sustain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f),
        // Release time: Range based on A2M potentiometer (0-2MΩ) with exponential curve
        std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::release, "Release",
            juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.3f), 0.1f));
    layout.add(std::move(egGroup));

    auto lfoGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "lfo", "LFO", "|",
        std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::lfoSpeed, "LFO Speed",
            juce::NormalisableRange<float>(0.0f, 21.0f, 0.01f, 0.3f), 5.0f),
        std::make_unique<juce::AudioParameterChoice>(ParameterIds::lfoTarget, "LFO Target",
                                                     juce::StringArray{"VCO", "VCF"}, 0),
        std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::modDepth, "Mod Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withAutomatable(false)));
    layout.add(std::move(lfoGroup));

    auto modGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "mod", "Modulation", "|",
        std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::pitchBend, "Pitch Bend", juce::NormalisableRange<float>(0.0f, 12.0f),
            0.0f, juce::AudioParameterFloatAttributes().withAutomatable(false)),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::breathVcf, "Breath VCF",
                                                    juce::NormalisableRange<float>(0.0f, 1.0f),
                                                    0.0f),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::breathVca, "Breath VCA",
                                                    juce::NormalisableRange<float>(0.0f, 1.0f),
                                                    0.0f),
        std::make_unique<juce::AudioParameterInt>(ParameterIds::pitchBendUpRange, "Pitch Bend Up",
                                                  0, 12, 12),
        std::make_unique<juce::AudioParameterInt>(ParameterIds::pitchBendDownRange,
                                                  "Pitch Bend Down", 0, 12, 12));
    layout.add(std::move(modGroup));

    auto globalGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "global", "Global", "|",
        std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::volume, "Volume", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::breathInput, "Breath Input",
                                                    juce::NormalisableRange<float>(0.0f, 1.0f),
                                                    0.0f),
        std::make_unique<juce::AudioParameterChoice>(ParameterIds::filterType, "Filter Type",
                                                     juce::StringArray{"Original", "Modern"}, 0));
    layout.add(std::move(globalGroup));

    return layout;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new CS01AudioProcessor();
}

void CS01AudioProcessor::updateVCAOutputConnections() {
    // Check if audio graph nodes are initialized
    if (vcaNode == nullptr || audioOutputNode == nullptr) {
        return;
    }

    // Remove existing connections
    audioGraph.removeConnection({{vcaNode->nodeID, 0}, {audioOutputNode->nodeID, 0}});
    audioGraph.removeConnection({{vcaNode->nodeID, 0}, {audioOutputNode->nodeID, 1}});

    // Get current output bus configuration
    auto outputLayout = getBusesLayout().getMainOutputChannelSet();

    // Assuming CS01VCAProcessor always outputs mono
    if (outputLayout == juce::AudioChannelSet::stereo()) {
        // For stereo output, duplicate mono signal to both channels
        audioGraph.addConnection({{vcaNode->nodeID, 0}, {audioOutputNode->nodeID, 0}});
        audioGraph.addConnection({{vcaNode->nodeID, 0}, {audioOutputNode->nodeID, 1}});
    } else  // For mono output
    {
        // For mono output, connect directly
        audioGraph.addConnection({{vcaNode->nodeID, 0}, {audioOutputNode->nodeID, 0}});
    }
}

void CS01AudioProcessor::processorLayoutsChanged() {
    // Call parent class processing
    AudioProcessor::processorLayoutsChanged();

    // Update connections if bus configuration has changed
    if (audioOutputNode != nullptr && vcaNode != nullptr) {
        updateVCAOutputConnections();
    }
}

// Handler for VCOProcessor's generator type changes
void CS01AudioProcessor::handleGeneratorTypeChanged() {
    // Check if audio graph nodes are initialized
    if (midiProcessorNode == nullptr || vcoNode == nullptr) {
        return;
    }

    // Update the MidiProcessor's sound generator reference
    auto* midiProcessor = static_cast<MidiProcessor*>(midiProcessorNode->getProcessor());
    auto* vcoProcessor = static_cast<VCOProcessor*>(vcoNode->getProcessor());

    if (midiProcessor != nullptr && vcoProcessor != nullptr) {
        midiProcessor->setSoundGenerator(vcoProcessor->getSoundGenerator());
    }
}

// Get current filter processor
IFilter* CS01AudioProcessor::getCurrentFilterProcessor() {
    auto filterType =
        static_cast<int>(apvts.getRawParameterValue(ParameterIds::filterType)->load());

    if (filterType == 0)  // Original
    {
        if (vcfNode != nullptr && vcfNode->getProcessor() != nullptr) {
            return dynamic_cast<IFilter*>(vcfNode->getProcessor());
        }
    } else  // Modern
    {
        if (modernVcfNode != nullptr && modernVcfNode->getProcessor() != nullptr) {
            return dynamic_cast<IFilter*>(modernVcfNode->getProcessor());
        }
    }

    return nullptr;
}

void CS01AudioProcessor::parameterChanged(const juce::String& parameterID, float newValue) {
    if (parameterID == ParameterIds::feet) {
        // VCOProcessor now handles the generator type change internally
        // and notifies us via the callback we set up
    } else if (parameterID == ParameterIds::lfoTarget) {
        // Record request and apply on audio thread
        requestedLfoTarget.store(static_cast<int>(newValue));
        pendingLfoTargetChange.store(true);
        return;
    }
    } else if (parameterID == ParameterIds::filterType) {
        // Record request and apply on audio thread
        requestedFilterType.store(static_cast<int>(newValue));
        pendingFilterTypeChange.store(true);
        return;
    }
}
