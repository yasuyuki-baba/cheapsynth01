#include "ProgramManager.h"
#include "BinaryData.h"

ProgramManager::ProgramManager(juce::AudioProcessorValueTreeState& apvts)
    : apvts(apvts)
{
    initializePresets();
}

ProgramManager::~ProgramManager()
{
}

void ProgramManager::initializePresets()
{
    factoryPresets.push_back({ "Default", "Default.xml" });
    factoryPresets.push_back({ "Flute", "Flute.xml" });
    factoryPresets.push_back({ "Violin", "Violin.xml" });
    factoryPresets.push_back({ "Trumpet", "Trumpet.xml" });
    factoryPresets.push_back({ "Clavinet", "Clavinet.xml" });
    factoryPresets.push_back({ "Solo Synth Lead", "Solo_Synth_Lead.xml" });
    factoryPresets.push_back({ "Synth Bass", "Synth_Bass.xml" });
}

int ProgramManager::getNumPrograms() const 
{ 
    return static_cast<int>(factoryPresets.size());
}

int ProgramManager::getCurrentProgram() const
{ 
    return currentProgram;
}

void ProgramManager::setCurrentProgram(int index) 
{
    if (index >= 0 && index < static_cast<int>(factoryPresets.size()))
    {
        currentProgram = index;
        loadFactoryPreset(index);
    }
}

juce::String ProgramManager::getProgramName(int index) const
{ 
    if (index >= 0 && index < static_cast<int>(factoryPresets.size()))
        return factoryPresets[index].name;
    return {};
}

void ProgramManager::loadFactoryPreset(int index)
{
    if (index >= 0 && index < static_cast<int>(factoryPresets.size()))
    {
        loadPresetFromBinaryData(factoryPresets[index].filename);
    }
}

void ProgramManager::getStateInformation(juce::MemoryBlock& destData)
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
        juce::AudioProcessor::copyXmlToBinary(*xml, destData);
    }
}

void ProgramManager::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(juce::AudioProcessor::getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
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
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
            
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

void ProgramManager::loadPresetFromBinaryData(const juce::String& filename)
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
            loadPresetFromXml(xmlState.get());
        }
    }
}

void ProgramManager::loadPresetFromXml(const juce::XmlElement* xml)
{
    if (xml != nullptr)
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
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        
        // Restore values of parameters excluded from state
        for (const auto& [paramId, value] : persistentValues)
        {
            if (auto* param = apvts.getParameter(paramId))
            {
                param->setValueNotifyingHost(value);
            }
        }
        
        // Notify parameter changes
        for (auto* param : apvts.processor.getParameters())
        {
            param->sendValueChangedMessageToListeners(param->getValue());
        }
    }
}

void ProgramManager::saveCurrentStateAsPreset(const juce::String& name)
{
    // Not implemented yet - could be used for user presets in the future
}

bool ProgramManager::isStateExcludedParameter(const juce::String& paramId) const
{
    return std::find(stateExcludedParameters.begin(), 
                     stateExcludedParameters.end(), 
                     paramId) != stateExcludedParameters.end();
}
