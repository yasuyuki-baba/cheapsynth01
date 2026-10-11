#pragma once
#include "Parameters.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include <cstring>

namespace cs01 {
// Constants checked against the pinned iPlug2 wrapper by PluginAPI.h. All
// public IDs are the SDK's standard indices, without JUCE hashes or aliases.
inline constexpr Steinberg::Vst::ParamID vstBypassParam = 65536;
inline constexpr Steinberg::Vst::ParamID vstProgramParam = 65537;
inline constexpr Steinberg::Vst::ParamID vstMidiParamStart = 65538;
inline constexpr unsigned vstMidiParamCount = 16 * Steinberg::Vst::kCountCtrlNumber;
constexpr bool validVstParameterId(Steinberg::Vst::ParamID id) noexcept {
    return id < parameterCount || id == vstBypassParam || id == vstProgramParam ||
           (id >= vstMidiParamStart && id - vstMidiParamStart < vstMidiParamCount);
}

// Borrow the original queues without changing their IDs, points or offsets.
// Intercept program selection because the pinned SDK's process queue does not
// handle it. Unknown IDs must not accidentally become MIDI controllers.
template <typename PresetChange>
class VST3ParameterChangesView final : public Steinberg::Vst::IParameterChanges {
   public:
    VST3ParameterChangesView(Steinberg::Vst::IParameterChanges& changes, PresetChange preset)
        : original(changes), onPreset(preset) {}
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid,
                                                 void** result) override {
        if (!result)
            return Steinberg::kInvalidArgument;
        *result = nullptr;
        if (!std::memcmp(iid, Steinberg::FUnknown_iid, 16) ||
            !std::memcmp(iid, Steinberg::Vst::IParameterChanges_iid, 16)) {
            *result = static_cast<Steinberg::Vst::IParameterChanges*>(this);
            return Steinberg::kResultOk;
        }
        return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef() override {
        return 1;
    }
    Steinberg::uint32 PLUGIN_API release() override {
        return 1;
    }
    Steinberg::int32 PLUGIN_API getParameterCount() override {
        return original.getParameterCount();
    }
    Steinberg::Vst::IParamValueQueue* PLUGIN_API getParameterData(Steinberg::int32 index) override {
        auto* queue = original.getParameterData(index);
        if (!queue)
            return nullptr;
        const auto id = queue->getParameterId();
        if (!validVstParameterId(id))
            return nullptr;
        if (id == vstProgramParam) {
            Steinberg::int32 offset;
            Steinberg::Vst::ParamValue value;
            const auto count = queue->getPointCount();
            if (count > 0 && queue->getPoint(count - 1, offset, value) == Steinberg::kResultOk &&
                std::isfinite(value) && value >= 0 && value <= 1)
                onPreset(value);
            return nullptr;
        }
        return queue;
    }
    Steinberg::Vst::IParamValueQueue* PLUGIN_API addParameterData(const Steinberg::Vst::ParamID&,
                                                                  Steinberg::int32&) override {
        return nullptr;
    }

   private:
    Steinberg::Vst::IParameterChanges& original;
    PresetChange onPreset;
};
}  // namespace cs01
