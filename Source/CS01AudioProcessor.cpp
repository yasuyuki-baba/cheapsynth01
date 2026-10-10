#include "CS01AudioProcessor.h"
#include "Utf8Path.h"
#include "IPlug_include_in_plug_src.h"
#include "IPlugPaths.h"
#include <limits>
using namespace iplug;
CS01AudioProcessor::CS01AudioProcessor(const InstanceInfo& info)
    : iplug::Plugin(info, MakeConfig(cs01::parameterCount, 1)) {
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
    WDL_String home;
    UserHomePath(home);
#ifdef OS_MAC
    programs.setUserDirectory(std::filesystem::path(home.Get()) / "Library" /
                              "Application Support" / "CheapSynth01" / "UserPresets");
#elif defined OS_WIN
    const char* roaming = std::getenv("APPDATA");
    if (roaming && *roaming)
        programs.setUserDirectory(cs01::utf8Path(roaming) / "CheapSynth01" / "UserPresets");
#endif
    programs.setCurrentProgram(0);
    syncParameters();
    // A single host preset; the panel manages factory/user XML presets separately.
    MakeDefaultPreset("Default", 1);
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
    for (int i = 0; i < cs01::parameterCount; ++i) {
        GetParam(i)->Set(parameters.get(static_cast<cs01::Param>(i)));
        SendParameterValueFromDelegate(i, GetParam(i)->Value(), false);
        if (notifyHost)
            InformHostOfParamChange(i, GetParam(i)->GetNormalized());
    }
}
bool CS01AudioProcessor::SerializeState(IByteChunk& chunk) const {
    const auto xml = programs.getStateInformation();
    chunk.PutBytes(xml.data(), static_cast<int>(xml.size()));
    return true;
}
int CS01AudioProcessor::UnserializeState(const IByteChunk& chunk, int pos) {
    if (pos < 0 || pos >= chunk.Size())
        return -1;
    if (!programs.setStateInformation({reinterpret_cast<const char*>(chunk.GetData() + pos),
                                       static_cast<size_t>(chunk.Size() - pos)}))
        return -1;
    syncParameters();
#if IPLUG_DSP
    pendingPanic.store(true);
#endif
#if IPLUG_EDITOR
    updateProgramLabel();
#endif
    return chunk.Size();
}
#if IPLUG_DSP
void CS01AudioProcessor::OnReset() {
    engine.prepare(GetSampleRate());
    midi.clear();
    pendingPanic.store(false);
    SetLatency(SynthEngine::latency);
}
void CS01AudioProcessor::ProcessMidiMsg(const IMidiMsg& message) {
    midi.add(message.mOffset, {message.mStatus, message.mData1, message.mData2});
}
void CS01AudioProcessor::ProcessBlock(sample**, sample** outputs, int frames) {
    if (pendingPanic.exchange(false) || midi.takeOverflow())
        engine.panic();
    if (pendingPanelBend.exchange(false) && !midi.containsPitchBend(frames)) {
        const float bend = parameters.get(cs01::Param::PitchBend);
        const int value = 8192 + static_cast<int>(std::lround(bend * (bend >= 0 ? 8191 : 8192)));
        engine.handleMidi(cs01::MidiMessage::pitchWheel(1, value));
    }
    for (int i = 0; i < frames; ++i) {
        while (!midi.empty() && midi.peek().offset <= i) {
            engine.handleMidi(midi.peek().message);
            midi.remove();
        }
        const float output = engine.renderSample();
        for (int channel = 0; channel < NOutChansConnected(); ++channel)
            outputs[channel][i] = output;
    }
    midi.flush(frames);
    // IParam::Set stores an atomic; host/UI notifications use iPlug2's deferred queue.
    const unsigned changed = parameters.takeMidiChanges();
    for (int i = 0; i < cs01::parameterCount; ++i)
        if (changed & (1u << i)) {
            const auto value = parameters.get(static_cast<cs01::Param>(i));
            GetParam(i)->Set(value);
            SendParameterValueFromAPI(i, value, false);
        }
    if (monitorEnabled.load() && NOutChansConnected() > 0)
        scope.ProcessBlock(outputs, frames, Scope, 1);
}
#endif
void CS01AudioProcessor::OnIdle() {
#if IPLUG_DSP
    scope.TransmitData(*this);
#endif
}
