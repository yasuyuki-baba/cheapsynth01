#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace cs01 {
// Order is permanent: iPlug2 uses these indices as host parameter IDs.
enum class Param : int {
    WaveType,
    Feet,
    PwmSpeed,
    Pitch,
    Glissando,
    Cutoff,
    Resonance,
    VcfEgDepth,
    VcaEgDepth,
    Attack,
    Decay,
    Sustain,
    Release,
    LfoSpeed,
    LfoTarget,
    ModDepth,
    PitchBend,
    BreathVcf,
    BreathVca,
    BendUp,
    BendDown,
    Volume,
    BreathInput,
    FilterType,
    Count
};
constexpr int parameterCount = static_cast<int>(Param::Count);
struct ParameterDefinition {
    std::string_view id, name, unit;
    float minimum, maximum, step, skew, initial;
    bool integer = false;
    bool transient = false;
};
inline constexpr std::array<ParameterDefinition, parameterCount> parameterDefinitions{
    {{"WAVE_TYPE", "Wave", "", 0, 4, 1, 1, 1, true},
     {"FEET", "Feet", "", 0, 4, 1, 1, 2, true},
     {"PWM_SPEED", "PWM Speed", "Hz", .6f, 12, .01f, .25f, 2},
     {"PITCH", "Pitch", "st", -1, 1, .001f, 1, 0},
     {"GLISSANDO", "Glissando", "s/st", 0, .208f, .001f, .5f, 0},
     {"CUTOFF", "Cutoff", "Hz", 20, 20000, 1, .3f, 20000},
     {"RESONANCE", "Resonance", "", 0, 1, .001f, 1, .2f},
     {"VCF_EG_DEPTH", "VCF EG Depth", "", 0, 1, .001f, 1, 0},
     {"VCA_EG_DEPTH", "VCA EG Depth", "", 0, 1, .001f, 1, 1},
     {"ATTACK", "Attack", "s", .001f, 2, .001f, .3f, .1f},
     {"DECAY", "Decay", "s", .001f, 2, .001f, .3f, .1f},
     {"SUSTAIN", "Sustain", "", 0, 1, .001f, 1, .8f},
     {"RELEASE", "Release", "s", .001f, 2, .001f, .3f, .1f},
     {"LFO_SPEED", "LFO Speed", "Hz", .8f, 21, .01f, .3f, 5},
     {"LFO_TARGET", "LFO Target", "", 0, 1, 1, 1, 0, true},
     {"MOD_DEPTH", "Modulation", "", 0, 1, .001f, 1, 0, false, true},
     {"PITCH_BEND", "Pitch Bend", "", -1, 1, .001f, 1, 0, false, true},
     {"BREATH_VCF", "Breath VCF", "", 0, 1, .001f, 1, 0},
     {"BREATH_VCA", "Breath VCA", "", 0, 1, .001f, 1, 0},
     {"PITCH_BEND_UP_RANGE", "Bend Up", "st", 0, 12, 1, 1, 12, true},
     {"PITCH_BEND_DOWN_RANGE", "Bend Down", "st", 0, 12, 1, 1, 0, true},
     {"VOLUME", "Volume", "", 0, 1, .001f, 1, .7f},
     {"BREATH_INPUT", "Breath", "", 0, 1, .001f, 1, 0, false, true},
     {"FILTER_TYPE", "Filter", "", 0, 1, 1, 1, 0, true}}};
inline const ParameterDefinition& definition(Param id) {
    return parameterDefinitions[static_cast<std::size_t>(id)];
}
inline float fromNormalized(Param id, float value) {
    const auto& d = definition(id);
    float result =
        d.minimum + (d.maximum - d.minimum) * std::pow(std::clamp(value, 0.f, 1.f), 1.f / d.skew);
    if (d.integer)
        result = std::round(result);
    return result;
}
inline float toNormalized(Param id, float value) {
    const auto& d = definition(id);
    return std::pow(std::clamp((value - d.minimum) / (d.maximum - d.minimum), 0.f, 1.f), d.skew);
}
class ParameterState {
   public:
    ParameterState() {
        for (int i = 0; i < parameterCount; ++i)
            values[i].store(parameterDefinitions[i].initial);
    }
    float get(Param id) const {
        return values[static_cast<int>(id)].load(std::memory_order_relaxed);
    }
    void set(Param id, float value, bool midi = false) {
        if (!std::isfinite(value))
            return;
        const auto& d = definition(id);
        value = std::clamp(value, d.minimum, d.maximum);
        if (d.integer)
            value = std::round(value);
        values[static_cast<int>(id)].store(value, std::memory_order_relaxed);
        if (midi)
            midiChanges.fetch_or(1u << static_cast<int>(id), std::memory_order_release);
    }
    void setNormalized(Param id, float value, bool midi = false) {
        set(id, fromNormalized(id, value), midi);
    }
    unsigned takeMidiChanges() {
        return midiChanges.exchange(0, std::memory_order_acquire);
    }

   private:
    static_assert(std::atomic<float>::is_always_lock_free);
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    std::array<std::atomic<float>, parameterCount> values;
    std::atomic<unsigned> midiChanges{0};
};
}  // namespace cs01
namespace ParameterIds {
using cs01::Param;
inline constexpr auto waveType = Param::WaveType, feet = Param::Feet, pwmSpeed = Param::PwmSpeed,
                      pitch = Param::Pitch, glissando = Param::Glissando, cutoff = Param::Cutoff,
                      resonance = Param::Resonance, vcfEgDepth = Param::VcfEgDepth,
                      vcaEgDepth = Param::VcaEgDepth, attack = Param::Attack, decay = Param::Decay,
                      sustain = Param::Sustain, release = Param::Release,
                      lfoSpeed = Param::LfoSpeed, lfoTarget = Param::LfoTarget,
                      modDepth = Param::ModDepth, pitchBend = Param::PitchBend,
                      breathVcf = Param::BreathVcf, breathVca = Param::BreathVca,
                      pitchBendUpRange = Param::BendUp, pitchBendDownRange = Param::BendDown,
                      volume = Param::Volume, breathInput = Param::BreathInput,
                      filterType = Param::FilterType;
}  // namespace ParameterIds
