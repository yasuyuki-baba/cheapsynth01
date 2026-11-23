#pragma once
#include <filesystem>
#include <string_view>

namespace cs01 {
// Prepare a complete settings file before entering the unmodified SDK host.
// Throws std::system_error/std::runtime_error before audio or GUI initialization.
std::filesystem::path prepareLinuxSettings(const char* xdgConfig, const char* home,
                                           std::string_view bundleName, bool noIO);
}  // namespace cs01
