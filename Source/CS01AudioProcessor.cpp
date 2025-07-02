#include "CS01AudioProcessor.h"
#include "CS01AudioProcessorEditor.h"
#include "Parameters.h"
#include "CS01Synth/VCOProcessor.h"
#include "CS01Synth/MidiProcessor.h"
#include "BinaryData.h"

//==============================================================================
CS01AudioProcessor::CS01AudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    initializePresets();
    apvts.addParameterListener(ParameterIds::lfoTarget, this);
    apvts.addParameterListener(ParameterIds::filterType, this);
    apvts.addParameterListener(ParameterIds::feet, this);
}

CS01AudioProcessor::~CS01AudioProcessor()
{
    apvts.removeParameterListener(ParameterIds::lfoTarget, this);
    apvts.removeParameterListener(ParameterIds::filterType, this);
    apvts.removeParameterListener(ParameterIds::feet, this);
}

void CS01AudioProcessor::initializePresets()
{
    factoryPresets.push_back({ "Default", "Default.xml" });
    factoryPresets.push_back({ "Flute", "Flute.xml" });
    factoryPresets.push_back({ "Violin", "Violin.xml" });
    factoryPresets.push_back({ "Trumpet", "Trumpet.xml" });
    factoryPresets.push_back({ "Clavinet", "Clavinet.xml" });
    factoryPresets.push_back({ "Solo Synth Lead", "Solo_Synth_Lead.xml" });
    factoryPresets.push_back({ "Synth Bass", "Synth_Bass.xml" });
}

//==============================================================================
void CS01AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    midiMessageCollector.reset(sampleRate);
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) getMainBusNumOutputChannels() };

    audioGraph.clear();

    // 1. Add nodes
    midiInputNode = audioGraph.addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
        juce::AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode));
    audioOutputNode = audioGraph.addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    midiProcessorNode = audioGraph.addNode(std::make_unique<MidiProcessor>(apvts));
    vcoNode = audioGraph.addNode(std::make_unique<VCOProcessor>(apvts));
    egNode = audioGraph.addNode(std::make_unique<EGProcessor>(apvts));
    lfoNode = audioGraph.addNode(std::make_unique<LFOProcessor>(apvts));
    vcaNode = audioGraph.addNode(std::make_unique<VCAProcessor>(apvts));
    vcfNode = audioGraph.addNode(std::make_unique<CS01VCFProcessor>(apvts));
    modernVcfNode = audioGraph.addNode(std::make_unique<ModernVCFProcessor>(apvts));
    noiseNode = audioGraph.addNode(std::make_unique<NoiseProcessor>(apvts));

    // 2. Set bus layouts
    audioOutputNode->getProcessor()->enableAllBuses();
    // midiProcessorNode has no audio buses
    vcoNode->getProcessor()->enableAllBuses();
    egNode->getProcessor()->enableAllBuses();
    lfoNode->getProcessor()->enableAllBuses();
    vcaNode->getProcessor()->enableAllBuses();
    vcfNode->getProcessor()->enableAllBuses();
    modernVcfNode->getProcessor()->enableAllBuses();
    noiseNode->getProcessor()->enableAllBuses();

    // 3. Connect nodes
    // Configure connections based on filter type and feet setting
    auto filterType = static_cast<int>(apvts.getRawParameterValue(ParameterIds::filterType)->load());
    auto feetValue = static_cast<int>(apvts.getRawParameterValue(ParameterIds::feet)->load());
    bool isNoiseMode = (feetValue == static_cast<int>(Feet::WhiteNoise));
    
    // Audio Path: 
    // If noise mode: noise -> vcf -> vca -> output
    // Otherwise: vco -> vcf -> vca -> output
    // Connect only the mono channel (ch = 0)
    if (filterType == 0) // CS01
    {
        if (isNoiseMode)
        {
            audioGraph.addConnection({ {noiseNode->nodeID, 0}, {vcfNode->nodeID, 0} });
        }
        else
        {
            audioGraph.addConnection({ {vcoNode->nodeID, 0}, {vcfNode->nodeID, 0} });
        }
        audioGraph.addConnection({ {vcfNode->nodeID, 0}, {vcaNode->nodeID, 0} });
    }
    else // MODERN
    {
        if (isNoiseMode)
        {
            audioGraph.addConnection({ {noiseNode->nodeID, 0}, {modernVcfNode->nodeID, 0} });
        }
        else
        {
            audioGraph.addConnection({ {vcoNode->nodeID, 0}, {modernVcfNode->nodeID, 0} });
        }
        audioGraph.addConnection({ {modernVcfNode->nodeID, 0}, {vcaNode->nodeID, 0} });
    }
    // Connection from VCA to audioOutputNode (automatically configured based on output bus layout)
    updateVCAOutputConnections();

    // Sidechain Paths
    // EG -> VCA (Sidechain)
    audioGraph.addConnection({ {egNode->nodeID, 0}, {vcaNode->nodeID, 1} });
    // EG -> VCF (Sidechain)
    audioGraph.addConnection({ {egNode->nodeID, 0}, {vcfNode->nodeID, 1} });
    // EG -> ModernVCF (Sidechain)
    audioGraph.addConnection({ {egNode->nodeID, 0}, {modernVcfNode->nodeID, 1} });

    // LFO Path is connected dynamically via parameterChanged listener

    // MIDI Path - Simplified: only midiInput -> midiProcessor
    // No other MIDI connections needed as MidiProcessor directly controls ToneGenerator and EGProcessor
    audioGraph.addConnection({{midiInputNode->nodeID, juce::AudioProcessorGraph::midiChannelIndex},
                              {midiProcessorNode->nodeID, juce::AudioProcessorGraph::midiChannelIndex}});
                              
    // Set references to NoteHandler and EGProcessor in MidiProcessor
    auto* midiProcessor = static_cast<MidiProcessor*>(midiProcessorNode->getProcessor());
    auto* synthProcessor = static_cast<VCOProcessor*>(vcoNode->getProcessor());
    auto* noiseProcessor = static_cast<NoiseProcessor*>(noiseNode->getProcessor());
    auto* egProcessor = static_cast<EGProcessor*>(egNode->getProcessor());
    
    if (midiProcessor != nullptr && synthProcessor != nullptr && noiseProcessor != nullptr && egProcessor != nullptr)
    {
        // Set the appropriate note handler based on the feet parameter
        auto feetValue = static_cast<int>(apvts.getRawParameterValue(ParameterIds::feet)->load());
        bool isNoiseMode = (feetValue == static_cast<int>(Feet::WhiteNoise));
        
        if (isNoiseMode)
        {
            midiProcessor->setNoteHandler(noiseProcessor);
        }
        else
        {
            midiProcessor->setNoteHandler(synthProcessor->getNoteHandler());
        }
        
        midiProcessor->setEGProcessor(egProcessor);
    }
    // 4. Set graph's main bus layout and prepare
    audioGraph.setPlayConfigDetails(getMainBusNumInputChannels(),
                                    getMainBusNumOutputChannels(),
                                    sampleRate,
                                    samplesPerBlock);
    audioGraph.prepareToPlay(sampleRate, samplesPerBlock);

    // Set initial LFO routing
    parameterChanged(ParameterIds::lfoTarget, apvts.getRawParameterValue(ParameterIds::lfoTarget)->load());
}

void CS01AudioProcessor::releaseResources() {
    audioGraph.releaseResources();
}

bool CS01AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;
    
    return true;
}

void CS01AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    midiMessageCollector.removeNextBlockOfMessages(midiMessages, buffer.getNumSamples());

    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
    audioGraph.processBlock(buffer, midiMessages);

    if (auto* editor = dynamic_cast<CS01AudioProcessorEditor*>(getActiveEditor()))
    {
        editor->getOscilloscope().pushBuffer(buffer);
        editor->getAudioVisualiser().pushBuffer(buffer);
    }
}

//==============================================================================
//==============================================================================
int CS01AudioProcessor::getNumPrograms() 
{ 
    return static_cast<int>(factoryPresets.size());
}

int CS01AudioProcessor::getCurrentProgram() 
{ 
    return currentProgram;
}

void CS01AudioProcessor::setCurrentProgram (int index) 
{
    if (index >= 0 && index < factoryPresets.size())
    {
        currentProgram = index;
        loadPresetFromBinaryData(factoryPresets[index].filename);
    }
}

const juce::String CS01AudioProcessor::getProgramName (int index) 
{ 
    if (index >= 0 && index < factoryPresets.size())
        return factoryPresets[index].name;
    return {};
}

void CS01AudioProcessor::changeProgramName (int index, const juce::String& newName) { }

//==============================================================================
void CS01AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Get current state as XML
    std::unique_ptr<juce::XmlElement> xml = apvts.copyState().createXml();
    
    // Get parameter elements from XML
    if (xml != nullptr)
    {
        // Add program number
        xml->setAttribute("program", currentProgram);
        
        // Get parameter elements
        if (auto* params = xml->getChildByName("PARAMETERS"))
        {
            // Remove excluded parameters
            for (int i = params->getNumChildElements() - 1; i >= 0; --i)
            {
                auto* param = params->getChildElement(i);
                if (param != nullptr)
                {
                    // Get parameter ID
                    if (param->hasAttribute("id"))
                    {
                        juce::String id = param->getStringAttribute("id");
                        
                        // Remove parameters excluded from state
                        if (isStateExcludedParameter(id))
                        {
                            params->removeChildElement(param, true);
                        }
                    }
                }
            }
        }
        
        // Convert XML to binary
        copyXmlToBinary(*xml, destData);
    }
}

void CS01AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName (apvts.state.getType()))
        {
            // Save current values of parameters excluded from state
            std::map<juce::String, float> persistentValues;
            for (const auto& paramId : stateExcludedParameters)
            {
                if (auto* param = apvts.getParameter(paramId))
                {
                    persistentValues[paramId] = param->getValue();
                }
            }
            
            // Restore state
            currentProgram = xmlState->getIntAttribute("program", 0);
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
            
            // Restore values of parameters excluded from state
            for (const auto& [paramId, value] : persistentValues)
            {
                if (auto* param = apvts.getParameter(paramId))
                {
                    param->setValueNotifyingHost(value);
                }
            }
        }
    }
}

void CS01AudioProcessor::loadPresetFromBinaryData(const juce::String& filename)
{
    // Generate resource name (replace dot in filename extension with underscore)
    auto resourceName = filename.replace(".", "_");
    
    int dataSize = 0;
    const char* data = BinaryData::getNamedResource(resourceName.toRawUTF8(), dataSize);

    if (dataSize > 0)
    {
        std::unique_ptr<juce::XmlElement> xmlState(juce::XmlDocument::parse(data));
        if (xmlState != nullptr)
        {
            
            // Save current values of parameters excluded from state
            std::map<juce::String, float> persistentValues;
            for (const auto& paramId : stateExcludedParameters)
            {
                if (auto* param = apvts.getParameter(paramId))
                {
                    persistentValues[paramId] = param->getValue();
                }
            }
                
            // Replace ValueTree state
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
            
            // Restore values of parameters excluded from state
            for (const auto& [paramId, value] : persistentValues)
            {
                if (auto* param = apvts.getParameter(paramId))
                {
                    param->setValueNotifyingHost(value);
                }
            }
            
            // Notify parameter changes
            for (auto* param : getParameters())
            {
                param->sendValueChangedMessageToListeners(param->getValue());
            }
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* CS01AudioProcessor::createEditor()
{
    return new CS01AudioProcessorEditor(*this);
}
bool CS01AudioProcessor::hasEditor() const { return true; }

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout CS01AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto vcoGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "vco", "VCO", "|",
        std::make_unique<juce::AudioParameterChoice>(ParameterIds::waveType, "Wave Type", juce::StringArray{"Triangle", "Sawtooth", "Square", "Pulse", "PWM"}, 1),
        std::make_unique<juce::AudioParameterChoice>(ParameterIds::feet, "Feet", juce::StringArray{"32'", "16'", "8'", "4'", "WN"}, 2),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::pwmSpeed, "PWM Speed", juce::NormalisableRange<float>(0.0f, 60.0f, 0.01f, 0.25f), 2.0f),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::pitch, "Pitch", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::glissando, "Glissando", juce::NormalisableRange<float>(0.0f, Constants::maxGlissandoPerSemitoneSeconds, 0.001f, 0.5f), 0.0f)
    );
    layout.add(std::move(vcoGroup));

    auto vcfGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "vcf", "VCF", "|",
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::cutoff, "Cutoff", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 20000.0f),
        std::make_unique<juce::AudioParameterBool>(ParameterIds::resonance, "Resonance", false),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::vcfEgDepth, "VCF EG Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f)
    );
    layout.add(std::move(vcfGroup));

    auto vcaGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "vca", "VCA", "|",
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::vcaEgDepth, "VCA EG Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f)
    );
    layout.add(std::move(vcaGroup));

    auto egGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "eg", "EG", "|",
        // Attack time: Range based on A2M potentiometer (0-2MΩ) with exponential curve
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::attack, "Attack", juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.3f), 0.1f),
        // Decay time: Range based on A2M potentiometer (0-2MΩ) with exponential curve
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::decay, "Decay", juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.3f), 0.1f),
        // Sustain level: Range based on B1M potentiometer (0-1MΩ) with linear response
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::sustain, "Sustain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f),
        // Release time: Range based on A2M potentiometer (0-2MΩ) with exponential curve
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::release, "Release", juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.3f), 0.1f)
    );
    layout.add(std::move(egGroup));

    auto lfoGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "lfo", "LFO", "|",
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::lfoSpeed, "LFO Speed", juce::NormalisableRange<float>(0.0f, 21.0f, 0.01f, 0.3f), 5.0f),
        std::make_unique<juce::AudioParameterChoice>(ParameterIds::lfoTarget, "LFO Target", juce::StringArray{"VCO", "VCF"}, 0),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::modDepth, "Mod Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f, juce::AudioParameterFloatAttributes().withAutomatable(false))
    );
    layout.add(std::move(lfoGroup));

    auto modGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "mod", "Modulation", "|",
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::pitchBend, "Pitch Bend", juce::NormalisableRange<float>(0.0f, 12.0f), 0.0f, juce::AudioParameterFloatAttributes().withAutomatable(false)),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::breathVcf, "Breath VCF", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::breathVca, "Breath VCA", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f),
        std::make_unique<juce::AudioParameterInt>(ParameterIds::pitchBendUpRange, "Pitch Bend Up", 0, 12, 12),
        std::make_unique<juce::AudioParameterInt>(ParameterIds::pitchBendDownRange, "Pitch Bend Down", 0, 12, 12)
    );
    layout.add(std::move(modGroup));

    auto globalGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "global", "Global", "|",
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::volume, "Volume", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::breathInput, "Breath Input", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f),
        std::make_unique<juce::AudioParameterChoice>(ParameterIds::filterType, "Filter Type", juce::StringArray{"CS01", "MODERN"}, 0)
    );
    layout.add(std::move(globalGroup));

    return layout;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CS01AudioProcessor();
}

void CS01AudioProcessor::updateVCAOutputConnections()
{
    // Check if audio graph nodes are initialized
    if (vcaNode == nullptr || audioOutputNode == nullptr)
    {
        return;
    }

    // Remove existing connections
    audioGraph.removeConnection({ {vcaNode->nodeID, 0}, {audioOutputNode->nodeID, 0} });
    audioGraph.removeConnection({ {vcaNode->nodeID, 0}, {audioOutputNode->nodeID, 1} });

    // Get current output bus configuration
    auto outputLayout = getBusesLayout().getMainOutputChannelSet();
    
    // Assuming CS01VCAProcessor always outputs mono
    if (outputLayout == juce::AudioChannelSet::stereo())
    {
        // For stereo output, duplicate mono signal to both channels
        audioGraph.addConnection({ {vcaNode->nodeID, 0}, {audioOutputNode->nodeID, 0} });
        audioGraph.addConnection({ {vcaNode->nodeID, 0}, {audioOutputNode->nodeID, 1} });
    }
    else // For mono output
    {
        // For mono output, connect directly
        audioGraph.addConnection({ {vcaNode->nodeID, 0}, {audioOutputNode->nodeID, 0} });
    }
}

void CS01AudioProcessor::processorLayoutsChanged()
{
    // Call parent class processing
    AudioProcessor::processorLayoutsChanged();
    
    // Update connections if bus configuration has changed
    if (audioOutputNode != nullptr && vcaNode != nullptr)
    {
        updateVCAOutputConnections();
    }
}

void CS01AudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == ParameterIds::feet)
    {
        // Check if audio graph nodes are initialized
        if (vcoNode == nullptr || noiseNode == nullptr || vcfNode == nullptr || modernVcfNode == nullptr || midiProcessorNode == nullptr)
        {
            // Do nothing if nodes are not initialized yet
            return;
        }

        // Determine if we're in noise mode
        bool isNoiseMode = (static_cast<int>(newValue) == static_cast<int>(Feet::WhiteNoise));
        
        // Remove existing connections
        audioGraph.removeConnection({ {vcoNode->nodeID, 0}, {vcfNode->nodeID, 0} });
        audioGraph.removeConnection({ {vcoNode->nodeID, 0}, {modernVcfNode->nodeID, 0} });
        audioGraph.removeConnection({ {noiseNode->nodeID, 0}, {vcfNode->nodeID, 0} });
        audioGraph.removeConnection({ {noiseNode->nodeID, 0}, {modernVcfNode->nodeID, 0} });
        
        // Reconfigure connections based on filter type and noise mode
        auto filterType = static_cast<int>(apvts.getRawParameterValue(ParameterIds::filterType)->load());
        
        if (filterType == 0) // CS01
        {
            if (isNoiseMode)
            {
                audioGraph.addConnection({ {noiseNode->nodeID, 0}, {vcfNode->nodeID, 0} });
            }
            else
            {
                audioGraph.addConnection({ {vcoNode->nodeID, 0}, {vcfNode->nodeID, 0} });
            }
        }
        else // MODERN
        {
            if (isNoiseMode)
            {
                audioGraph.addConnection({ {noiseNode->nodeID, 0}, {modernVcfNode->nodeID, 0} });
            }
            else
            {
                audioGraph.addConnection({ {vcoNode->nodeID, 0}, {modernVcfNode->nodeID, 0} });
            }
        }
        
        // Update the note handler in MidiProcessor based on the feet parameter
        auto* midiProcessor = static_cast<MidiProcessor*>(midiProcessorNode->getProcessor());
        auto* synthProcessor = static_cast<VCOProcessor*>(vcoNode->getProcessor());
        auto* noiseProcessor = static_cast<NoiseProcessor*>(noiseNode->getProcessor());
        
        if (midiProcessor != nullptr && synthProcessor != nullptr && noiseProcessor != nullptr)
        {
            if (isNoiseMode)
            {
                midiProcessor->setNoteHandler(noiseProcessor);
            }
            else
            {
                midiProcessor->setNoteHandler(synthProcessor->getNoteHandler());
            }
        }
    }
    else if (parameterID == ParameterIds::lfoTarget)
    {
        // Check if audio graph nodes are initialized
        if (lfoNode == nullptr || vcfNode == nullptr || modernVcfNode == nullptr || vcoNode == nullptr)
        {
            // Do nothing if nodes are not initialized yet
            return;
        }

        // Disconnect all LFO connections first
        audioGraph.removeConnection({ {lfoNode->nodeID, 0}, {vcfNode->nodeID, 2} });
        audioGraph.removeConnection({ {lfoNode->nodeID, 0}, {modernVcfNode->nodeID, 2} });
        audioGraph.removeConnection({ {lfoNode->nodeID, 0}, {vcoNode->nodeID, 0} });

        // LFOProcessor has mono output, so connect only channel 0
        // Reconnect based on the new value
        if (static_cast<int>(newValue) == 0) // Target: VCO
        {
            // Connect LFO to SynthProcessor's LFO input (bus 0)
            audioGraph.addConnection({ {lfoNode->nodeID, 0}, {vcoNode->nodeID, 0} });
        }
        else // Target: VCF
        {
            // Connect LFO according to current filter type
            auto filterType = static_cast<int>(apvts.getRawParameterValue(ParameterIds::filterType)->load());
            if (filterType == 0) // CS01
            {
                // Connect LFO to VCFProcessor's LFO input (bus 2)
                audioGraph.addConnection({ {lfoNode->nodeID, 0}, {vcfNode->nodeID, 2} });
            }
            else // MODERN
            {
                // Connect LFO to ModernVCFProcessor's LFO input (bus 2)
                audioGraph.addConnection({ {lfoNode->nodeID, 0}, {modernVcfNode->nodeID, 2} });
            }
        }
    }
    else if (parameterID == ParameterIds::filterType)
    {
        // Check if audio graph nodes are initialized
        if (vcoNode == nullptr || vcfNode == nullptr || modernVcfNode == nullptr || vcaNode == nullptr)
        {
            // Do nothing if nodes are not initialized yet
            return;
        }

        // Remove existing connections (mono channel only)
        audioGraph.removeConnection({ {vcoNode->nodeID, 0}, {vcfNode->nodeID, 0} });
        audioGraph.removeConnection({ {vcfNode->nodeID, 0}, {vcaNode->nodeID, 0} });
        audioGraph.removeConnection({ {vcoNode->nodeID, 0}, {modernVcfNode->nodeID, 0} });
        audioGraph.removeConnection({ {modernVcfNode->nodeID, 0}, {vcaNode->nodeID, 0} });

        // Determine if we're in noise mode
        bool isNoiseMode = (static_cast<int>(apvts.getRawParameterValue(ParameterIds::feet)->load()) == static_cast<int>(Feet::WhiteNoise));
        
        // Reconfigure connections based on filter type
        if (static_cast<int>(newValue) == 0) // CS01
        {
            // Remove existing connections
            audioGraph.removeConnection({ {vcoNode->nodeID, 0}, {modernVcfNode->nodeID, 0} });
            audioGraph.removeConnection({ {noiseNode->nodeID, 0}, {modernVcfNode->nodeID, 0} });
            audioGraph.removeConnection({ {modernVcfNode->nodeID, 0}, {vcaNode->nodeID, 0} });
            
            // Add new connections
            if (isNoiseMode)
            {
                audioGraph.addConnection({ {noiseNode->nodeID, 0}, {vcfNode->nodeID, 0} });
            }
            else
            {
                audioGraph.addConnection({ {vcoNode->nodeID, 0}, {vcfNode->nodeID, 0} });
            }
            audioGraph.addConnection({ {vcfNode->nodeID, 0}, {vcaNode->nodeID, 0} });
            
            // Also update LFO connection (if LFO target is VCF)
            if (static_cast<int>(apvts.getRawParameterValue(ParameterIds::lfoTarget)->load()) == 1)
            {
                audioGraph.removeConnection({ {lfoNode->nodeID, 0}, {modernVcfNode->nodeID, 2} });
                audioGraph.addConnection({ {lfoNode->nodeID, 0}, {vcfNode->nodeID, 2} });
            }
        }
        else // MODERN
        {
            // Remove existing connections
            audioGraph.removeConnection({ {vcoNode->nodeID, 0}, {vcfNode->nodeID, 0} });
            audioGraph.removeConnection({ {noiseNode->nodeID, 0}, {vcfNode->nodeID, 0} });
            audioGraph.removeConnection({ {vcfNode->nodeID, 0}, {vcaNode->nodeID, 0} });
            
            // Add new connections
            if (isNoiseMode)
            {
                audioGraph.addConnection({ {noiseNode->nodeID, 0}, {modernVcfNode->nodeID, 0} });
            }
            else
            {
                audioGraph.addConnection({ {vcoNode->nodeID, 0}, {modernVcfNode->nodeID, 0} });
            }
            audioGraph.addConnection({ {modernVcfNode->nodeID, 0}, {vcaNode->nodeID, 0} });
            
            // Also update LFO connection (if LFO target is VCF)
            if (static_cast<int>(apvts.getRawParameterValue(ParameterIds::lfoTarget)->load()) == 1)
            {
                audioGraph.removeConnection({ {lfoNode->nodeID, 0}, {vcfNode->nodeID, 2} });
                audioGraph.addConnection({ {lfoNode->nodeID, 0}, {modernVcfNode->nodeID, 2} });
            }
        }
    }
}
