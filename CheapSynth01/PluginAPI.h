#pragma once
#include "IPlug_include_in_plug_hdr.h"

#if defined VST3_API
#include "VST3ParameterChanges.h"
#include "VST3State.h"
namespace cs01 {
// Keep iPlug2's controller/processor indices internal. Translate every public
// VST3 parameter boundary, preserving JUCE IDs without a mutable SDK patch.
class PluginAPI : public iplug::Plugin {
   public:
    using iplug::Plugin::Plugin;
    static_assert(iplug::kBypassParam == internalVstBypass);
    static_assert(iplug::kPresetParam == internalVstProgram);
    static_assert(iplug::kMIDICCParamStartIdx == internalVstMidiStart);
    Steinberg::tresult PLUGIN_API getParameterInfo(Steinberg::int32 index,
                                                   Steinberg::Vst::ParameterInfo& info) override {
        const auto result = iplug::Plugin::getParameterInfo(index, info);
        if (result == Steinberg::kResultOk)
            info.id = vstHostId(info.id);
        return result;
    }
    Steinberg::Vst::ParamValue PLUGIN_API getParamNormalized(Steinberg::Vst::ParamID id) override {
        const int index = vstInternalId(id);
        return index < 0 ? 0 : iplug::Plugin::getParamNormalized(index);
    }
    Steinberg::tresult PLUGIN_API setParamNormalized(Steinberg::Vst::ParamID id,
                                                     Steinberg::Vst::ParamValue value) override {
        const int index = vstInternalId(id);
        return index < 0 ? Steinberg::kInvalidArgument
                         : iplug::Plugin::setParamNormalized(index, value);
    }
    Steinberg::tresult PLUGIN_API getParamStringByValue(Steinberg::Vst::ParamID id,
                                                        Steinberg::Vst::ParamValue value,
                                                        Steinberg::Vst::String128 text) override {
        const int index = vstInternalId(id);
        return index < 0 ? Steinberg::kInvalidArgument
                         : iplug::Plugin::getParamStringByValue(index, value, text);
    }
    Steinberg::tresult PLUGIN_API
    getParamValueByString(Steinberg::Vst::ParamID id, Steinberg::Vst::TChar* text,
                          Steinberg::Vst::ParamValue& value) override {
        const int index = vstInternalId(id);
        return index < 0 ? Steinberg::kInvalidArgument
                         : iplug::Plugin::getParamValueByString(index, text, value);
    }
    Steinberg::Vst::ParamValue PLUGIN_API
    normalizedParamToPlain(Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue value) override {
        const int index = vstInternalId(id);
        return index < 0 ? 0 : iplug::Plugin::normalizedParamToPlain(index, value);
    }
    Steinberg::Vst::ParamValue PLUGIN_API
    plainParamToNormalized(Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue value) override {
        const int index = vstInternalId(id);
        return index < 0 ? 0 : iplug::Plugin::plainParamToNormalized(index, value);
    }
    void BeginInformHostOfParamChange(int index) override {
        beginEdit(vstHostId(index));
    }
    void InformHostOfParamChange(int index, double value) override {
        performEdit(vstHostId(index), value);
    }
    void EndInformHostOfParamChange(int index) override {
        endEdit(vstHostId(index));
    }
    Steinberg::tresult PLUGIN_API getMidiControllerAssignment(
        Steinberg::int32 bus, Steinberg::int16 channel, Steinberg::Vst::CtrlNumber cc,
        Steinberg::Vst::ParamID& id) override {
        const auto result = iplug::Plugin::getMidiControllerAssignment(bus, channel, cc, id);
        if (result == Steinberg::kResultOk)
            id = vstHostId(id);
        return result;
    }
    Steinberg::tresult PLUGIN_API
    getProgramListInfo(Steinberg::int32 index, Steinberg::Vst::ProgramListInfo& info) override {
        const auto result = iplug::Plugin::getProgramListInfo(index, info);
        if (result == Steinberg::kResultOk)
            info.id = legacyVstProgram;
        return result;
    }
    Steinberg::tresult PLUGIN_API getUnitInfo(Steinberg::int32 index,
                                              Steinberg::Vst::UnitInfo& info) override {
        const auto result = iplug::Plugin::getUnitInfo(index, info);
        if (result == Steinberg::kResultOk &&
            info.programListId == static_cast<int>(internalVstProgram))
            info.programListId = legacyVstProgram;
        return result;
    }
    Steinberg::tresult PLUGIN_API getProgramName(Steinberg::Vst::ProgramListID id,
                                                 Steinberg::int32 index,
                                                 Steinberg::Vst::String128 name) override {
        if (vstInternalId(static_cast<uint32_t>(id)) != static_cast<int>(internalVstProgram) ||
            index < 0 || index >= NPresets())
            return Steinberg::kInvalidArgument;
        return iplug::Plugin::getProgramName(internalVstProgram, index, name);
    }
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
        auto mapped = data;
        mapped.inputParameterChanges = &changes;
        return iplug::Plugin::process(mapped);
    }
};
}  // namespace cs01
#else
namespace cs01 {
using PluginAPI = iplug::Plugin;
}
#endif
