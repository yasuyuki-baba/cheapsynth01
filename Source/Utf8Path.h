#pragma once
#include <filesystem>
#include <string>
#include <string_view>
namespace cs01 {
inline std::filesystem::path utf8Path(std::string_view text) {
    std::u8string utf8;
    utf8.reserve(text.size());
    for (unsigned char byte : text)
        utf8.push_back(static_cast<char8_t>(byte));
    return std::filesystem::path(utf8);
}
}  // namespace cs01
