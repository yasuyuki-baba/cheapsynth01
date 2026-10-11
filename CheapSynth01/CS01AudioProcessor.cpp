#include "CS01AudioProcessor.h"
#include "Utf8Path.h"
#include "IPlug_include_in_plug_src.h"
#include "IPlugPaths.h"
#include <limits>
#include <cstring>
using namespace iplug;
namespace {
std::filesystem::path userPresetDirectory() {
#ifdef OS_MAC
    WDL_String home;
    UserHomePath(home);
    return std::filesystem::path(home.Get()) / "Library" / "Application Support" / "CheapSynth01" /
           "UserPresets";
#elif defined OS_WIN
    const char* roaming = std::getenv("APPDATA");
    if (roaming && *roaming)
        return cs01::utf8Path(roaming) / "CheapSynth01" / "UserPresets";
#endif
    return {};
}
std::vector<PreparedProgram> initialHostCatalogue() {
    cs01::ParameterState parameters;
    ProgramManager programs(parameters);
    programs.setUserDirectory(userPresetDirectory());
    return programs.preparePrograms();
}
}  // namespace
CS01AudioProcessor::CS01AudioProcessor(const InstanceInfo& info)
    : CS01AudioProcessor(info, initialHostCatalogue()) {}
CS01AudioProcessor::CS01AudioProcessor(const InstanceInfo& info,
                                       std::vector<PreparedProgram> catalogue)
    : cs01::PluginAPI(info, MakeConfig(cs01::parameterCount, static_cast<int>(catalogue.size()))),
      hostPrograms(std::make_unique<cs01::HostProgramBank>(std::move(catalogue))) {
    for (int i = 0; i < cs01::parameterCount; ++i) {
        const auto& d = cs01::parameterDefinitions[i];
        const int flags = (i == static_cast<int>(cs01::Param::PitchBend) ||
                           i == static_cast<int>(cs01::Param::ModDepth))
                              ? IParam::kFlagCannotAutomate
                              : IParam::kFlagsNone;
        if (d.integer)
            GetParam(i)->InitInt(d.name.data(), static_cast<int>(d.initial),
                                 static_cast<int>(d.minimum), static_cast<int>(d.maximum),
                                 d.unit.data(), flags);
        else
            GetParam(i)->InitDouble(d.name.data(), d.initial, d.minimum, d.maximum, d.step,
                                    d.unit.data(), flags, "", IParam::ShapePowCurve(1.0 / d.skew));
    }
    GetParam(static_cast<int>(cs01::Param::WaveType))
        ->InitEnum("Wave", 1, {"Triangle", "Sawtooth", "Square", "Pulse", "PWM"});
    GetParam(static_cast<int>(cs01::Param::Feet))
        ->InitEnum("Feet", 2, {"32'", "16'", "8'", "4'", "WN"});
    GetParam(static_cast<int>(cs01::Param::LfoTarget))->InitEnum("LFO Target", 0, {"VCO", "VCF"});
    GetParam(static_cast<int>(cs01::Param::FilterType))
        ->InitEnum("Filter", 0, {"Original", "Modern"});
    programs.setBeforePatchCommit([this] { hostPrograms->cancel(); });
    programs.setUserDirectory(userPresetDirectory());
    programs.setCurrentProgram(0);
    syncParameters();
#if IPLUG_DSP
    parameters.tryCopyTo(renderParameters);
#endif
    // Host metadata is immutable for the lifetime of this instance. A host may
    // cache names/count; panel saves/renames are available here after reopening.
    for (size_t i = 0; i < hostPrograms->programs().size(); ++i) {
        const auto& program = hostPrograms->programs()[i];
        auto* preset = GetPreset(static_cast<int>(i));
        preset->mInitialized = true;
        // Cut at a UTF-8 boundary when the SDK's fixed name buffer is shorter.
        size_t length = std::min(program.identity.name.size(), sizeof(preset->mName) - 1);
        if (length < program.identity.name.size())
            while (length &&
                   (static_cast<unsigned char>(program.identity.name[length]) & 0xc0) == 0x80)
                --length;
        std::memcpy(preset->mName, program.identity.name.data(), length);
        preset->mName[length] = 0;
        const auto bytes = cs01::HostProgramBank::requestChunk(static_cast<unsigned>(i));
        preset->mChunk.PutBytes(bytes.data(), static_cast<int>(bytes.size()));
    }
#if IPLUG_EDITOR
    mMakeGraphicsFunc = [this]() {
        return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                            GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
    };
    mLayoutFunc = [this](igraphics::IGraphics* graphics) { layoutEditor(graphics); };
#endif
}
void CS01AudioProcessor::OnParamChange(int index) {
    if (index < 0 || index >= cs01::parameterCount)
        return;
    parameters.set(static_cast<cs01::Param>(index), static_cast<float>(GetParam(index)->Value()));
#if IPLUG_DSP
    if (index == static_cast<int>(cs01::Param::PitchBend))
        pendingPanelBend.store(true);
#endif
}
void CS01AudioProcessor::syncParameters(bool notifyHost) {
    if (notifyHost && hostPrograms) {
        syncHostProgramIndex();
    }
    const auto values = parameters.snapshot();
    for (int i = 0; i < cs01::parameterCount; ++i) {
        GetParam(i)->Set(values[i]);
    }
    if (notifyHost) {
        DirtyParametersFromUI();
        InformHostOfPresetChange();
    }
    refreshUI.store(true, std::memory_order_release);
}
void CS01AudioProcessor::syncHostProgramIndex() {
    if (!hostPrograms)
        return;
    const auto selected = programs.currentProgramIdentity();
    int hostIndex = 0;
    for (size_t i = 0; i < hostPrograms->programs().size(); ++i) {
        const auto& identity = hostPrograms->programs()[i].identity;
        if (identity.filename == selected.filename && identity.type == selected.type) {
            hostIndex = static_cast<int>(i);
            break;
        }
    }
    SetCurrentPresetIdx(hostIndex);
}
bool CS01AudioProcessor::SerializeState(IByteChunk& chunk) const {
    const auto xml =
        hostPrograms ? hostPrograms->serializeSession(programs) : programs.getStateInformation();
    chunk.PutBytes(xml.data(), static_cast<int>(xml.size()));
    return true;
}
int CS01AudioProcessor::UnserializeState(const IByteChunk& chunk, int pos) {
    if (pos < 0 || pos >= chunk.Size())
        return -1;
    const std::string_view bytes{reinterpret_cast<const char*>(chunk.GetData() + pos),
                                 static_cast<size_t>(chunk.Size() - pos)};
    if (bytes.size() == 12 && bytes.substr(0, 4) == "CSHP") {
        if (!hostPrograms || !hostPrograms->requestChunk(bytes))
            return -1;
        return chunk.Size();
    }
    if (!programs.setStateInformation(bytes))
        return -1;
    syncHostProgramIndex();
    syncParameters();
    return chunk.Size();
}
#if IPLUG_DSP
void CS01AudioProcessor::OnReset() {
    parameters.tryCopyTo(renderParameters);
    engine.prepare(GetSampleRate());
    midi.clear();
    uiNotes.resetAudio();
    uiNotes.publishExternal(engine.externalNotes());
    appliedStateReset = renderParameters.stateResetSerial();
    SetLatency(SynthEngine::latency);
}
void CS01AudioProcessor::ProcessMidiMsg(const IMidiMsg& message) {
    midi.add(message.mOffset, {message.mStatus, message.mData1, message.mData2});
}
void CS01AudioProcessor::ProcessBlock(sample**, sample** outputs, int frames) {
    const bool programApplied = hostPrograms && hostPrograms->applyPending(parameters) >= 0;
    const bool copied = parameters.tryCopyTo(renderParameters);
    if (programApplied) {
        if (copied)
            for (int i = 0; i < cs01::parameterCount; ++i)
                GetParam(i)->Set(renderParameters.get(static_cast<cs01::Param>(i)));
        refreshUI.store(true, std::memory_order_release);
    }
    const auto resetSerial = renderParameters.stateResetSerial();
    const bool restored = resetSerial != appliedStateReset;
    appliedStateReset = resetSerial;
    const bool overflowed = midi.takeOverflow();
    if (restored || overflowed)
        engine.panic();
    if (pendingPanelBend.exchange(false) && !midi.containsPitchBend(frames)) {
        const float bend = parameters.get(cs01::Param::PitchBend);
        renderParameters.set(cs01::Param::PitchBend, bend);
        const int value = 8192 + static_cast<int>(std::lround(bend * (bend >= 0 ? 8191 : 8192)));
        engine.handleMidi(cs01::MidiMessage::pitchWheel(1, value));
    }
    uiNotes.consume([this](auto message) { engine.handleUiMidi(message); });
    // Even an empty callback must apply events due at its block boundary.
    midi.consumeThrough(0, [this](auto message) { engine.handleMidi(message); });
    for (int i = 0; i < frames; ++i) {
        midi.consumeThrough(i, [this](auto message) { engine.handleMidi(message); });
        const float output = engine.renderSample();
        for (int channel = 0; channel < NOutChansConnected(); ++channel)
            outputs[channel][i] = output;
    }
    midi.flush(frames);
    uiNotes.publishExternal(engine.externalNotes());
    // Publish parameter values atomically; the idle callback refreshes controls.
    const unsigned changed = renderParameters.takeMidiChanges();
    for (int i = 0; i < cs01::parameterCount; ++i)
        if (changed & (1u << i)) {
            const auto value = renderParameters.get(static_cast<cs01::Param>(i));
            parameters.set(static_cast<cs01::Param>(i), value);
            GetParam(i)->Set(value);
            refreshUI.store(true, std::memory_order_release);
        }
    if (monitorEnabled.load() && NOutChansConnected() > 0)
        scope.ProcessBlock(outputs, frames, Scope, 1);
}
#endif
void CS01AudioProcessor::OnRestoreState() {
    // SDK default touches controls directly. Hosts can restore from an audio
    // callback, so only schedule the main-thread idle update here.
    refreshUI.store(true, std::memory_order_release);
}
void CS01AudioProcessor::OnIdle() {
#if IPLUG_EDITOR
    updateKeyboardInput();
#endif
    if (refreshUI.exchange(false, std::memory_order_acquire)) {
        // Control/API values can be updated during rendering; use one completed
        // patch for both the editor and the separate VST3 controller cache.
        const auto values = parameters.snapshot();
        for (int i = 0; i < NParams(); ++i)
            GetParam(i)->Set(values[i]);
        if (hostPrograms)
            if (const auto* selected = hostPrograms->selected())
                programs.selectProgramIdentity(selected->identity);
#if defined VST3_API
        // VST3 keeps a separate controller parameter cache; changing IParam
        // alone leaves host queries stale after a deferred preset or MIDI CC.
        for (int i = 0; i < NParams(); ++i)
            IPlugVST3ControllerBase::SetVST3ParamNormalized(i, GetParam(i)->GetNormalized());
        if (NPresets() > 1)
            IPlugVST3ControllerBase::SetVST3ParamNormalized(
                kPresetParam, static_cast<double>(GetCurrentPresetIdx()) / (NPresets() - 1));
#endif
        SendCurrentParamValuesFromDelegate();
#if IPLUG_EDITOR
        updateProgramLabel();
        updateFilterControls();
#endif
    }
#if IPLUG_DSP
    scope.TransmitData(*this);
#endif
}
