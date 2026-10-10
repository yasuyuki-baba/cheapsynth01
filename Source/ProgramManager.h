#pragma once
#include "Parameters.h"
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

enum class PresetType { Factory, User };
struct Program {
    std::string name, filename;
    PresetType type = PresetType::Factory;
};
class ProgramManager {
   public:
    explicit ProgramManager(cs01::ParameterState& p) : parameters(p) {
        refreshUserPresets();
    }
    void setUserDirectory(std::filesystem::path directory);
    std::vector<Program> programs() const;
    int getCurrentProgram() const;
    bool setCurrentProgram(int index);
    bool loadPresetFromXml(std::string_view xml);
    bool loadPresetFile(const std::filesystem::path& file);
    bool saveCurrentStateAsPreset(std::string_view name);
    bool deleteUserPreset(int index);
    bool renameUserPreset(int index, std::string_view name);
    void refreshUserPresets();
    std::string getStateInformation() const;
    bool setStateInformation(std::string_view state);

   private:
    bool loadXml(std::string_view xml, bool session);
    std::string serialize(bool session) const;
    std::filesystem::path uniquePath(std::string_view name) const;
    void refreshUnlocked();
    cs01::ParameterState& parameters;
    std::filesystem::path directory;
    std::vector<Program> allPresets;
    int currentProgram = 0;
    mutable std::recursive_mutex mutex;
};
