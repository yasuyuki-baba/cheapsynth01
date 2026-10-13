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
    // Saved-patch category for the selection menu; not part of the identity.
    std::string category{};
};
struct PresetMetadata {
    std::string id, author, category, description;
    std::vector<std::string> tags;
    bool operator==(const PresetMetadata&) const = default;
};
struct PreparedProgram {
    Program identity;
    std::array<float, cs01::parameterCount> values{};
    unsigned mask = 0;
    bool valid = false;
    PresetMetadata metadata;
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
    PreparedProgram currentPreparedProgram() const;
    PresetMetadata currentPresetMetadata() const;
    bool isCurrentProgramModified() const;
    bool setCurrentProgram(int index);
    std::vector<PreparedProgram> preparePrograms() const;
    bool selectProgramIdentity(const Program& program);
    bool selectPreparedProgram(const PreparedProgram& program);
    bool loadPresetFromXml(std::string_view xml);
    bool loadPresetFile(const std::filesystem::path& file);
    bool saveCurrentStateAsPreset(std::string_view name);
    bool saveCurrentUserPreset();
    bool saveCurrentStateAsNewPreset(std::string_view name);
    static bool isValidPresetName(std::string_view name);
    bool userPresetExists(std::string_view name) const;
    bool deleteUserPreset(int index);
    bool deleteUserPreset(const Program& identity);
    bool loadProgramIdentity(const Program& identity);
    bool renameUserPreset(int index, std::string_view name);
    bool renameUserPreset(const Program& identity, std::string_view name);
    void refreshUserPresets();
    std::string getStateInformation(const PreparedProgram* selected = nullptr,
                                    bool pending = false) const;
    bool setStateInformation(std::string_view state);

   private:
    bool loadXml(std::string_view xml, bool session);
    std::string serialize(bool session, const PreparedProgram* selected = nullptr,
                          bool pending = false, const PresetMetadata* metadata = nullptr) const;
    int findProgram(const Program& identity, std::string_view id) const;
    std::filesystem::path uniquePath(std::string_view name) const;
    void refreshUnlocked();
    void setReference(const PreparedProgram& program);
    cs01::ParameterState& parameters;
    std::filesystem::path directory;
    std::vector<Program> allPresets;
    // File references are prepared outside host state-restore/audio callbacks.
    std::vector<PreparedProgram> references;
    cs01::ParameterState::Values referenceValues{};
    bool referenceKnown = false;
    PresetMetadata metadata, referenceMetadata;
    int currentProgram = 0;
    mutable std::recursive_mutex mutex;
    std::function<void()> beforePatchCommit;
};
