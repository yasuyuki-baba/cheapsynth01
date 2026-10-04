#pragma once
#include "SynthConstants.h"
#include "IWaveformStrategy.h"
#include <map>
#include <memory>

// Provisional waveform transfer model, not a gate-level YM10150 reconstruction.
// Waveform strategies supply empirical shaping; MIDI state and oversampling
// remain outside this model. Phase is explicit to preserve existing behavior.
class YM10150 {
public:
    YM10150();
    void reset();
    void selectWaveform(Waveform waveform);
    float generateWaveform(float masterSquare, float phase, float increment,
                           float sampleRate, juce::dsp::Oscillator<double>& pwmLfo);
    float generateMasterSquareWave(float pitch, float sampleRate,
                                   float& phase, float& increment) const;
    float shapeOutput(float value) const;
private:
    std::map<Waveform, std::unique_ptr<IWaveformStrategy>> strategies;
    IWaveformStrategy* selected = nullptr;
    Waveform previous = Waveform::Sawtooth;
    float previousSample = 0.0f;
};
