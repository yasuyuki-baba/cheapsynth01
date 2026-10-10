#include "ProgramManager.h"
#include "Utf8Path.h"
#include "FactoryPresets.h"
#include <tinyxml2.h>
#include <array>
#include <fstream>
#include <iterator>
#include <limits>
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
}  // namespace
void ProgramManager::setUserDirectory(std::filesystem::path path) {
    std::lock_guard lock(mutex);
    directory = std::move(path);
    refreshUnlocked();
}
std::vector<Program> ProgramManager::programs() const {
    std::lock_guard lock(mutex);
    return allPresets;
}
int ProgramManager::getCurrentProgram() const {
    std::lock_guard lock(mutex);
    return currentProgram;
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
    for (size_t i = 0; i < allPresets.size(); ++i)
        if (allPresets[i].filename == selected.filename && allPresets[i].type == selected.type)
            currentProgram = static_cast<int>(i);
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
bool ProgramManager::loadPresetFromXml(std::string_view xml) {
    std::lock_guard lock(mutex);
    return loadXml(xml, false);
}
bool ProgramManager::loadPresetFile(const std::filesystem::path& file) {
    return loadPresetFromXml(readFile(file));
}
bool ProgramManager::loadXml(std::string_view xml, bool session) {
    // Parse and validate everything before changing any live parameters.
    if (xml.empty() || xml.size() > 1024 * 1024)
        return false;
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.data(), xml.size()) != tinyxml2::XML_SUCCESS)
        return false;
    const auto* root = doc.FirstChildElement("Parameters");
    if (!root)
        return false;
    const auto* parent = root->FirstChildElement("PARAMETERS");
    if (!parent)
        parent = root;
    std::array<float, cs01::parameterCount> values{};
    std::array<bool, cs01::parameterCount> found{};
    for (auto* item = parent->FirstChildElement("PARAM"); item;
         item = item->NextSiblingElement("PARAM")) {
        const char* id = item->Attribute("id");
        float value = 0;
        if (!id || item->QueryFloatAttribute("value", &value) != tinyxml2::XML_SUCCESS ||
            !std::isfinite(value))
            return false;
        for (int i = 0; i < cs01::parameterCount; ++i) {
            const auto& d = cs01::parameterDefinitions[i];
            if (d.id != id)
                continue;
            if (found[i] || value < d.minimum || value > d.maximum)
                return false;
            found[i] = true;
            values[i] = value;
        }
    }
    if (std::none_of(found.begin(), found.end(), [](bool v) { return v; }))
        return false;
    for (int i = 0; i < cs01::parameterCount; ++i) {
        const auto id = static_cast<cs01::Param>(i);
        if (found[i] && !excluded(id, session))
            parameters.set(id, values[i]);
    }
    if (session) {
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
std::string ProgramManager::serialize(bool session) const {
    tinyxml2::XMLDocument doc;
    auto* root = doc.NewElement("Parameters");
    doc.InsertEndChild(root);
    root->SetAttribute("schema", 1);
    if (session) {
        root->SetAttribute("program", currentProgram);
        root->SetAttribute("programFilename", allPresets[currentProgram].filename.c_str());
        root->SetAttribute("programIsUser", allPresets[currentProgram].type == PresetType::User);
    }
    for (int i = 0; i < cs01::parameterCount; ++i) {
        const auto id = static_cast<cs01::Param>(i);
        if (excluded(id, session))
            continue;
        auto* param = doc.NewElement("PARAM");
        root->InsertEndChild(param);
        param->SetAttribute("id", cs01::definition(id).id.data());
        param->SetAttribute("value", parameters.get(id));
    }
    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    return printer.CStr();
}
std::string ProgramManager::getStateInformation() const {
    std::lock_guard lock(mutex);
    return serialize(true);
}
bool ProgramManager::setStateInformation(std::string_view bytes) {
    std::lock_guard lock(mutex);
    // Legacy JUCE XML chunk: little-endian magic 0x21324356 and a byte length.
    if (bytes.size() >= 8 && bytes.substr(0, 4) == std::string_view("VC2!", 4)) {
        uint32_t length = 0;
        for (unsigned i = 0; i < 4; ++i)
            length |= static_cast<uint32_t>(static_cast<unsigned char>(bytes[4 + i])) << (8 * i);
        if (length > bytes.size() - 8)
            return false;
        bytes = bytes.substr(8, length);
    }
    return loadXml(bytes, true);
}
std::filesystem::path ProgramManager::uniquePath(std::string_view name) const {
    if (directory.empty() || name.empty() || name == "." || name == ".." || name.size() > 128 ||
        name.back() == '.' || name.back() == ' ')
        return {};
    for (unsigned char c : name)
        if (c < 32 ||
            std::string_view("/\\:*?\"<>|").find(static_cast<char>(c)) != std::string_view::npos)
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
    const auto path = uniquePath(name);
    if (path.empty())
        return false;
    std::ofstream file(path, std::ios::binary);
    if (!file)
        return false;
    file << serialize(false);
    file.close();
    if (!file)
        return false;
    refreshUnlocked();
    return true;
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
bool ProgramManager::renameUserPreset(int index, std::string_view name) {
    std::lock_guard lock(mutex);
    if (index < 0 || index >= static_cast<int>(allPresets.size()) ||
        allPresets[index].type != PresetType::User)
        return false;
    const auto path = uniquePath(name);
    if (path.empty())
        return false;
    std::error_code ec;
    std::filesystem::rename(directory / cs01::utf8Path(allPresets[index].filename), path, ec);
    if (ec)
        return false;
    allPresets[index].filename = pathText(path.filename());
    refreshUnlocked();
    return true;
}
