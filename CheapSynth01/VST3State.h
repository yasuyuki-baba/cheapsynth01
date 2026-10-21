#pragma once
#include "pluginterfaces/base/ibstream.h"
#include <array>
#include <string>
#include <string_view>

namespace cs01 {
// State restoration is a non-real-time VST3 API. Bound allocation and accept
// bytes returned by a final short read even if the stream reports EOF as false.
template <typename Restore>
bool restoreVst3State(Steinberg::IBStream* stream, Restore restore, bool& bypass) {
    if (!stream)
        return false;
    constexpr size_t limit = 1024 * 1024;
    std::string bytes;
    std::array<char, 4096> buffer;
    for (;;) {
        Steinberg::int32 count = 0;
        const auto status =
            stream->read(buffer.data(), static_cast<Steinberg::int32>(buffer.size()), &count);
        if (status != Steinberg::kResultOk && status != Steinberg::kResultFalse)
            return false;
        if (count < 0 || count > static_cast<Steinberg::int32>(buffer.size()) ||
            bytes.size() + static_cast<size_t>(count) > limit)
            return false;
        bytes.append(buffer.data(), static_cast<size_t>(count));
        if (count == 0 || status != Steinberg::kResultOk)
            break;
    }
    if (bytes.empty())
        return false;
    std::string_view payload(bytes);
    bool savedBypass = false;
    // Native iPlug2 VST3 state appends a little-endian four-byte bypass value
    // to XML state chunks. Do not strip the index from 12-byte CSHP chunks.
    const bool isHostProgramChunk =
        payload.size() == 12 && payload.substr(0, 8) == std::string_view("CSHP\1\0\0\0", 8);
    if (!isHostProgramChunk && payload.size() >= 4) {
        const auto tail = payload.substr(payload.size() - 4);
        if (tail == std::string_view("\0\0\0\0", 4) || tail == std::string_view("\1\0\0\0", 4)) {
            savedBypass = tail.front() != 0;
            payload.remove_suffix(4);
        }
    }
    if (!restore(payload))
        return false;
    bypass = savedBypass;
    return true;
}
}  // namespace cs01
