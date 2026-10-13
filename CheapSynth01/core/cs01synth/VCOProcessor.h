#pragma once
#include "cs01synth/ToneGenerator.h"
#include "cs01synth/NoiseGenerator.h"
class VCOProcessor {
   public:
    explicit VCOProcessor(cs01::ParameterState& p)
        : parameters(p), tone(p), noise(p), current(&tone) {}
    void prepare(double rate) {
        tone.prepare(rate);
        noise.prepare(rate);
        current = &tone;
        applyPendingGeneratorChange();
    }
    void setSampleRate(double rate) {
        tone.setSampleRate(rate);
        noise.setSampleRate(rate);
    }
    void applyPendingGeneratorChange() {
        ISoundGenerator* next =
            parameters.get(ParameterIds::feet) == 4 ? static_cast<ISoundGenerator*>(&noise) : &tone;
        if (next == current)
            return;
        const auto state = current->getPlaybackState();
        current->stopNote(false);
        if (next == &tone)
            tone.reset();
        else
            noise.reset();
        current = next;
        current->restorePlaybackState(state);
    }
    ISoundGenerator* getSoundGenerator() {
        return current;
    }
    float processSample(float lfo) {
        tone.updateBlockRateParameters();
        tone.setLfoValue(lfo * parameters.get(ParameterIds::modDepth));
        return current == &tone ? tone.renderContinuousSample() : noise.renderContinuousSample();
    }

   private:
    cs01::ParameterState& parameters;
    ToneGenerator tone;
    NoiseGenerator noise;
    ISoundGenerator* current;
};
