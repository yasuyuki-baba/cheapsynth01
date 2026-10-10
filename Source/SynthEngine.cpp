#include "SynthEngine.h"
SynthEngine::SynthEngine(cs01::ParameterState& p)
    : parameters(p), vco(p), eg(p), lfo(p), midi(p), original(p), modern(p), vca(p) {
    midi.setSoundGenerator(vco.getSoundGenerator());
    midi.setEGProcessor(&eg);
}
void SynthEngine::prepare(double rate) {
    if (!std::isfinite(rate) || rate < 8000 || rate > 384000)
        rate = 44100.0;  // Protect host reset callbacks from invalid/uninitialized rates.
    const double internal = rate * cs01::OutputConverter::factor;
    vco.prepareToPlay(internal, 1);
    eg.prepareToPlay(internal, 1);
    lfo.prepareToPlay(internal, 1);
    original.prepareToPlay(internal, 1);
    modern.prepareToPlay(internal, 1);
    vca.prepareToPlay(internal, 1);
    midi.setSoundGenerator(vco.getSoundGenerator());
    midi.prepareToPlay(internal, 1);
    converter.reset();
}
void SynthEngine::panic() {
    midi.releaseResources();
    original.releaseResources();
    modern.releaseResources();
    vca.releaseResources();
    converter.reset();
}
void SynthEngine::handleMidi(const cs01::MidiMessage& message) {
    vco.applyPendingGeneratorChange();
    midi.setSoundGenerator(vco.getSoundGenerator());
    midi.handleMidiEvent(message);
    if (message.isAllSoundOff())
        panic();
}
float SynthEngine::renderSample() {
    vco.applyPendingGeneratorChange();
    midi.setSoundGenerator(vco.getSoundGenerator());
    float output = 0;
    for (int i = 0; i < cs01::OutputConverter::factor; ++i) {
        const float modulation = lfo.processSample();
        const bool pitchTarget = parameters.get(ParameterIds::lfoTarget) == 0;
        const float envelope = eg.processSample();
        float audio = vco.processSample(pitchTarget ? modulation : 0);
        if (parameters.get(ParameterIds::filterType) == 0)
            audio = original.processSample(audio, envelope, pitchTarget ? 0 : modulation);
        else
            audio = modern.processSample(audio, envelope, pitchTarget ? 0 : modulation);
        converter.push(vca.processSample(audio, envelope));
        // Sample the internal clock at multiples of four. FIR delay is exactly 16 frames.
        if (i == 0)
            output = converter.output();
    }
    return output;
}
