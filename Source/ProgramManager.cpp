#include "ProgramManager.h"
#include "Utf8Path.h"
#include "FactoryPresets.h"
#include <tinyxml2.h>
#include <array>
#include <fstream>
#include <iterator>
#include <limits>
#include <cstdio>
#include <chrono>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace {
std::string pathText(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}
bool excluded(cs01::Param id, bool session) {
    return cs01::definition(id).transient || (!session && id == cs01::Param::Volume);
}
std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return {};
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0 || size > 1024 * 1024)
        return {};
    input.seekg(0, std::ios::beg);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
PreparedProgram prepareXml(std::string_view xml, bool session) {
    // Parse and validate everything before changing any live parameters.
    if (xml.empty() || xml.size() > 1024 * 1024)
        return {};
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.data(), xml.size()) != tinyxml2::XML_SUCCESS)
        return {};
    const auto* root = doc.FirstChildElement("Parameters");
    if (!root)
        return {};
    const auto* parent = root->FirstChildElement("PARAMETERS");
    if (!parent)
        parent = root;
    PreparedProgram result;
    auto& values = result.values;
    std::array<bool, cs01::parameterCount> found{};
    for (auto* item = parent->FirstChildElement("PARAM"); item;
         item = item->NextSiblingElement("PARAM")) {
        const char* id = item->Attribute("id");
        float value = 0;
        if (!id || item->QueryFloatAttribute("value", &value) != tinyxml2::XML_SUCCESS ||
            !std::isfinite(value))
            return {};
        for (int i = 0; i < cs01::parameterCount; ++i) {
            const auto& d = cs01::parameterDefinitions[i];
            if (d.id != id)
                continue;
            if (found[i] || value < d.minimum || value > d.maximum)
                return {};
            found[i] = true;
            values[i] = value;
        }
    }
    if (std::none_of(found.begin(), found.end(), [](bool v) { return v; }))
        return {};
    for (int i = 0; i < cs01::parameterCount; ++i)
        if (found[i] && !excluded(static_cast<cs01::Param>(i), session))
            result.mask |= 1u << i;
    result.valid = true;
    return result;
}
// Exclusive temporary file in the target directory; replacement is a single
// filesystem operation. A failed write/replace never truncates the old preset.
bool replaceFile(const std::filesystem::path& target, std::string_view bytes) {
    static std::atomic<unsigned> sequence{0};
    std::filesystem::path temporary;
    FILE* file = nullptr;
    for (int attempt = 0; attempt < 16 && !file; ++attempt) {
        temporary = target;
        temporary += "." +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                     "." + std::to_string(sequence.fetch_add(1)) + ".tmp";
#ifdef _WIN32
        file = _wfopen(temporary.c_str(), L"wbx");
#else
        file = std::fopen(temporary.c_str(), "wbx");
#endif
    }
    if (!file)
        return false;
    const bool written = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    const bool closed = std::fclose(file) == 0;
    bool replaced = false;
    std::error_code ec;
    if (written && closed) {
#ifdef _WIN32
        replaced = MoveFileExW(temporary.c_str(), target.c_str(),
                               MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
        std::filesystem::rename(temporary, target, ec);
        replaced = !ec;
#endif
    }
    if (!replaced)
        std::filesystem::remove(temporary, ec);
    return replaced;
}
}  // namespace
void ProgramManager::setUserDirectory(std::filesystem::path path) {
    std::lock_guard lock(mutex);
    directory = std::move(path);
    refreshUnlocked();
}
void ProgramManager::setBeforePatchCommit(std::function<void()> callback) {
    std::lock_guard lock(mutex);
    beforePatchCommit = std::move(callback);
}
std::vector<Program> ProgramManager::programs() const {
    std::lock_guard lock(mutex);
    return allPresets;
}
int ProgramManager::getCurrentProgram() const {
    std::lock_guard lock(mutex);
    return currentProgram;
}
Program ProgramManager::currentProgramIdentity() const {
    std::lock_guard lock(mutex);
    return allPresets[currentProgram];
}
void ProgramManager::refreshUserPresets() {
    std::lock_guard lock(mutex);
    refreshUnlocked();
}
void ProgramManager::refreshUnlocked() {
    const Program selected = allPresets.empty() ? Program{} : allPresets[currentProgram];
    allPresets.clear();
    for (const auto& preset : cs01::factoryPresets)
        allPresets.push_back({preset.name, preset.filename, PresetType::Factory});
    if (!directory.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        std::vector<Program> user;
        for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end;
             it.increment(ec)) {
            if (it->is_regular_file(ec) && it->path().extension() == ".xml")
                user.push_back({pathText(it->path().stem()), pathText(it->path().filename()),
                                PresetType::User});
        }
        std::sort(user.begin(), user.end(),
                  [](const auto& a, const auto& b) { return a.name < b.name; });
        allPresets.insert(allPresets.end(), user.begin(), user.end());
    }
    currentProgram = 0;
    bool retained = false;
    for (size_t i = 0; i < allPresets.size(); ++i)
        if (allPresets[i].filename == selected.filename && allPresets[i].type == selected.type) {
            currentProgram = static_cast<int>(i);
            retained = true;
        }
    if (!retained && !selected.filename.empty())
        loadXml(cs01::factoryPresets[0].xml, false);
}
bool ProgramManager::setCurrentProgram(int index) {
    std::lock_guard lock(mutex);
    if (index < 0 || index >= static_cast<int>(allPresets.size()))
        return false;
    const auto& program = allPresets[index];
    const bool loaded =
        program.type == PresetType::Factory
            ? loadXml(cs01::factoryPresets[index].xml, false)
            : loadXml(readFile(directory / cs01::utf8Path(program.filename)), false);
    if (loaded)
        currentProgram = index;
    return loaded;
}
std::vector<PreparedProgram> ProgramManager::preparePrograms() const {
    std::lock_guard lock(mutex);
    std::vector<PreparedProgram> result;
    result.reserve(allPresets.size());
    for (size_t i = 0; i < allPresets.size(); ++i) {
        const auto& program = allPresets[i];
        auto prepared =
            program.type == PresetType::Factory
                ? prepareXml(cs01::factoryPresets[i].xml, false)
                : prepareXml(readFile(directory / cs01::utf8Path(program.filename)), false);
        prepared.identity = program;
        result.push_back(std::move(prepared));
    }
    return result;
}
bool ProgramManager::selectProgramIdentity(const Program& program) {
    std::lock_guard lock(mutex);
    for (size_t i = 0; i < allPresets.size(); ++i)
        if (allPresets[i].filename == program.filename && allPresets[i].type == program.type) {
            currentProgram = static_cast<int>(i);
            return true;
        }
    currentProgram = 0;
    return false;
}
bool ProgramManager::loadPresetFromXml(std::string_view xml) {
    std::lock_guard lock(mutex);
    return loadXml(xml, false);
}
bool ProgramManager::loadPresetFile(const std::filesystem::path& file) {
    return loadPresetFromXml(readFile(file));
}
bool ProgramManager::loadXml(std::string_view xml, bool session) {
    const auto prepared = prepareXml(xml, session);
    if (!prepared.valid)
        return false;
    if (beforePatchCommit)
        beforePatchCommit();
    if (!parameters.setPatch(prepared.values, prepared.mask, session))
        return false;
    if (session) {
        tinyxml2::XMLDocument doc;
        doc.Parse(xml.data(), xml.size());
        const auto* root = doc.FirstChildElement("Parameters");
        currentProgram = 0;
        const char* filename = root->Attribute("programFilename");
        if (filename) {
            const auto type =
                root->BoolAttribute("programIsUser") ? PresetType::User : PresetType::Factory;
            for (size_t i = 0; i < allPresets.size(); ++i)
                if (allPresets[i].filename == filename && allPresets[i].type == type)
                    currentProgram = static_cast<int>(i);
        } else
            currentProgram = std::clamp(root->IntAttribute("program", 0), 0,
                                        static_cast<int>(allPresets.size()) - 1);
    }
    return true;
}
std::string ProgramManager::serialize(bool session, const PreparedProgram* selected,
                                      bool pending) const {
    const auto values = parameters.snapshot();
    tinyxml2::XMLDocument doc;
    auto* root = doc.NewElement("Parameters");
    doc.InsertEndChild(root);
    root->SetAttribute("schema", 1);
    if (session) {
        int index = currentProgram;
        if (selected) {
            index = 0;
            for (size_t i = 0; i < allPresets.size(); ++i)
                if (allPresets[i].filename == selected->identity.filename &&
                    allPresets[i].type == selected->identity.type)
                    index = static_cast<int>(i);
        }
        root->SetAttribute("program", index);
        const auto& identity = selected ? selected->identity : allPresets[currentProgram];
        root->SetAttribute("programFilename", identity.filename.c_str());
        root->SetAttribute("programIsUser", identity.type == PresetType::User);
    }
    for (int i = 0; i < cs01::parameterCount; ++i) {
        const auto id = static_cast<cs01::Param>(i);
        if (excluded(id, session))
            continue;
        auto* param = doc.NewElement("PARAM");
        root->InsertEndChild(param);
        param->SetAttribute("id", cs01::definition(id).id.data());
        const float value =
            selected && pending && (selected->mask & (1u << i)) ? selected->values[i] : values[i];
        param->SetAttribute("value", value);
    }
    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    return printer.CStr();
}
std::string ProgramManager::getStateInformation(const PreparedProgram* selected,
                                                bool pending) const {
    std::lock_guard lock(mutex);
    return serialize(true, selected, pending);
}
bool ProgramManager::setStateInformation(std::string_view bytes) {
    std::lock_guard lock(mutex);
    return loadXml(bytes, true);
}
bool ProgramManager::isValidPresetName(std::string_view name) {
    if (name.empty() || name == "." || name == ".." || name.size() > 128 || name.front() == ' ' ||
        name.back() == '.' || name.back() == ' ')
        return false;
    for (unsigned char c : name)
        if (c < 32 || c == 127 ||
            std::string_view("/\\:*?\"<>|").find(static_cast<char>(c)) != std::string_view::npos)
            return false;
    std::string stem(name.substr(0, name.find('.')));
    for (char& c : stem)
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL")
        return false;
    if (stem.size() == 4 && (stem.starts_with("COM") || stem.starts_with("LPT")) &&
        stem[3] >= '1' && stem[3] <= '9')
        return false;
    return true;
}
std::filesystem::path ProgramManager::uniquePath(std::string_view name) const {
    if (directory.empty() || !isValidPresetName(name))
        return {};
    const std::string base(name);
    auto path = directory / cs01::utf8Path(base + ".xml");
    std::error_code ec;
    for (int suffix = 1; std::filesystem::exists(path, ec); ++suffix)
        path = directory / cs01::utf8Path(base + " (" + std::to_string(suffix) + ").xml");
    if (ec)
        return {};
    return path;
}
bool ProgramManager::saveCurrentStateAsPreset(std::string_view name) {
    std::lock_guard lock(mutex);
    if (directory.empty() || !isValidPresetName(name))
        return false;
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec)
        return false;
    const auto path = directory / cs01::utf8Path(std::string(name) + ".xml");
    if (!replaceFile(path, serialize(false)))
        return false;
    refreshUnlocked();
    return true;
}
bool ProgramManager::userPresetExists(std::string_view name) const {
    std::lock_guard lock(mutex);
    if (directory.empty() || !isValidPresetName(name))
        return false;
    std::error_code ec;
    // Also detect files created outside the current menu snapshot.
    return std::filesystem::exists(directory / cs01::utf8Path(std::string(name) + ".xml"), ec);
}
bool ProgramManager::loadProgramIdentity(const Program& identity) {
    std::lock_guard lock(mutex);
    const auto found = std::find_if(allPresets.begin(), allPresets.end(), [&](const Program& item) {
        return item.type == identity.type && item.filename == identity.filename;
    });
    return found != allPresets.end() &&
           setCurrentProgram(static_cast<int>(found - allPresets.begin()));
}
bool ProgramManager::deleteUserPreset(const Program& identity) {
    std::lock_guard lock(mutex);
    if (identity.type != PresetType::User)
        return false;
    const auto found = std::find_if(allPresets.begin(), allPresets.end(), [&](const Program& item) {
        return item.type == PresetType::User && item.filename == identity.filename;
    });
    return found != allPresets.end() &&
           deleteUserPreset(static_cast<int>(found - allPresets.begin()));
}
bool ProgramManager::deleteUserPreset(int index) {
    std::lock_guard lock(mutex);
    if (index < 0 || index >= static_cast<int>(allPresets.size()) ||
        allPresets[index].type != PresetType::User)
        return false;
    const bool selected = index == currentProgram;
    std::error_code ec;
    if (!std::filesystem::remove(directory / cs01::utf8Path(allPresets[index].filename), ec))
        return false;
    refreshUnlocked();
    if (selected)
        setCurrentProgram(0);
    return true;
}
bool ProgramManager::renameUserPreset(const Program& identity, std::string_view name) {
    std::lock_guard lock(mutex);
    if (identity.type != PresetType::User)
        return false;
    const auto found = std::find_if(allPresets.begin(), allPresets.end(), [&](const Program& item) {
        return item.type == PresetType::User && item.filename == identity.filename;
    });
    return found != allPresets.end() &&
           renameUserPreset(static_cast<int>(found - allPresets.begin()), name);
}
bool ProgramManager::renameUserPreset(int index, std::string_view name) {
    std::lock_guard lock(mutex);
    if (index < 0 || index >= static_cast<int>(allPresets.size()) ||
        allPresets[index].type != PresetType::User)
        return false;
    const auto path = uniquePath(name);
    if (path.empty())
        return false;
    std::error_code ec;
    const auto source = directory / cs01::utf8Path(allPresets[index].filename);
    // Rename must never replace another preset (POSIX rename would overwrite it).
    if (source != path && std::filesystem::exists(std::filesystem::symlink_status(path, ec)))
        return false;
    if (ec && ec != std::errc::no_such_file_or_directory)
        return false;
    ec.clear();
    std::filesystem::rename(source, path, ec);
    if (ec)
        return false;
    allPresets[index].filename = pathText(path.filename());
    refreshUnlocked();
    return true;
}
