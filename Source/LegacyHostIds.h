#pragma once
#include "Parameters.h"
#include <cstdint>

namespace cs01 {
// Frozen JUCE 9.0.3/main 32b91c8 IDs, not freshly generated plugin identities.
inline constexpr std::array<uint32_t, 24> legacyParameterIds{
    65448128u,   2153902u,    855387598u,  76139296u,   500012252u,  1999233005u,
    796563502u,  814297868u,  1796865393u, 1941037640u, 64921498u,   1013575171u,
    1808577511u, 1707273597u, 1401039611u, 1513434886u, 215084792u,  2054883034u,
    2054883029u, 1853466592u, 172956391u,  389929754u,  1193457419u, 1292594081u};
static_assert(parameterCount == legacyParameterIds.size(),
              "Append an explicit stable host ID for new parameters");
inline constexpr uint32_t legacyVstProgram = 0x70727374u;
inline constexpr uint32_t legacyVstBypass = 0x62797073u;
inline constexpr uint32_t legacyVstMidiStart = 0x6d636d00u;
inline constexpr uint32_t internalVstBypass = 65536;
inline constexpr uint32_t internalVstProgram = 65537;
inline constexpr uint32_t internalVstMidiStart = 65538;
inline constexpr uint32_t vstMidiCount = 16 * 130;
inline constexpr uint32_t unknownHostId = 0xffffffffu;

constexpr uint32_t vstHostId(uint32_t internal) noexcept {
    if (internal < legacyParameterIds.size())
        return legacyParameterIds[internal];
    if (internal == internalVstProgram)
        return legacyVstProgram;
    if (internal == internalVstBypass)
        return legacyVstBypass;
    if (internal >= internalVstMidiStart && internal - internalVstMidiStart < vstMidiCount)
        return legacyVstMidiStart + internal - internalVstMidiStart;
    return unknownHostId;
}
constexpr int vstInternalId(uint32_t host) noexcept {
    for (size_t i = 0; i < legacyParameterIds.size(); ++i)
        if (legacyParameterIds[i] == host)
            return static_cast<int>(i);
    if (host == legacyVstProgram)
        return internalVstProgram;
    if (host == legacyVstBypass)
        return internalVstBypass;
    if (host >= legacyVstMidiStart && host - legacyVstMidiStart < vstMidiCount)
        return static_cast<int>(internalVstMidiStart + host - legacyVstMidiStart);
    // Accept early migration's index-based sessions as aliases. Only legacy IDs
    // are advertised; unknown IDs must never become accidental MIDI CC events.
    if (host < legacyParameterIds.size() || host == internalVstBypass ||
        host == internalVstProgram ||
        (host >= internalVstMidiStart && host - internalVstMidiStart < vstMidiCount))
        return static_cast<int>(host);
    return -1;
}
}  // namespace cs01
