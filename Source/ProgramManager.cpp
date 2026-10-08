#include "ProgramManager.h"

#include "MidiParameterValue.h"

#include "BinaryData.h"

#include <cmath>

namespace {
// Silent audio-side program/MIDI writes can leave APVTS's adapter cache behind.
// replaceState compares against that cache, so synchronize it on the non-RT path
// before replacement; otherwise a return to the cached old value can be skipped.
void synchronizeParameterAdapters(juce::AudioProcessorValueTreeState& state) {
    for (auto* parameter : state.processor.getParameters())
        parameter->sendValueChangedMessageToListeners(parameter->getValue());
}
}  // namespace

ProgramManager::ProgramManager(juce::AudioProcessorValueTreeState& apvts) : apvts(apvts) {
#if defined(CHEAPSYNTH_TEST_PRESET_ISOLATION)
    static const auto testRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                     .getChildFile("cheapsynth-tests-" + juce::Uuid().toString());
    userDirectoryOverride = testRoot;
#endif
    initializePresets();
    createUserPresetsDirectory();
    refreshUserPresets();
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
    beginCatalogueRead();
    const auto* table = catalogue.load();
    const int count = table ? static_cast<int>(table->programs.size()) : 0;
    endCatalogueRead();
    return count;
}

int ProgramManager::getCurrentProgram() const {
    beginCatalogueRead();
    const auto* selection = selectedProgram.load();
    const auto* table = catalogue.load();
    int index = 0;
    if (selection && table)
        for (size_t i = 0; i < table->programs.size(); ++i)
            if (table->programs[i].filename == selection->filename &&
                table->programs[i].type == selection->type) {
                index = static_cast<int>(i);
                break;
            }
    endCatalogueRead();
    return index;
}
void ProgramManager::selectPublishedProgram(int index) {
    beginCatalogueRead();
    const auto* table = catalogue.load();
    if (table && index >= 0 && index < static_cast<int>(table->prepared.size()))
        selectedProgram.store(&table->prepared[index]);
    endCatalogueRead();
}

void ProgramManager::setCurrentProgram(int index) {
    if (index >= 0 && index < static_cast<int>(allPresets.size())) {
        const auto& preset = allPresets[index];

        if (preset.type == PresetType::Factory) {
            if (loadPresetFromBinaryData(preset.filename)) {
                currentProgram = index;
                selectPublishedProgram(index);
            }
        } else {
            // Load user preset from file
            auto userPresetsDir = getUserPresetsDirectory();
            auto presetFile = userPresetsDir.getChildFile(preset.filename);
            if (loadUserPresetFromFile(presetFile)) {
                currentProgram = index;
                selectPublishedProgram(index);
            }
        }
    }
}

juce::String ProgramManager::getProgramName(int index) const {
    beginCatalogueRead();
    const auto* table = catalogue.load();
    juce::String result;
    if (table && index >= 0 && index < static_cast<int>(table->programs.size()))
        result = table->programs[index].name;
    endCatalogueRead();
    return result;
}
PresetType ProgramManager::getPresetType(int index) const {
    beginCatalogueRead();
    const auto* table = catalogue.load();
    const auto type = table && index >= 0 && index < static_cast<int>(table->programs.size())
                          ? table->programs[index].type
                          : PresetType::Factory;
    endCatalogueRead();
    return type;
}
bool ProgramManager::isUserPreset(int index) const {
    return getPresetType(index) == PresetType::User;
}
int ProgramManager::findProgram(const juce::String& filename, PresetType type) const {
    beginCatalogueRead();
    const auto* table = catalogue.load();
    int found = -1;
    if (table)
        for (size_t i = 0; i < table->programs.size(); ++i)
            if (table->programs[i].filename == filename && table->programs[i].type == type) {
                found = static_cast<int>(i);
                break;
            }
    endCatalogueRead();
    return found;
}
juce::String ProgramManager::getProgramFilename(int index) const {
    beginCatalogueRead();
    const auto* table = catalogue.load();
    juce::String result;
    if (table && index >= 0 && index < static_cast<int>(table->programs.size()))
        result = table->programs[index].filename;
    endCatalogueRead();
    return result;
}

void ProgramManager::loadFactoryPreset(int index) {
    if (index >= 0 && index < static_cast<int>(factoryPresets.size())) {
        loadPresetFromBinaryData(factoryPresets[index].filename);
    }
}

void ProgramManager::getStateInformation(juce::MemoryBlock& destData) {
    // Get current state as XML
    std::unique_ptr<juce::XmlElement> xml = copyCurrentMidiParameterState(apvts).createXml();

    // Get parameter elements from XML
    if (xml != nullptr) {
        // Add program number
        xml->setAttribute("program", getCurrentProgram());
        xml->setAttribute("programFilename", getProgramFilename(getCurrentProgram()));
        xml->setAttribute("programIsUser", isUserPreset(getCurrentProgram()));

        // Get parameter elements
        {
            auto* params = xml->getChildByName("PARAMETERS");
            if (params == nullptr)
                params = xml.get();
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
            if (xmlState->hasAttribute("programFilename")) {
                const int identified =
                    findProgram(xmlState->getStringAttribute("programFilename"),
                                xmlState->getBoolAttribute("programIsUser") ? PresetType::User
                                                                            : PresetType::Factory);
                currentProgram = identified >= 0 ? identified : 0;
            }
            currentProgram = juce::jlimit(0, getNumPrograms() - 1, currentProgram.load());
            selectPublishedProgram(currentProgram.load());
            synchronizeParameterAdapters(apvts);
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

bool ProgramManager::loadPresetFromBinaryData(const juce::String& filename) {
    // Generate resource name (replace dot in filename extension with underscore)
    auto resourceName = filename.replace(".", "_");

    int dataSize = 0;
    const char* data = BinaryData::getNamedResource(resourceName.toRawUTF8(), dataSize);

    if (dataSize > 0) {
        std::unique_ptr<juce::XmlElement> xmlState(juce::XmlDocument::parse(data));
        if (xmlState != nullptr) {
            return loadPresetFromXml(xmlState.get());
        }
    }
    return false;
}

bool ProgramManager::loadPresetFromXml(const juce::XmlElement* xml) {
    if (xml != nullptr && xml->hasTagName(apvts.state.getType())) {
        // Save current values of parameters excluded from preset loading
        std::map<juce::String, float> persistentValues;
        for (const auto& paramId : presetExcludedParameters) {
            if (auto* param = apvts.getParameter(paramId)) {
                persistentValues[paramId] = param->getValue();
            }
        }

        // Replace ValueTree state
        synchronizeParameterAdapters(apvts);
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
        return true;
    }
    return false;
}

bool ProgramManager::saveCurrentStateAsPreset(const juce::String& name) {
    if (!isValidPresetName(name))
        return false;
    auto userPresetsDir = getUserPresetsDirectory();

    if (!userPresetsDir.exists()) {
        if (!createUserPresetsDirectory()) {
            return false;  // Failed to create directory
        }
    }

    // Use the exact name provided (no automatic numbering)
    auto filename = name + ".xml";
    auto presetFile = userPresetsDir.getChildFile(filename);

    // Get current state as XML
    std::unique_ptr<juce::XmlElement> xml = copyCurrentMidiParameterState(apvts).createXml();
    if (xml != nullptr) {
        // Remove excluded parameters from saved preset
        {
            auto* params = xml->getChildByName("PARAMETERS");
            if (params == nullptr)
                params = xml.get();
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
        juce::TemporaryFile temporary(presetFile);
        if (xml->writeTo(temporary.getFile()) && temporary.overwriteTargetFileWithTemporary()) {
            // Add to user presets list and rebuild
            refreshUserPresets();
            return true;
        }
    }
    return false;
}

bool ProgramManager::deleteUserPreset(int index) {
    if (!isUserPreset(index)) {
        return false;  // Cannot delete factory presets
    }

    const auto& preset = allPresets[index];
    auto userPresetsDir = getUserPresetsDirectory();
    auto presetFile = userPresetsDir.getChildFile(preset.filename);

    if (presetFile.exists() && presetFile.deleteFile()) {
        // Refresh presets and rebuild list
        refreshUserPresets();

        return true;
    }

    return false;
}

bool ProgramManager::renameUserPreset(int index, const juce::String& newName) {
    if (!isUserPreset(index) || !isValidPresetName(newName)) {
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
        // Update the identity before rebuilding so a selected renamed preset is retained.
        const bool wasSelected = getCurrentProgram() == index;
        allPresets[index].filename = newFilename;
        refreshUserPresets();
        if (wasSelected)
            selectPublishedProgram(findProgram(newFilename, PresetType::User));
        return true;
    }

    return false;
}

void ProgramManager::refreshUserPresets() {
    userPresets.clear();
    auto userPresetsDir = getUserPresetsDirectory();

    if (userPresetsDir.exists()) {
        for (const auto& file :
             userPresetsDir.findChildFiles(juce::File::findFiles, false, "*.xml")) {
            auto nameWithoutExtension = file.getFileNameWithoutExtension();
            userPresets.emplace_back(nameWithoutExtension, file.getFileName(), PresetType::User);
        }

        // Sort user presets alphabetically
        std::sort(userPresets.begin(), userPresets.end(),
                  [](const Program& a, const Program& b) { return a.name < b.name; });
    }
    rebuildAllPresetsList();
}

juce::File ProgramManager::getUserPresetsDirectory() const {
    if (userDirectoryOverride != juce::File{})
        return userDirectoryOverride;
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("CheapSynth01")
        .getChildFile("UserPresets");
}

bool ProgramManager::createUserPresetsDirectory() {
    auto dir = getUserPresetsDirectory();
    return dir.createDirectory();
}

void ProgramManager::rebuildAllPresetsList() {
    const int selection = getCurrentProgram();
    const bool selectedValid = selection >= 0 && selection < static_cast<int>(allPresets.size());
    const auto selectedFilename = selectedValid ? allPresets[selection].filename : juce::String{};
    const auto selectedType = selectedValid ? allPresets[selection].type : PresetType::Factory;
    allPresets.clear();

    // Add factory presets first
    for (const auto& preset : factoryPresets) {
        allPresets.push_back(preset);
    }

    // Add user presets
    for (const auto& preset : userPresets) {
        allPresets.push_back(preset);
    }
    publishCatalogue();
    const int selected = findProgram(selectedFilename, selectedType);
    if (selected >= 0) {
        currentProgram = selected;  // Do not reload: preserve edits to the current sound.
    } else if (selectedFilename.isNotEmpty()) {
        setCurrentProgram(0);  // A removed selected preset falls back to Default and its sound.
    } else {
        currentProgram = 0;
    }
}

bool ProgramManager::loadUserPresetFromFile(const juce::File& file) {
    if (file.exists()) {
        juce::XmlDocument xmlDoc(file.loadFileAsString());
        std::unique_ptr<juce::XmlElement> xmlState(xmlDoc.getDocumentElement());
        if (xmlState != nullptr) {
            return loadPresetFromXml(xmlState.get());
        }
    }
    return false;
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

// Portable single filename: reject path syntax, control characters and Windows devices.
bool ProgramManager::isValidPresetName(const juce::String& name) {
    if (name.isEmpty() || name != name.trim() || name.endsWithChar('.') || name.length() > 120 ||
        name.containsAnyOf("/\\:<>\"|?*") || name == "." || name == "..")
        return false;
    for (auto c : name)
        if (c < 32 || c == 127)
            return false;
    const auto stem = name.upToFirstOccurrenceOf(".", false, false).toUpperCase();
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL")
        return false;
    for (int i = 1; i <= 9; ++i)
        if (stem == "COM" + juce::String(i) || stem == "LPT" + juce::String(i))
            return false;
    return true;
}

void ProgramManager::setUserPresetsDirectoryForTesting(const juce::File& directory) {
    userDirectoryOverride = directory;
    refreshUserPresets();
}

void ProgramManager::publishCatalogue() {
    auto table = std::make_unique<Catalogue>();
    table->programs = allPresets;
    for (size_t i = 0; i < allPresets.size(); ++i) {
        const auto& program = allPresets[i];
        std::unique_ptr<juce::XmlElement> xml;
        if (program.type == PresetType::Factory) {
            int size = 0;
            const auto resource = program.filename.replace(".", "_");
            if (const auto* data = BinaryData::getNamedResource(resource.toRawUTF8(), size))
                xml = juce::XmlDocument::parse(juce::String::fromUTF8(data, size));
        } else {
            xml =
                juce::XmlDocument::parse(getUserPresetsDirectory().getChildFile(program.filename));
        }
        PreparedProgram prepared;
        prepared.index = static_cast<int>(i);
        prepared.filename = program.filename;
        prepared.type = program.type;
        prepared.valid = xml && xml->hasTagName(apvts.state.getType());
        if (prepared.valid) {
            // APVTS fills missing parameter nodes from each parameter's default.
            // Include those defaults so cached host application matches replaceState.
            for (auto* raw : apvts.processor.getParameters())
                if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(raw))
                    if (!isPresetExcludedParameter(parameter->paramID))
                        prepared.values.emplace_back(parameter, parameter->getDefaultValue());
            const auto* parameters = xml->getChildByName("PARAMETERS");
            if (!parameters)
                parameters = xml.get();
            for (auto* child : parameters->getChildIterator()) {
                const auto id = child->getStringAttribute("id");
                if (isPresetExcludedParameter(id))
                    continue;
                if (auto* parameter = apvts.getParameter(id)) {
                    const float value = static_cast<float>(child->getDoubleAttribute("value"));
                    if (!std::isfinite(value)) {
                        prepared.valid = false;
                        break;
                    }
                    for (auto& [target, normalized] : prepared.values)
                        if (target == parameter)
                            normalized = parameter->convertTo0to1(value);
                }
            }
        }
        table->prepared.push_back(std::move(prepared));
    }
    const auto* published = table.get();
    catalogues.push_back(std::move(table));
    catalogue.store(published);
    reclaimCatalogues();
}
void ProgramManager::reclaimCatalogues() {
    const auto activity = catalogueActivity.load();
    const auto* current = catalogue.load();
    const auto* pending = pendingProgram.load();
    const auto* selected = selectedProgram.load();
    // Pins and reader count must describe one quiescent interval. Also detect
    // a reader that started AND finished while these snapshots were taken.
    if (catalogueReaders.load() != 0 || catalogueActivity.load() != activity)
        return;
    std::erase_if(catalogues, [&](const auto& table) {
        if (table.get() == current)
            return false;
        // A queued host request pins the table until the audio thread applies it.
        for (const auto& program : table->prepared)
            if (&program == pending || &program == selected)
                return false;
        return true;
    });
}
void ProgramManager::requestCurrentProgram(int index) {
    beginCatalogueRead();
    const auto* table = catalogue.load();
    if (table && index >= 0 && index < static_cast<int>(table->prepared.size()) &&
        table->prepared[index].valid)
        pendingProgram.store(&table->prepared[index]);
    endCatalogueRead();
}
void ProgramManager::applyPendingProgram() {
    beginCatalogueRead();
    if (const auto* program = pendingProgram.exchange(nullptr)) {
        const int index = findProgram(program->filename, program->type);
        if (index < 0) {
            endCatalogueRead();
            return;
        }
        for (const auto& [parameter, value] : program->values)
            parameter->setValue(value);  // standard JUCE parameter atomic stores; no listeners
        selectedProgram.store(program);
        currentProgram.store(index);
        programNotifications.store(true);
    }
    endCatalogueRead();
}
void ProgramManager::dispatchProgramNotifications() {
    if (programNotifications.exchange(false))
        for (auto* parameter : apvts.processor.getParameters())
            parameter->sendValueChangedMessageToListeners(parameter->getValue());
    reclaimCatalogues();
}
