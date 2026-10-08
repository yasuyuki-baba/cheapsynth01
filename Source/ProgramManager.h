#pragma once

#include <JuceHeader.h>

#include "Parameters.h"

#include <vector>
#include <atomic>
#include <memory>

//==============================================================================
enum class PresetType { Factory, User };

struct Program {
    juce::String name;
    juce::String filename;
    PresetType type;

    Program(const juce::String& n, const juce::String& f, PresetType t = PresetType::Factory)
        : name(n), filename(f), type(t) {}
};

class ProgramManager {
   public:
    ProgramManager(juce::AudioProcessorValueTreeState& apvts);
    ~ProgramManager();

    void requestCurrentProgram(int index);
    void applyPendingProgram();
    void dispatchProgramNotifications();

    // プリセット操作メソッド
    static bool isValidPresetName(const juce::String& name);
    void setUserPresetsDirectoryForTesting(const juce::File& directory);

    void loadFactoryPreset(int index);
    bool loadPresetFromXml(const juce::XmlElement* xml);
    bool saveCurrentStateAsPreset(const juce::String& name);
    bool deleteUserPreset(int index);
    bool renameUserPreset(int index, const juce::String& newName);

    // プログラム（プリセット）管理
    int getNumPrograms() const;
    int getCurrentProgram() const;
    void setCurrentProgram(int index);
    juce::String getProgramName(int index) const;
    PresetType getPresetType(int index) const;
    bool isUserPreset(int index) const;
    int findProgram(const juce::String& filename, PresetType type) const;
    juce::String getProgramFilename(int index) const;

    // ユーザープリセット管理
    void refreshUserPresets();
    juce::File getUserPresetsDirectory() const;
    bool createUserPresetsDirectory();

    // 状態の保存と復元
    void getStateInformation(juce::MemoryBlock& destData);
    void setStateInformation(const void* data, int sizeInBytes);

   private:
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Program> factoryPresets;
    std::vector<Program> userPresets;
    std::vector<Program> allPresets;  // Combined list for easy access
    std::atomic<int> currentProgram{0};
    juce::File userDirectoryOverride;
    struct PreparedProgram {
        int index = 0;
        bool valid = false;
        juce::String filename;
        PresetType type = PresetType::Factory;
        std::vector<std::pair<juce::RangedAudioParameter*, float>> values;
    };
    struct Catalogue {
        std::vector<Program> programs;
        std::vector<PreparedProgram> prepared;
    };
    // Readers use an atomic count; only the non-RT writer allocates/reclaims.
    mutable std::atomic<unsigned> catalogueReaders{0};
    mutable std::atomic<uint64_t> catalogueActivity{0};
    void beginCatalogueRead() const {
        catalogueReaders.fetch_add(1);
        catalogueActivity.fetch_add(1);
    }
    void endCatalogueRead() const {
        catalogueActivity.fetch_add(1);
        catalogueReaders.fetch_sub(1);
    }
    std::atomic<const Catalogue*> catalogue{nullptr};
    std::atomic<const PreparedProgram*> pendingProgram{nullptr};
    std::atomic<const PreparedProgram*> selectedProgram{nullptr};
    void selectPublishedProgram(int index);
    std::vector<std::unique_ptr<Catalogue>> catalogues;
    std::atomic<bool> programNotifications{false};
    void publishCatalogue();
    void reclaimCatalogues();

    // プリセット読み込み時に除外するパラメータ（音量変化を防ぐため）
    const std::vector<juce::String> presetExcludedParameters = {
        ParameterIds::breathInput, ParameterIds::volume, ParameterIds::modDepth,
        ParameterIds::pitchBend};

    // DAWセッション保存時に除外するパラメータ（リアルタイム入力系のみ）
    const std::vector<juce::String> sessionExcludedParameters = {
        ParameterIds::breathInput, ParameterIds::pitchBend, ParameterIds::modDepth};

    bool isSessionExcludedParameter(const juce::String& paramId) const;
    bool isPresetExcludedParameter(const juce::String& paramId) const;

    void initializePresets();
    bool loadPresetFromBinaryData(const juce::String& filename);
    void rebuildAllPresetsList();
    bool loadUserPresetFromFile(const juce::File& file);
    juce::String generateUniquePresetName(const juce::String& baseName) const;
};
