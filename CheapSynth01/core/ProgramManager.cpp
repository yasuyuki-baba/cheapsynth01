#include "ProgramManager.h"
#include "Utf8Path.h"
#include "FactoryPresets.h"
#include "BuildVersion.h"
#include <tinyxml2.h>
#include <array>
#include <fstream>
#include <iterator>
#include <limits>
#include <cstdio>
#include <chrono>
#include <random>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace {
constexpr const char* currentSchema = "1";
std::string newPresetId() {
    // UUID v4; generated only when saving a new user file, outside audio paths.
    std::array<unsigned char, 16> bytes;
    try {
        std::random_device random;
        for (auto& byte : bytes)
            byte = static_cast<unsigned char>(random());
    } catch (const std::exception&) {
        return {};
    }
    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            result += '-';
        result += hex[bytes[i] >> 4];
        result += hex[bytes[i] & 0x0f];
    }
    return result;
}
std::string pathText(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}
bool excluded(cs01::Param id, bool session) {
    return cs01::definition(id).transient ||
           (!session && (id == cs01::Param::Volume || id == cs01::Param::Oversampling));
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
    // A missing schema denotes legacy XML. Explicit versions must be supported;
    // do not silently interpret a future format using today's parameter rules.
    const char* schema = root->Attribute("schema");
    if (schema && std::string_view(schema) != currentSchema)
        return {};
    const auto* parent = root->FirstChildElement("PARAMETERS");
    if (!parent)
        parent = root;
    PreparedProgram result;
    auto attribute = [root](const char* name) {
        const char* text = root->Attribute(name);
        return std::string(text ? text : "");
    };
    result.metadata.id = attribute("id");
    result.metadata.author = attribute("author");
    result.metadata.category = attribute("category");
    result.metadata.description = attribute("description");
    if (const auto* tags = root->FirstChildElement("TAGS"))
        for (const auto* tag = tags->FirstChildElement("TAG"); tag;
             tag = tag->NextSiblingElement("TAG"))
            if (const char* text = tag->GetText(); text && *text)
                result.metadata.tags.emplace_back(text);
    auto& values = result.values;
    std::array<bool, cs01::parameterCount> found{};
    for (auto* item = parent->FirstChildElement("PARAM"); item;
         item = item->NextSiblingElement("PARAM")) {
        const char* id = item->Attribute("id");
        // Older user XML sometimes wrote empty bend-range elements. Treat only
        // these schema-less placeholders as missing, so defaults apply below.
        if (!schema && id && !item->Attribute("value") &&
            (std::string_view(id) == "PITCH_BEND_UP_RANGE" ||
             std::string_view(id) == "PITCH_BEND_DOWN_RANGE"))
            continue;
        float value = 0;
        if (!id || item->QueryFloatAttribute("value", &value) != tinyxml2::XML_SUCCESS ||
            !std::isfinite(value))
            return {};
        for (int i = 0; i < cs01::parameterCount; ++i) {
            const auto& d = cs01::parameterDefinitions[i];
            if (d.id != id)
                continue;
            // Legacy envelopes allowed shorter times than the circuit model.
            // Import them at the current minimum; keep schema 1 validation strict.
            if (!schema && cs01::isEnvelopeTime(static_cast<cs01::Param>(i)) &&
                value >= 0 && value < d.minimum)
                value = d.minimum;
            if (found[i] || value < d.minimum || value > d.maximum)
                return {};
            found[i] = true;
            values[i] = value;
        }
    }
    if (std::none_of(found.begin(), found.end(), [](bool v) { return v; }))
        return {};
    // Missing saved controls use initialization defaults, never the previous
    // patch. This also supplies bend ranges absent from legacy factory XML.
    for (int i = 0; i < cs01::parameterCount; ++i)
        if (!excluded(static_cast<cs01::Param>(i), session)) {
            if (!found[i])
                values[i] = cs01::parameterDefinitions[i].initial;
            result.mask |= 1u << i;
        }
    result.valid = true;
    return result;
}
// Exclusive temporary file in the target directory; replacement is a single
// filesystem operation. A failed write/replace never truncates the old preset.
bool replaceFile(const std::filesystem::path& target, std::string_view bytes,
                 bool overwrite = true) {
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
        replaced =
            MoveFileExW(temporary.c_str(), target.c_str(),
                        (overwrite ? MOVEFILE_REPLACE_EXISTING : 0) | MOVEFILE_WRITE_THROUGH) != 0;
#else
        if (overwrite)
            std::filesystem::rename(temporary, target, ec);
        else
            // Publish the completed file without replacing a concurrently created preset.
            std::filesystem::create_hard_link(temporary, target, ec);
        replaced = !ec;
#endif
    }
    if (!replaced || !overwrite)
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
    auto result = allPresets;
    for (size_t i = 0; i < result.size(); ++i)
        result[i].category = references[i].metadata.category;
    return result;
}
int ProgramManager::getCurrentProgram() const {
    std::lock_guard lock(mutex);
    return currentProgram;
}
Program ProgramManager::currentProgramIdentity() const {
    std::lock_guard lock(mutex);
    auto result = allPresets[currentProgram];
    result.category = references[currentProgram].metadata.category;
    return result;
}
PreparedProgram ProgramManager::currentPreparedProgram() const {
    std::lock_guard lock(mutex);
    return references[currentProgram];
}
PresetMetadata ProgramManager::currentPresetMetadata() const {
    std::lock_guard lock(mutex);
    return metadata;
}
bool ProgramManager::isCurrentProgramModified() const {
    std::lock_guard lock(mutex);
    if (!referenceKnown || metadata.author != referenceMetadata.author ||
        metadata.category != referenceMetadata.category ||
        metadata.description != referenceMetadata.description ||
        metadata.tags != referenceMetadata.tags)
        return true;
    const auto values = parameters.snapshot();
    for (int i = 0; i < cs01::parameterCount; ++i) {
        if (excluded(static_cast<cs01::Param>(i), false))
            continue;
        const float tolerance = 1.e-6f * std::max(1.f, std::abs(referenceValues[i]));
        if (std::abs(values[i] - referenceValues[i]) > tolerance)
            return true;
    }
    return false;
}
void ProgramManager::setReference(const PreparedProgram& program) {
    referenceKnown = program.valid;
    referenceMetadata = program.metadata;
    referenceValues = parameters.snapshot();
    for (int i = 0; i < cs01::parameterCount; ++i)
        if (program.mask & (1u << i))
            referenceValues[i] = program.values[i];
}
void ProgramManager::refreshUserPresets() {
    std::lock_guard lock(mutex);
    refreshUnlocked();
}
void ProgramManager::refreshUnlocked() {
    const Program selected = allPresets.empty() ? Program{} : allPresets[currentProgram];
    const auto selectedId =
        references.empty() ? std::string{} : references[currentProgram].metadata.id;
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
    references = preparePrograms();
    const int retainedIndex = findProgram(selected, selectedId);
    const bool retained = retainedIndex >= 0;
    currentProgram = retained ? retainedIndex : 0;
    if (!retained) {
        if (!selected.filename.empty())
            loadXml(cs01::factoryPresets[0].xml, false);
        else
            metadata = references[0].metadata;
        setReference(references[0]);
    }
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
    if (loaded) {
        currentProgram = index;
        // Use the actual loaded values, including fields omitted by older files.
        referenceValues = parameters.snapshot();
        referenceKnown = true;
        references[index] = {program, referenceValues, (1u << cs01::parameterCount) - 1, true,
                             metadata};
        referenceMetadata = metadata;
    }
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
int ProgramManager::findProgram(const Program& program, std::string_view id) const {
    int match = -1;
    if (!id.empty()) {
        for (size_t i = 0; i < references.size(); ++i)
            if (allPresets[i].type == program.type && references[i].metadata.id == id) {
                if (match >= 0) {
                    match = -1;  // Duplicated IDs require a matching filename.
                    break;
                }
                match = static_cast<int>(i);
            }
        if (match >= 0)
            return match;
    }
    for (size_t i = 0; i < allPresets.size(); ++i)
        if (allPresets[i].filename == program.filename && allPresets[i].type == program.type &&
            (id.empty() || references[i].metadata.id.empty() || references[i].metadata.id == id))
            return static_cast<int>(i);
    return -1;
}
bool ProgramManager::selectProgramIdentity(const Program& program) {
    std::lock_guard lock(mutex);
    const int index = findProgram(program, {});
    if (index >= 0) {
        currentProgram = index;
        metadata = references[index].metadata;
        setReference(references[index]);
        return true;
    }
    currentProgram = 0;
    referenceKnown = false;
    return false;
}
bool ProgramManager::selectPreparedProgram(const PreparedProgram& program) {
    std::lock_guard lock(mutex);
    const int index = findProgram(program.identity, program.metadata.id);
    const bool found = index >= 0;
    currentProgram = found ? index : 0;
    metadata = program.metadata;
    // Hosts load their immutable cached patch, which may differ from today's file.
    setReference(program);
    return found;
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
    metadata = prepared.metadata;
    if (session) {
        tinyxml2::XMLDocument doc;
        doc.Parse(xml.data(), xml.size());
        const auto* root = doc.FirstChildElement("Parameters");
        currentProgram = 0;
        const char* filename = root->Attribute("programFilename");
        bool matched = !filename;
        if (filename) {
            const auto type =
                root->BoolAttribute("programIsUser") ? PresetType::User : PresetType::Factory;
            const int index = findProgram({{}, filename, type}, metadata.id);
            if (index >= 0) {
                currentProgram = index;
                matched = true;
            }
        } else
            currentProgram = std::clamp(root->IntAttribute("program", 0), 0,
                                        static_cast<int>(allPresets.size()) - 1);
        setReference(references[currentProgram]);
        if (!matched)
            referenceKnown = false;
    }
    return true;
}
std::string ProgramManager::serialize(bool session, const PreparedProgram* selected, bool pending,
                                      const PresetMetadata* savedMetadata) const {
    const auto values = parameters.snapshot();
    tinyxml2::XMLDocument doc;
    auto* root = doc.NewElement("Parameters");
    doc.InsertEndChild(root);
    root->SetAttribute("schema", currentSchema);
    root->SetAttribute("pluginVersion", cs01::pluginVersion);
    const auto& saved = savedMetadata ? *savedMetadata : selected ? selected->metadata : metadata;
    if (!saved.id.empty())
        root->SetAttribute("id", saved.id.c_str());
    if (!saved.author.empty())
        root->SetAttribute("author", saved.author.c_str());
    if (!saved.category.empty())
        root->SetAttribute("category", saved.category.c_str());
    if (!saved.description.empty())
        root->SetAttribute("description", saved.description.c_str());
    if (!saved.tags.empty()) {
        auto* tags = doc.NewElement("TAGS");
        root->InsertEndChild(tags);
        for (const auto& text : saved.tags) {
            auto* tag = doc.NewElement("TAG");
            tag->SetText(text.c_str());
            tags->InsertEndChild(tag);
        }
    }
    if (session) {
        int index = currentProgram;
        if (selected) {
            index = 0;
            const int match = findProgram(selected->identity, selected->metadata.id);
            if (match >= 0)
                index = match;
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
    auto savedMetadata = metadata;
    savedMetadata.id = prepareXml(readFile(path), false).metadata.id;
    if (savedMetadata.id.empty())
        savedMetadata.id = newPresetId();
    if (savedMetadata.id.empty())
        return false;
    const auto xml = serialize(false, nullptr, false, &savedMetadata);
    if (!replaceFile(path, xml))
        return false;
    refreshUnlocked();
    const auto& selected = allPresets[currentProgram];
    if (selected.type == PresetType::User && selected.filename == pathText(path.filename())) {
        metadata = savedMetadata;
        setReference(prepareXml(xml, false));
    }
    return true;
}
bool ProgramManager::saveCurrentUserPreset() {
    std::lock_guard lock(mutex);
    const auto& selected = allPresets[currentProgram];
    if (directory.empty() || selected.type != PresetType::User)
        return false;
    auto savedMetadata = metadata;
    savedMetadata.id = references[currentProgram].metadata.id;
    if (savedMetadata.id.empty())
        savedMetadata.id = newPresetId();
    if (savedMetadata.id.empty())
        return false;
    const auto xml = serialize(false, nullptr, false, &savedMetadata);
    if (!replaceFile(directory / cs01::utf8Path(selected.filename), xml))
        return false;
    const auto saved = prepareXml(xml, false);
    metadata = saved.metadata;
    setReference(saved);
    references[currentProgram] = saved;
    references[currentProgram].identity = selected;
    return true;
}
bool ProgramManager::saveCurrentStateAsNewPreset(std::string_view name) {
    std::lock_guard lock(mutex);
    if (directory.empty() || !isValidPresetName(name))
        return false;
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec)
        return false;
    const auto path = uniquePath(name);
    if (path.empty())
        return false;
    auto savedMetadata = metadata;
    savedMetadata.id = newPresetId();
    if (savedMetadata.id.empty())
        return false;
    const auto xml = serialize(false, nullptr, false, &savedMetadata);
    if (path.empty() || !replaceFile(path, xml, false))
        return false;
    // Refresh around the new identity so even an externally removed source
    // cannot trigger a default-patch reload. Live controls stay untouched.
    allPresets[currentProgram] = {pathText(path.stem()), pathText(path.filename()),
                                  PresetType::User};
    metadata = savedMetadata;
    references[currentProgram] = prepareXml(xml, false);
    references[currentProgram].identity = allPresets[currentProgram];
    refreshUnlocked();
    setReference(prepareXml(xml, false));
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
