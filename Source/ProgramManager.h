#pragma once
#include "Parameters.h"
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

enum class PresetType { Factory, User };
struct Program {
    std::string name, filename;
    PresetType type = PresetType::Factory;
};
struct PreparedProgram {
    Program identity;
    std::array<float, cs01::parameterCount> values{};
    unsigned mask = 0;
    bool valid = false;
};
class ProgramManager {
   public:
    explicit ProgramManager(cs01::ParameterState& p) : parameters(p) {
        refreshUserPresets();
    }
    void setUserDirectory(std::filesystem::path directory);
    void setBeforePatchCommit(std::function<void()> callback);
    std::vector<Program> programs() const;
    int getCurrentProgram() const;
    Program currentProgramIdentity() const;
    bool setCurrentProgram(int index);
    std::vector<PreparedProgram> preparePrograms() const;
    bool selectProgramIdentity(const Program& program);
    bool loadPresetFromXml(std::string_view xml);
    bool loadPresetFile(const std::filesystem::path& file);
    bool saveCurrentStateAsPreset(std::string_view name);
    static bool isValidPresetName(std::string_view name);
    bool deleteUserPreset(int index);
    bool renameUserPreset(int index, std::string_view name);
    void refreshUserPresets();
    std::string getStateInformation(const PreparedProgram* selected = nullptr,
                                    bool pending = false) const;
    bool setStateInformation(std::string_view state);

   private:
    bool loadXml(std::string_view xml, bool session);
    std::string serialize(bool session, const PreparedProgram* selected = nullptr,
                          bool pending = false) const;
    std::filesystem::path uniquePath(std::string_view name) const;
    void refreshUnlocked();
    cs01::ParameterState& parameters;
    std::filesystem::path directory;
    std::vector<Program> allPresets;
    int currentProgram = 0;
    mutable std::recursive_mutex mutex;
    std::function<void()> beforePatchCommit;
};
