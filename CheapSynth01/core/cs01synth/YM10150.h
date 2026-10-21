#pragma once

#include "cs01synth/IWaveformStrategy.h"
#include "cs01synth/SynthConstants.h"
#include "cs01synth/WaveformStrategies.h"

// Provisional waveform transfer model, not a gate-level YM10150 reconstruction.
// Waveform strategies supply empirical shaping; MIDI state and oversampling
// remain outside this model. Phase is explicit to preserve existing behavior.
class YM10150 {
   public:
    YM10150();
    void reset();
    void selectWaveform(Waveform waveform);
    float generateWaveform(float masterSquare, float phase, float increment, float sampleRate,
                           cs01::Oscillator& pwmLfo);
    float generateMasterSquareWave(float pitch, float sampleRate, float& phase,
                                   float& increment) const;
    float shapeOutput(float value) const;

   private:
    IWaveformStrategy* strategyFor(Waveform waveform) noexcept;
    TriangleWaveformStrategy triangle;
    SawtoothWaveformStrategy sawtooth;
    SquareWaveformStrategy square;
    PulseWaveformStrategy pulse;
    PWMWaveformStrategy pwm;
    IWaveformStrategy* selected = &sawtooth;
    Waveform previous = Waveform::Sawtooth;
    float previousSample = 0.0f;
};
