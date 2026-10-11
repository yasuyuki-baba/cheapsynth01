#pragma once
#include "LegacyHostIds.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <cstring>

namespace cs01 {
// Borrowed, read-only views used only inside one SDK process() call. The pinned
// SDK consumes queues sequentially without retaining them; no queue is copied,
// resized, allocated or modified. COM references never transfer stack ownership.
class VST3QueueView final : public Steinberg::Vst::IParamValueQueue {
   public:
    void bind(Steinberg::Vst::IParamValueQueue* queue, Steinberg::Vst::ParamID id) noexcept {
        original = queue;
        internalId = id;
    }
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid,
                                                 void** result) override {
        if (!result)
            return Steinberg::kInvalidArgument;
        *result = nullptr;
        if (!std::memcmp(iid, Steinberg::FUnknown_iid, 16) ||
            !std::memcmp(iid, Steinberg::Vst::IParamValueQueue_iid, 16)) {
            *result = static_cast<Steinberg::Vst::IParamValueQueue*>(this);
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
    Steinberg::Vst::ParamID PLUGIN_API getParameterId() override {
        return internalId;
    }
    Steinberg::int32 PLUGIN_API getPointCount() override {
        return original->getPointCount();
    }
    Steinberg::tresult PLUGIN_API getPoint(Steinberg::int32 index, Steinberg::int32& offset,
                                           Steinberg::Vst::ParamValue& value) override {
        return original->getPoint(index, offset, value);
    }
    Steinberg::tresult PLUGIN_API addPoint(Steinberg::int32, Steinberg::Vst::ParamValue,
                                           Steinberg::int32&) override {
        return Steinberg::kNotImplemented;
    }

   private:
    Steinberg::Vst::IParamValueQueue* original = nullptr;
    Steinberg::Vst::ParamID internalId = 0;
};

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
        const int id = vstInternalId(queue->getParameterId());
        if (id < 0)
            return nullptr;
        if (id == static_cast<int>(internalVstProgram)) {
            Steinberg::int32 offset;
            Steinberg::Vst::ParamValue value;
            const auto count = queue->getPointCount();
            if (count > 0 && queue->getPoint(count - 1, offset, value) == Steinberg::kResultOk &&
                std::isfinite(value) && value >= 0 && value <= 1)
                onPreset(value);
            return nullptr;
        }
        view.bind(queue, static_cast<Steinberg::Vst::ParamID>(id));
        return &view;
    }
    Steinberg::Vst::IParamValueQueue* PLUGIN_API addParameterData(const Steinberg::Vst::ParamID&,
                                                                  Steinberg::int32&) override {
        return nullptr;
    }

   private:
    Steinberg::Vst::IParameterChanges& original;
    PresetChange onPreset;
    VST3QueueView view;
};
}  // namespace cs01
