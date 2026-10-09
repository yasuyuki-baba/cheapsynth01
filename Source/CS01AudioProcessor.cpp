#include "CS01AudioProcessor.h"

#include "CS01AudioProcessorEditor.h"
#include "CS01Synth/EGProcessor.h"
#include "CS01Synth/IFilter.h"  // Explicit include
#include "CS01Synth/LFOProcessor.h"
#include "CS01Synth/MidiProcessor.h"
#include "CS01Synth/ModernVCFProcessor.h"
#include "CS01Synth/OriginalVCFProcessor.h"
#include "CS01Synth/SynthConstants.h"
#include "CS01Synth/VCAProcessor.h"
#include "CS01Synth/VCOProcessor.h"
#include "MidiParameterValue.h"
#include "Parameters.h"
#include "ParameterFormatting.h"

//==============================================================================
CS01AudioProcessor::CS01AudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()),
      presetManager(apvts) {
    filterChoice =
        static_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::filterType));
    lfoChoice =
        static_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::lfoTarget));
    keyboardState.addListener(this);
    keyboardMirrorMidi.ensureSize(32768);
    startTimerHz(60);
}

CS01AudioProcessor::~CS01AudioProcessor() {
    stopTimer();
    keyboardState.removeListener(this);
}

//==============================================================================
void CS01AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    midiMessageCollector.reset(sampleRate);
    panelBendCollector.reset(sampleRate);
    panelBendMidi.ensureSize(32768);
    queuedMidi.ensureSize(65536);
    keyboardMirror.reset(sampleRate);
    processingCapacity = juce::jmax(1, samplesPerBlock);
    outputOversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        getMainBusNumOutputChannels(), Constants::oversamplingStages,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
    outputOversampling->initProcessing(processingCapacity);
    outputOversampling->reset();
    internalAudio.setSize(getMainBusNumOutputChannels(),
                          processingCapacity * Constants::oversamplingFactor);

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
    static_cast<VCOProcessor*>(vcoNode->getProcessor())->setExternalOversampling(true);
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

    // 3. Connect nodes. Dynamic audio and LFO routing is applied as one state below.
    // Connection from VCA to audioOutputNode (automatically configured based on output bus layout)
    updateVCAOutputConnections();

    if (!usesLegacyRouting()) {
        audioGraph.addConnection({{vcoNode->nodeID, 0}, {vcfNode->nodeID, 0}});
        audioGraph.addConnection({{vcoNode->nodeID, 0}, {modernVcfNode->nodeID, 0}});
        audioGraph.addConnection({{vcfNode->nodeID, 0}, {vcaNode->nodeID, 0}});
        audioGraph.addConnection({{modernVcfNode->nodeID, 0}, {vcaNode->nodeID, 0}});
        audioGraph.addConnection({{lfoNode->nodeID, 0}, {vcoNode->nodeID, 0}});
        audioGraph.addConnection({{lfoNode->nodeID, 0}, {vcfNode->nodeID, 2}});
        audioGraph.addConnection({{lfoNode->nodeID, 0}, {modernVcfNode->nodeID, 2}});
    }

    // Sidechain Paths
    // EG -> VCA (Sidechain)
    audioGraph.addConnection({{egNode->nodeID, 0}, {vcaNode->nodeID, 1}});
    // EG -> VCF (Sidechain)
    audioGraph.addConnection({{egNode->nodeID, 0}, {vcfNode->nodeID, 1}});
    // EG -> ModernVCF (Sidechain)
    audioGraph.addConnection({{egNode->nodeID, 0}, {modernVcfNode->nodeID, 1}});

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
                                    sampleRate * Constants::oversamplingFactor,
                                    processingCapacity * Constants::oversamplingFactor);
    audioGraph.prepareToPlay(sampleRate * Constants::oversamplingFactor,
                             processingCapacity * Constants::oversamplingFactor);

    requestedFilterType.store(
        static_cast<int>(apvts.getRawParameterValue(ParameterIds::filterType)->load()));
    requestedLfoTarget.store(
        static_cast<int>(apvts.getRawParameterValue(ParameterIds::lfoTarget)->load()));
    if (usesLegacyRouting())
        applyFilterRouting(requestedFilterType.load(), requestedLfoTarget.load(),
                           juce::AudioProcessorGraph::UpdateKind::sync);
    else
        applyAudioRouting();
}

void CS01AudioProcessor::applyAudioRouting() {
    if (vcoNode == nullptr || vcfNode == nullptr || modernVcfNode == nullptr)
        return;
    const int filter = filterChoice->getIndex();
    const int target = lfoChoice->getIndex();
    requestedFilterType.store(filter);
    requestedLfoTarget.store(target);
    bool bothInputs = false;
#if defined(CHEAPSYNTH_ROUTING_REFERENCE)
    bothInputs = dualFilterInputForTesting;
#endif
    static_cast<VCOProcessor*>(vcoNode->getProcessor())->setLfoRoutingEnabled(target == 0);
    static_cast<OriginalVCFProcessor*>(vcfNode->getProcessor())
        ->setRouting(filter == 0 || bothInputs, filter == 0, target == 1 && filter == 0);
    static_cast<ModernVCFProcessor*>(modernVcfNode->getProcessor())
        ->setRouting(filter == 1 || bothInputs, filter == 1, target == 1 && filter == 1);
}

void CS01AudioProcessor::applyFilterRouting(int filterType, int lfoTarget,
                                            juce::AudioProcessorGraph::UpdateKind updateKind) {
    if (vcoNode == nullptr || vcfNode == nullptr || modernVcfNode == nullptr ||
        vcaNode == nullptr || lfoNode == nullptr)
        return;

    const bool useModernFilter = filterType != 0;
    const bool targetVco = lfoTarget == 0;

    const auto updateConnection =
        [this, updateKind](const juce::AudioProcessorGraph::Connection& connection,
                           bool shouldExist) {
            const bool isConnected = audioGraph.isConnected(connection);
            if (shouldExist && !isConnected) {
                audioGraph.addConnection(connection, updateKind);
            } else if (!shouldExist && isConnected) {
                audioGraph.removeConnection(connection, updateKind);
            }
        };

    updateConnection({{vcoNode->nodeID, 0}, {vcfNode->nodeID, 0}}, !useModernFilter);
    updateConnection({{vcfNode->nodeID, 0}, {vcaNode->nodeID, 0}}, !useModernFilter);
    updateConnection({{vcoNode->nodeID, 0}, {modernVcfNode->nodeID, 0}}, useModernFilter);
    updateConnection({{modernVcfNode->nodeID, 0}, {vcaNode->nodeID, 0}}, useModernFilter);

    updateConnection({{lfoNode->nodeID, 0}, {vcoNode->nodeID, 0}}, targetVco);
    updateConnection({{lfoNode->nodeID, 0}, {vcfNode->nodeID, 2}}, !targetVco && !useModernFilter);
    updateConnection({{lfoNode->nodeID, 0}, {modernVcfNode->nodeID, 2}},
                     !targetVco && useModernFilter);
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
    presetManager.applyPendingProgram();
    // One authoritative routing snapshot per host callback, including empty blocks.
    if (!usesLegacyRouting())
        applyAudioRouting();
    queuedMidi.clear();
    panelBendMidi.clear();
    bool queuePanic = false;
    if (buffer.getNumSamples() > 0) {
        queuePanic =
            midiMessageCollector.removeNextBlockOfMessages(queuedMidi, buffer.getNumSamples());
        queuePanic =
            panelBendCollector.removeNextBlockOfMessages(panelBendMidi, buffer.getNumSamples()) ||
            queuePanic;
    }
    bool externalBend = false;
    for (const auto metadata : midiMessages)
        externalBend =
            externalBend || (metadata.numBytes == 3 && (metadata.data[0] & 0xf0) == 0xe0);
    if (externalBend)
        externalBendRevision.fetch_add(1);
    else
        queuedMidi.addEvents(panelBendMidi, 0, buffer.getNumSamples(), 0);
    if (queuePanic) {
        queuedMidi.clear();
        queuedMidi.addEvent(juce::MidiMessage::controllerEvent(1, 120, 0), 0);
    }
    // MidiProcessor applies events immediately. Render the graph in segments
    // so those events are applied at their actual host-sample positions.
    juce::MidiBuffer emptyMidi;
    int position = 0;
    const auto renderUntil = [&](int end) {
        if (end <= position)
            return;
        if (vcoNode != nullptr)
            static_cast<VCOProcessor*>(vcoNode->getProcessor())->applyPendingGeneratorChange();
        for (int offset = position; offset < end;) {
            const int length = juce::jmin(processingCapacity, end - offset);
            juce::AudioBuffer<float> host(buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                          offset, length);
            host.clear();
            juce::dsp::AudioBlock<float> hostBlock(host);
            auto high = outputOversampling->processSamplesUp(hostBlock);
            const int highLength = static_cast<int>(high.getNumSamples());
            juce::AudioBuffer<float> internal(internalAudio.getArrayOfWritePointers(),
                                              internalAudio.getNumChannels(), highLength);
            internal.clear();
            auto* envelope = static_cast<EGProcessor*>(egNode->getProcessor());
            const int remaining = envelope->getReleaseSamplesRemaining();
            if (remaining >= 0)
                static_cast<VCOProcessor*>(vcoNode->getProcessor())
                    ->getSoundGenerator()
                    ->setReleaseSamplesRemaining(remaining);
            audioGraph.processBlock(internal, emptyMidi);
            for (int channel = 0; channel < internal.getNumChannels(); ++channel)
                juce::FloatVectorOperations::copy(high.getChannelPointer(channel),
                                                  internal.getReadPointer(channel), highLength);
            outputOversampling->processSamplesDown(hostBlock);
            offset += length;
        }
        position = end;
    };
    // Merge non-owning host events with bounded GUI storage. Host events precede
    // queued GUI events at the same position, as in JUCE's collector insertion.
    auto hostEvent = midiMessages.cbegin();
    auto guiEvent = queuedMidi.cbegin();
    while (hostEvent != midiMessages.cend() || guiEvent != queuedMidi.cend()) {
        const bool useHost = guiEvent == queuedMidi.cend() ||
                             (hostEvent != midiMessages.cend() &&
                              (*hostEvent).samplePosition <= (*guiEvent).samplePosition);
        const auto metadata = useHost ? *hostEvent++ : *guiEvent++;
        const int eventPosition = juce::jlimit(0, buffer.getNumSamples(), metadata.samplePosition);
        renderUntil(eventPosition);
        // Out-of-range events clamp to the block boundary and are applied after
        // rendering, including zero-sample blocks. No event is silently dropped.
        // Skip unsupported long messages without copying their payload.
        if (metadata.numBytes <= 3 && midiProcessorNode != nullptr) {
            static_cast<VCOProcessor*>(vcoNode->getProcessor())->applyPendingGeneratorChange();
            const auto message = metadata.getMessage();
            if (message.isNoteOnOrOff() || message.isAllNotesOff() || message.isAllSoundOff())
                keyboardMirror.addMessageToQueue(message);
            static_cast<MidiProcessor*>(midiProcessorNode->getProcessor())
                ->processShortEvent(message);
            if (message.isAllSoundOff()) {
                vcfNode->getProcessor()->releaseResources();
                modernVcfNode->getProcessor()->releaseResources();
                vcaNode->getProcessor()->releaseResources();
                outputOversampling->reset();
            }
        }
    }
    // Queue overflow is a fail-closed panic, including host note-ons in this block.
    if (queuePanic && midiProcessorNode != nullptr) {
        renderUntil(buffer.getNumSamples());
        static_cast<MidiProcessor*>(midiProcessorNode->getProcessor())->releaseResources();
        vcfNode->getProcessor()->releaseResources();
        modernVcfNode->getProcessor()->releaseResources();
        vcaNode->getProcessor()->releaseResources();
        outputOversampling->reset();
        keyboardMirror.addMessageToQueue(juce::MidiMessage::controllerEvent(1, 120, 0));
        buffer.clear();
    }
    renderUntil(buffer.getNumSamples());
    midiMessages.clear();

    audioDisplayFifo.push(buffer);
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
    // Hosts may call this on any thread, including their message thread while
    // holding audio locks. Never classify the caller or load XML/files here.
    // The editor explicitly uses ProgramManager's non-RT loading path instead.
    presetManager.requestCurrentProgram(index);
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
            juce::ParameterID{ParameterIds::waveType, 1}, "Wave Type",
            juce::StringArray{"Triangle", "Sawtooth", "Square", "Pulse", "PWM"}, 1),
        std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{ParameterIds::feet, 1}, "Feet",
            juce::StringArray{"32'", "16'", "8'", "4'", "WN"}, 2),
        ParameterFormatting::makeFloat(
            juce::ParameterID{ParameterIds::pwmSpeed, 1}, "PWM Speed",
            // CS01J owner's manual, printed page 24. Taper remains approximate.
            juce::NormalisableRange<float>(0.6f, 12.0f, 0.01f, 0.25f), 2.0f),
        ParameterFormatting::makeFloat(juce::ParameterID{ParameterIds::pitch, 1}, "Pitch",
                                       juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f),
        ParameterFormatting::makeFloat(
            juce::ParameterID{ParameterIds::glissando, 1}, "Glissando",
            juce::NormalisableRange<float>(0.0f, Constants::maxGlissandoPerSemitoneSeconds, 0.001f,
                                           0.5f),
            0.0f));
    layout.add(std::move(vcoGroup));

    auto vcfGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "vcf", "VCF", "|",
        ParameterFormatting::makeFloat(juce::ParameterID{ParameterIds::cutoff, 1}, "Cutoff",
                                       juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f),
                                       20000.0f),
        ParameterFormatting::makeFloat(juce::ParameterID{ParameterIds::resonance, 1}, "Resonance",
                                       juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f),
        ParameterFormatting::makeFloat(juce::ParameterID{ParameterIds::vcfEgDepth, 1},
                                       "VCF EG Depth", juce::NormalisableRange<float>(0.0f, 1.0f),
                                       0.0f));
    layout.add(std::move(vcfGroup));

    auto vcaGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "vca", "VCA", "|",
        ParameterFormatting::makeFloat(juce::ParameterID{ParameterIds::vcaEgDepth, 1},
                                       "VCA EG Depth", juce::NormalisableRange<float>(0.0f, 1.0f),
                                       1.0f));
    layout.add(std::move(vcaGroup));

    auto egGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "eg", "EG", "|",
        // Uncalibrated seconds range. Owner's manual specifies only S-L;
        // skew is a UI mapping, not a measured A2M potentiometer taper.
        ParameterFormatting::makeFloat(juce::ParameterID{ParameterIds::attack, 1}, "Attack",
                                       juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.3f),
                                       0.1f),
        // Uncalibrated decay duration; not derived from the circuit's RC constant.
        ParameterFormatting::makeFloat(juce::ParameterID{ParameterIds::decay, 1}, "Decay",
                                       juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.3f),
                                       0.1f),
        // Normalized sustain level. Circuit audit identifies a B10K potentiometer.
        ParameterFormatting::makeFloat(juce::ParameterID{ParameterIds::sustain, 1}, "Sustain",
                                       juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f),
        // Uncalibrated release duration; retain independently of envelope shape.
        ParameterFormatting::makeFloat(juce::ParameterID{ParameterIds::release, 1}, "Release",
                                       juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.3f),
                                       0.1f));
    layout.add(std::move(egGroup));

    auto lfoGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "lfo", "LFO", "|",
        ParameterFormatting::makeFloat(juce::ParameterID{ParameterIds::lfoSpeed, 1}, "LFO Speed",
                                       juce::NormalisableRange<float>(0.8f, 21.0f, 0.01f, 0.3f),
                                       5.0f),
        std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ParameterIds::lfoTarget, 1},
                                                     "LFO Target", juce::StringArray{"VCO", "VCF"},
                                                     0),
        ParameterFormatting::makeFloat(
            juce::ParameterID{ParameterIds::modDepth, 1}, "Mod Depth",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withAutomatable(false)));
    layout.add(std::move(lfoGroup));

    auto modGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "mod", "Modulation", "|",
        ParameterFormatting::makeFloat(
            juce::ParameterID{ParameterIds::pitchBend, 1}, "Pitch Bend",
            juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withAutomatable(false)),
        ParameterFormatting::makeFloat(juce::ParameterID{ParameterIds::breathVcf, 1}, "Breath VCF",
                                       juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f),
        ParameterFormatting::makeFloat(juce::ParameterID{ParameterIds::breathVca, 1}, "Breath VCA",
                                       juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f),
        ParameterFormatting::makeInt(juce::ParameterID{ParameterIds::pitchBendUpRange, 1},
                                     "Pitch Bend Up", 0, 12, 12),
        ParameterFormatting::makeInt(juce::ParameterID{ParameterIds::pitchBendDownRange, 1},
                                     "Pitch Bend Down", 0, 12, 0));
    layout.add(std::move(modGroup));

    auto globalGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "global", "Global", "|",
        ParameterFormatting::makeFloat(juce::ParameterID{ParameterIds::volume, 1}, "Volume",
                                       juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f),
        ParameterFormatting::makeFloat(juce::ParameterID{ParameterIds::breathInput, 1},
                                       "Breath Input", juce::NormalisableRange<float>(0.0f, 1.0f),
                                       0.0f),
        std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ParameterIds::filterType, 1},
                                                     "Filter Type",
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
        return;
    }

    // Publish only; the message-thread timer changes graph routing.
    if (parameterID == ParameterIds::lfoTarget) {
        requestedLfoTarget.store(static_cast<int>(newValue));
        pendingRoutingChange.store(true);
        return;
    }

    if (parameterID == ParameterIds::filterType) {
        requestedFilterType.store(static_cast<int>(newValue));
        pendingRoutingChange.store(true);
    }
}

void CS01AudioProcessor::timerCallback() {
    presetManager.dispatchProgramNotifications();
    keyboardMirrorMidi.clear();
    const bool overflow = keyboardMirror.removeNextBlockOfMessages(keyboardMirrorMidi, 1);
    mirroringKeyboard = true;
    if (overflow)
        keyboardState.reset();
    keyboardState.processNextMidiBuffer(keyboardMirrorMidi, 0, 1, false);
    mirroringKeyboard = false;

    if (!usesLegacyRouting())
        return;

    // Read authoritative values on the message thread. Registering APVTS listeners
    // here would allocate ListenerList iterator storage on the first host notification.
    const int filter = static_cast<int>(getCurrentParameterValue(apvts, ParameterIds::filterType));
    const int target = static_cast<int>(getCurrentParameterValue(apvts, ParameterIds::lfoTarget));
    const bool filterChanged = requestedFilterType.exchange(filter) != filter;
    const bool targetChanged = requestedLfoTarget.exchange(target) != target;
    if (filterChanged || targetChanged)
        pendingRoutingChange.store(true);

    if (!pendingRoutingChange.exchange(false))
        return;

    applyFilterRouting(requestedFilterType.load(), requestedLfoTarget.load(),
                       juce::AudioProcessorGraph::UpdateKind::async);
}

void CS01AudioProcessor::handleNoteOn(juce::MidiKeyboardState*, int channel, int note,
                                      float velocity) {
    if (mirroringKeyboard.load() && juce::MessageManager::getInstance()->isThisTheMessageThread())
        return;
    auto message = juce::MidiMessage::noteOn(channel, note, velocity);
    message.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    midiMessageCollector.addMessageToQueue(message);
}
void CS01AudioProcessor::handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float) {
    if (mirroringKeyboard.load() && juce::MessageManager::getInstance()->isThisTheMessageThread())
        return;
    auto message = juce::MidiMessage::noteOff(channel, note);
    message.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    midiMessageCollector.addMessageToQueue(message);
}
