#pragma once

#include <JuceHeader.h>
#include "Parameters.h"

//==============================================================================
enum class PresetType {
    Factory,
    User
};

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

    // プリセット操作メソッド
    void loadFactoryPreset(int index);
    void loadPresetFromXml(const juce::XmlElement* xml);
    void saveCurrentStateAsPreset(const juce::String& name);
    bool deleteUserPreset(int index);
    bool renameUserPreset(int index, const juce::String& newName);

    // プログラム（プリセット）管理
    int getNumPrograms() const;
    int getCurrentProgram() const;
    void setCurrentProgram(int index);
    juce::String getProgramName(int index) const;
    PresetType getPresetType(int index) const;
    bool isUserPreset(int index) const;

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
    std::vector<Program> allPresets; // Combined list for easy access
    int currentProgram = 0;

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
    void loadPresetFromBinaryData(const juce::String& filename);
    void rebuildAllPresetsList();
    void loadUserPresetFromFile(const juce::File& file);
    juce::String generateUniquePresetName(const juce::String& baseName) const;
};
