#include "ProgramManager.h"
#include "BinaryData.h"

ProgramManager::ProgramManager(juce::AudioProcessorValueTreeState& apvts) : apvts(apvts) {
    initializePresets();
    createUserPresetsDirectory();
    refreshUserPresets();
    rebuildAllPresetsList();
}

ProgramManager::~ProgramManager() {}

void ProgramManager::initializePresets() {
    factoryPresets.clear();
    factoryPresets.emplace_back("Default", "Default.xml", PresetType::Factory);
    factoryPresets.emplace_back("Flute", "Flute.xml", PresetType::Factory);
    factoryPresets.emplace_back("Violin", "Violin.xml", PresetType::Factory);
    factoryPresets.emplace_back("Trumpet", "Trumpet.xml", PresetType::Factory);
    factoryPresets.emplace_back("Clavinet", "Clavinet.xml", PresetType::Factory);
    factoryPresets.emplace_back("Solo Synth Lead", "Solo_Synth_Lead.xml", PresetType::Factory);
    factoryPresets.emplace_back("Synth Bass", "Synth_Bass.xml", PresetType::Factory);
}

int ProgramManager::getNumPrograms() const {
    return static_cast<int>(allPresets.size());
}

int ProgramManager::getCurrentProgram() const {
    return currentProgram;
}

void ProgramManager::setCurrentProgram(int index) {
    if (index >= 0 && index < static_cast<int>(allPresets.size())) {
        currentProgram = index;
        const auto& preset = allPresets[index];
        
        if (preset.type == PresetType::Factory) {
            loadPresetFromBinaryData(preset.filename);
        } else {
            // Load user preset from file
            auto userPresetsDir = getUserPresetsDirectory();
            auto presetFile = userPresetsDir.getChildFile(preset.filename);
            loadUserPresetFromFile(presetFile);
        }
    }
}

juce::String ProgramManager::getProgramName(int index) const {
    if (index >= 0 && index < static_cast<int>(allPresets.size()))
        return allPresets[index].name;
    return {};
}

PresetType ProgramManager::getPresetType(int index) const {
    if (index >= 0 && index < static_cast<int>(allPresets.size()))
        return allPresets[index].type;
    return PresetType::Factory;
}

bool ProgramManager::isUserPreset(int index) const {
    return getPresetType(index) == PresetType::User;
}

void ProgramManager::loadFactoryPreset(int index) {
    if (index >= 0 && index < static_cast<int>(factoryPresets.size())) {
        loadPresetFromBinaryData(factoryPresets[index].filename);
    }
}

void ProgramManager::getStateInformation(juce::MemoryBlock& destData) {
    // Get current state as XML
    std::unique_ptr<juce::XmlElement> xml = apvts.copyState().createXml();

    // Get parameter elements from XML
    if (xml != nullptr) {
        // Add program number
        xml->setAttribute("program", currentProgram);

        // Get parameter elements
        if (auto* params = xml->getChildByName("PARAMETERS")) {
            // Remove excluded parameters (only realtime input parameters for DAW sessions)
            for (int i = params->getNumChildElements() - 1; i >= 0; --i) {
                auto* param = params->getChildElement(i);
                if (param != nullptr) {
                    // Get parameter ID
                    if (param->hasAttribute("id")) {
                        juce::String id = param->getStringAttribute("id");

                        // Remove parameters excluded from DAW session state
                        if (isSessionExcludedParameter(id)) {
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

void ProgramManager::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(
        juce::AudioProcessor::getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr) {
        if (xmlState->hasTagName(apvts.state.getType())) {
            // Save current values of parameters excluded from DAW session state
            std::map<juce::String, float> persistentValues;
            for (const auto& paramId : sessionExcludedParameters) {
                if (auto* param = apvts.getParameter(paramId)) {
                    persistentValues[paramId] = param->getValue();
                }
            }

            // Restore state
            currentProgram = xmlState->getIntAttribute("program", 0);
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

            // Restore values of parameters excluded from DAW session state
            for (const auto& [paramId, value] : persistentValues) {
                if (auto* param = apvts.getParameter(paramId)) {
                    param->setValueNotifyingHost(value);
                }
            }
        }
    }
}

void ProgramManager::loadPresetFromBinaryData(const juce::String& filename) {
    // Generate resource name (replace dot in filename extension with underscore)
    auto resourceName = filename.replace(".", "_");

    int dataSize = 0;
    const char* data = BinaryData::getNamedResource(resourceName.toRawUTF8(), dataSize);

    if (dataSize > 0) {
        std::unique_ptr<juce::XmlElement> xmlState(juce::XmlDocument::parse(data));
        if (xmlState != nullptr) {
            loadPresetFromXml(xmlState.get());
        }
    }
}

void ProgramManager::loadPresetFromXml(const juce::XmlElement* xml) {
    if (xml != nullptr) {
        // Save current values of parameters excluded from preset loading
        std::map<juce::String, float> persistentValues;
        for (const auto& paramId : presetExcludedParameters) {
            if (auto* param = apvts.getParameter(paramId)) {
                persistentValues[paramId] = param->getValue();
            }
        }

        // Replace ValueTree state
        apvts.replaceState(juce::ValueTree::fromXml(*xml));

        // Restore values of parameters excluded from preset loading
        for (const auto& [paramId, value] : persistentValues) {
            if (auto* param = apvts.getParameter(paramId)) {
                param->setValueNotifyingHost(value);
            }
        }

        // Notify parameter changes
        for (auto* param : apvts.processor.getParameters()) {
            param->sendValueChangedMessageToListeners(param->getValue());
        }
    }
}

void ProgramManager::saveCurrentStateAsPreset(const juce::String& name) {
    auto userPresetsDir = getUserPresetsDirectory();
    
    if (!userPresetsDir.exists()) {
        if (!createUserPresetsDirectory()) {
            return; // Failed to create directory
        }
    }

    // Use the exact name provided (no automatic numbering)
    auto filename = name + ".xml";
    auto presetFile = userPresetsDir.getChildFile(filename);

    // Get current state as XML
    std::unique_ptr<juce::XmlElement> xml = apvts.copyState().createXml();
    if (xml != nullptr) {
        // Remove excluded parameters from saved preset
        if (auto* params = xml->getChildByName("PARAMETERS")) {
            for (int i = params->getNumChildElements() - 1; i >= 0; --i) {
                auto* param = params->getChildElement(i);
                if (param != nullptr && param->hasAttribute("id")) {
                    juce::String id = param->getStringAttribute("id");
                    if (isPresetExcludedParameter(id)) {
                        params->removeChildElement(param, true);
                    }
                }
            }
        }

        // Save to file (will overwrite if exists)
        if (xml->writeTo(presetFile)) {
            // Add to user presets list and rebuild
            refreshUserPresets();
            rebuildAllPresetsList();
        }
    }
}

bool ProgramManager::deleteUserPreset(int index) {
    if (!isUserPreset(index)) {
        return false; // Cannot delete factory presets
    }

    const auto& preset = allPresets[index];
    auto userPresetsDir = getUserPresetsDirectory();
    auto presetFile = userPresetsDir.getChildFile(preset.filename);

    if (presetFile.exists() && presetFile.deleteFile()) {
        // Refresh presets and rebuild list
        refreshUserPresets();
        rebuildAllPresetsList();
        
        // Adjust current program if necessary
        if (currentProgram >= static_cast<int>(allPresets.size())) {
            currentProgram = allPresets.empty() ? 0 : static_cast<int>(allPresets.size()) - 1;
        }
        
        return true;
    }
    
    return false;
}

bool ProgramManager::renameUserPreset(int index, const juce::String& newName) {
    if (!isUserPreset(index) || newName.isEmpty()) {
        return false;
    }

    const auto& preset = allPresets[index];
    auto userPresetsDir = getUserPresetsDirectory();
    auto oldFile = userPresetsDir.getChildFile(preset.filename);
    
    if (!oldFile.exists()) {
        return false;
    }

    // Generate unique filename for new name
    auto uniqueName = generateUniquePresetName(newName);
    auto newFilename = uniqueName + ".xml";
    auto newFile = userPresetsDir.getChildFile(newFilename);

    if (oldFile.moveFileTo(newFile)) {
        refreshUserPresets();
        rebuildAllPresetsList();
        return true;
    }
    
    return false;
}

void ProgramManager::refreshUserPresets() {
    userPresets.clear();
    auto userPresetsDir = getUserPresetsDirectory();
    
    if (userPresetsDir.exists()) {
        for (const auto& file : userPresetsDir.findChildFiles(juce::File::findFiles, false, "*.xml")) {
            auto nameWithoutExtension = file.getFileNameWithoutExtension();
            userPresets.emplace_back(nameWithoutExtension, file.getFileName(), PresetType::User);
        }
        
        // Sort user presets alphabetically
        std::sort(userPresets.begin(), userPresets.end(), 
                  [](const Program& a, const Program& b) {
                      return a.name < b.name;
                  });
    }
}

juce::File ProgramManager::getUserPresetsDirectory() const {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
           .getChildFile("CheapSynth01")
           .getChildFile("UserPresets");
}

bool ProgramManager::createUserPresetsDirectory() {
    auto dir = getUserPresetsDirectory();
    return dir.createDirectory();
}

void ProgramManager::rebuildAllPresetsList() {
    allPresets.clear();
    
    // Add factory presets first
    for (const auto& preset : factoryPresets) {
        allPresets.push_back(preset);
    }
    
    // Add user presets
    for (const auto& preset : userPresets) {
        allPresets.push_back(preset);
    }
}

void ProgramManager::loadUserPresetFromFile(const juce::File& file) {
    if (file.exists()) {
        juce::XmlDocument xmlDoc(file.loadFileAsString());
        std::unique_ptr<juce::XmlElement> xmlState(xmlDoc.getDocumentElement());
        if (xmlState != nullptr) {
            loadPresetFromXml(xmlState.get());
        }
    }
}

juce::String ProgramManager::generateUniquePresetName(const juce::String& baseName) const {
    auto userPresetsDir = getUserPresetsDirectory();
    auto name = baseName;
    int counter = 1;
    
    // Check if name already exists
    while (userPresetsDir.getChildFile(name + ".xml").exists()) {
        name = baseName + " (" + juce::String(counter) + ")";
        counter++;
    }
    
    return name;
}

bool ProgramManager::isSessionExcludedParameter(const juce::String& paramId) const {
    return std::find(sessionExcludedParameters.begin(), sessionExcludedParameters.end(), paramId) !=
           sessionExcludedParameters.end();
}

bool ProgramManager::isPresetExcludedParameter(const juce::String& paramId) const {
    return std::find(presetExcludedParameters.begin(), presetExcludedParameters.end(), paramId) !=
           presetExcludedParameters.end();
}
