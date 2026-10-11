#pragma once
#include "PluginAPI.h"
// iPlug2 generates a format-specific bundle ID; retain JUCE's shared macOS ID.
#ifdef OS_MAC
#undef BUNDLE_ID
#define BUNDLE_ID "org.github.yasuyukibaba.cheapsynth01"
#endif
#include "Parameters.h"
#include "ProgramManager.h"
#include "HostProgramBank.h"
#include <memory>
#include "SynthEngine.h"
#include "DSP/MidiQueue.h"
#include "ISender.h"
#if IPLUG_EDITOR
#include "IControls.h"
#endif

class CS01AudioProcessor final : public cs01::PluginAPI {
   public:
    explicit CS01AudioProcessor(const iplug::InstanceInfo& info);
    bool SerializeState(iplug::IByteChunk& chunk) const override;
    int UnserializeState(const iplug::IByteChunk& chunk, int startPos) override;
    void OnParamChange(int index) override;
    void OnIdle() override;
    void OnRestoreState() override;
#if IPLUG_DSP
    void ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int frames) override;
    void ProcessMidiMsg(const iplug::IMidiMsg& message) override;
    void OnReset() override;
#endif
#if IPLUG_EDITOR
    void OnUIOpen() override;
    void OnUIClose() override;
#endif
   private:
    CS01AudioProcessor(const iplug::InstanceInfo& info, std::vector<PreparedProgram> catalogue);
    enum ControlTag {
        ProgramLabel = 100,
        PresetName,
        Scope,
        Keyboard,
        Resonance,
        ResonanceHigh,
        RenamePreset,
        DeletePreset,
        ResonanceHighLabel
    };
    void syncParameters(bool notifyHost = false);
    void syncHostProgramIndex();
    cs01::ParameterState parameters;
    ProgramManager programs{parameters};
    std::unique_ptr<cs01::HostProgramBank> hostPrograms;
    std::atomic<bool> refreshUI{false};
#if IPLUG_DSP
    cs01::ParameterState renderParameters;
    SynthEngine engine{renderParameters};
    cs01::MidiQueue midi;
    iplug::IBufferSender<1, 64, 512> scope{-100, 512};
    uint64_t appliedStateReset = 0;
    std::atomic<bool> pendingPanelBend{false};
#endif
    std::atomic<bool> monitorEnabled{false};
#if IPLUG_EDITOR
    void layoutEditor(iplug::igraphics::IGraphics* graphics);
    void updateProgramLabel();
    void updateFilterControls();
    void reportPresetError();
    void OnParamChangeUI(int index, iplug::EParamSource source) override;
    WDL_String dialogFile, dialogPath;
#endif
};
