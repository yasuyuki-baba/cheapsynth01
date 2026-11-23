#include "LinuxSettings.h"
#define NOMINMAX
#include "swell.h"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <string>
#include <system_error>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace cs01 {
namespace {
namespace fs = std::filesystem;

bool absoluteDirectory(const char* value) {
    if (!value || !*value || !fs::path(value).is_absolute())
        return false;
    for (const auto& part : fs::path(value))
        if (part == "..")
            return false;
    return true;
}

[[noreturn]] void fail(const std::string& message, int error) {
    throw std::system_error(error ? error : EIO, std::generic_category(), message);
}

void createPrivateDirectories(const fs::path& directory) {
    fs::path current;
    for (const auto& part : directory) {
        current /= part;
        if (mkdir(current.c_str(), S_IRWXU) == 0)
            continue;
        const int error = errno;
        struct stat status{};
        if (error == EEXIST && stat(current.c_str(), &status) == 0 && S_ISDIR(status.st_mode))
            continue;
        fail("Cannot create settings directory '" + current.string() + "'",
             error == EEXIST ? ENOTDIR : error);
    }
}

std::string readSettings(const fs::path& path) {
    struct stat status{};
    if (stat(path.c_str(), &status) != 0) {
        if (errno == ENOENT)
            return {};
        fail("Cannot save settings '" + path.string() + "'", errno);
    }
    if (!S_ISREG(status.st_mode))
        fail("Cannot save settings '" + path.string() + "'", EINVAL);
    std::ifstream input(path, std::ios::binary);
    if (!input)
        fail("Cannot read settings '" + path.string() + "'", errno);
    std::string content{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (input.bad())
        fail("Cannot read settings '" + path.string() + "'", EIO);
    return content;
}

struct Field {
    const char* section;
    const char* key;
    const char* fallback;
    bool number;
};

// Match the Linux standalone host's defaults and its GetPrivateProfileInt
// normalization so its initial UpdateINI does not need to rewrite this file.
constexpr Field fields[] = {
    {"audio", "driver", "0", true},        {"audio", "indev", "default", false},
    {"audio", "outdev", "default", false}, {"audio", "in1", "1", true},
    {"audio", "in2", "2", true},           {"audio", "out1", "1", true},
    {"audio", "out2", "2", true},          {"audio", "buffer", "512", true},
    {"audio", "sr", "44100", true},        {"midi", "indev", "off", false},
    {"midi", "outdev", "off", false},      {"midi", "inchan", "0", true},
    {"midi", "outchan", "0", true}};

void saveChecked(const fs::path& path, const std::string& content) {
    const auto temporary = path.parent_path() / ("." + path.filename().string() + ".new");
    // Do not follow or truncate a pre-existing temporary path. In particular,
    // an unexpected symlink must never be renamed over the real settings.
    const int descriptor = open(
        temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
    if (descriptor < 0)
        fail("Cannot save settings '" + path.string() + "'", errno);
    FILE* output = fdopen(descriptor, "wb");
    if (!output) {
        const int error = errno;
        close(descriptor);
        std::remove(temporary.c_str());
        fail("Cannot save settings '" + path.string() + "'", error);
    }
    int error = fchmod(descriptor, S_IRUSR | S_IWUSR) == 0 ? 0 : errno;
    if (!error && std::fwrite(content.data(), 1, content.size(), output) != content.size())
        error = errno ? errno : EIO;
    if (std::fflush(output) != 0 && !error)
        error = errno ? errno : EIO;
    if (std::fclose(output) != 0 && !error)
        error = errno ? errno : EIO;
    if (!error && std::rename(temporary.c_str(), path.c_str()) != 0)
        error = errno;
    if (error) {
        std::remove(temporary.c_str());
        fail("Cannot save settings '" + path.string() + "'", error);
    }
}
}  // namespace

std::filesystem::path prepareLinuxSettings(const char* xdgConfig, const char* home,
                                           std::string_view bundleName, bool noIO) {
    fs::path directory;
    if (absoluteDirectory(xdgConfig))
        directory = fs::path(xdgConfig) / bundleName;
    else if (absoluteDirectory(home))
        directory = fs::path(home) / ".config" / bundleName;
    else
        throw std::runtime_error(
            "Cannot resolve settings directory: absolute XDG_CONFIG_HOME or HOME is required");
    createPrivateDirectories(directory);
    const auto path = directory / "settings.ini";
    auto content = readSettings(path);
    std::string canonical;
    std::string section;
    bool incomplete = false;
    for (const auto& field : fields) {
        std::array<char, 100> value{};
        const auto count = GetPrivateProfileString(field.section, field.key, "", value.data(),
                                                   static_cast<int>(value.size()), path.c_str());
        if (count >= value.size() - 1)
            throw std::runtime_error("Setting value is too long: " + std::string(field.key));
        std::string normalized;
        if (field.number) {
            const auto number = GetPrivateProfileInt(field.section, field.key,
                                                     std::stoi(field.fallback), path.c_str());
            if (!noIO && std::string_view(field.key) == "driver" && number != 0 && number != 1)
                throw std::runtime_error("Could not initialize audio driver " +
                                         std::to_string(number));
            normalized = std::to_string(static_cast<uint32_t>(number));
        } else {
            // An empty device value is meaningful; read again with the actual
            // default so that only absent keys acquire a fallback.
            GetPrivateProfileString(field.section, field.key, field.fallback, value.data(),
                                    static_cast<int>(value.size()), path.c_str());
            normalized = value.data();
        }
        incomplete = incomplete || normalized != value.data();
        // Numeric empty values and missing string defaults must be materialized.
        if (!field.number && count == 0 && !normalized.empty())
            incomplete = true;
        if (section != field.section) {
            section = field.section;
            canonical += "\n[" + section + "]\n";
        }
        canonical += std::string(field.key) + "=" + normalized + "\n";
    }
    if (incomplete || content.empty()) {
        // SWELL merges repeated sections and the last key wins. Appending keeps
        // unknown keys, comments and [.swell] preferences intact.
        if (!content.empty() && content.back() != '\n')
            content += '\n';
        content += canonical;
    }
    saveChecked(path, content);
    return path;
}
}  // namespace cs01
