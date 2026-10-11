#include "SynthEngine.h"
SynthEngine::SynthEngine(cs01::ParameterState& p)
    : parameters(p), vco(p), eg(p), lfo(p), midi(p), original(p), modern(p), vca(p) {
    midi.setSoundGenerator(vco.getSoundGenerator());
    midi.setEGProcessor(&eg);
}
void SynthEngine::prepare(double rate) {
    noteOwnership.reset();
    if (!std::isfinite(rate) || rate < 8000 || rate > 384000)
        rate = 44100.0;  // Protect host reset callbacks from invalid/uninitialized rates.
    const double internal = rate * cs01::OutputConverter::factor;
    vco.prepare(internal);
    eg.prepare(internal);
    lfo.prepare(internal);
    original.prepare(internal);
    modern.prepare(internal);
    vca.prepare(internal);
    midi.setSoundGenerator(vco.getSoundGenerator());
    midi.prepare(internal);
    converter.reset();
}
void SynthEngine::panic() {
    noteOwnership.reset();
    midi.reset();
    original.reset();
    modern.reset();
    vca.reset();
    converter.reset();
}
void SynthEngine::handleMidi(const cs01::MidiMessage& message) {
    noteOwnership.process(message, false, [this](auto event) { applyMidi(event); });
}
void SynthEngine::handleUiMidi(const cs01::MidiMessage& message) {
    noteOwnership.process(message, true, [this](auto event) { applyMidi(event); });
}
void SynthEngine::applyMidi(const cs01::MidiMessage& message) {
    vco.applyPendingGeneratorChange();
    midi.setSoundGenerator(vco.getSoundGenerator());
    midi.handleMidiEvent(message);
    if (message.isAllSoundOff())
        panic();
}
float SynthEngine::renderSample() {
    vco.getSoundGenerator()->setReleaseSamplesRemaining(
        static_cast<int>(eg.getReleaseSamplesRemaining()));
    vco.applyPendingGeneratorChange();
    midi.setSoundGenerator(vco.getSoundGenerator());
    float output = 0;
    for (int i = 0; i < cs01::OutputConverter::factor; ++i) {
        const float modulation = lfo.processSample();
        const bool pitchTarget = parameters.get(ParameterIds::lfoTarget) == 0;
        const float envelope = eg.processSample();
        vco.getSoundGenerator()->setReleaseSamplesRemaining(
            static_cast<int>(eg.getReleaseSamplesRemaining()));
        float audio = vco.processSample(pitchTarget ? modulation : 0);
        if (parameters.get(ParameterIds::filterType) == 0)
            audio = original.processSample(audio, envelope, pitchTarget ? 0 : modulation);
        else
            audio = modern.processSample(audio, envelope, pitchTarget ? 0 : modulation);
        converter.push(vca.processSample(audio, envelope, eg.getNoteGate()));
        // Sample the internal clock at multiples of four. FIR delay is exactly 16 frames.
        if (i == 0)
            output = converter.output();
    }
    return output;
}
