#pragma once
#include "IPlug_include_in_plug_hdr.h"

#if defined VST3_API
#include "VST3ParameterChanges.h"
#include "VST3State.h"
namespace cs01 {
// iPlug2 owns parameter IDs and public controller APIs. Keep only the native
// preset-queue and state-stream fixes needed by this plugin's XML state.
class PluginAPI : public iplug::Plugin {
   public:
    using iplug::Plugin::Plugin;
    static_assert(iplug::kBypassParam == vstBypassParam);
    static_assert(iplug::kPresetParam == vstProgramParam);
    static_assert(iplug::kMIDICCParamStartIdx == vstMidiParamStart);
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* stream) override {
        if (!stream)
            return Steinberg::kInvalidArgument;
        bool bypass = false;
        if (!restoreVst3State(
                stream,
                [this](std::string_view bytes) {
                    iplug::IByteChunk chunk;
                    chunk.PutBytes(bytes.data(), static_cast<int>(bytes.size()));
                    return UnserializeState(chunk, 0) >= 0;
                },
                bypass))
            return Steinberg::kResultFalse;
        SetBypassed(bypass);
        iplug::IPlugVST3ControllerBase::UpdateParams(this, bypass);
        OnRestoreState();
        return Steinberg::kResultOk;
    }
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) override {
        if (!data.inputParameterChanges)
            return iplug::Plugin::process(data);
        VST3ParameterChangesView changes(*data.inputParameterChanges, [this](double value) {
            if (NPresets() > 0)
                RestorePreset(static_cast<int>(std::lround(value * (NPresets() - 1))));
        });
        auto filtered = data;
        filtered.inputParameterChanges = &changes;
        return iplug::Plugin::process(filtered);
    }
};
}  // namespace cs01
#else
namespace cs01 {
using PluginAPI = iplug::Plugin;
}
#endif
