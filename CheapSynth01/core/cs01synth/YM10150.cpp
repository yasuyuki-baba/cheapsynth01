#include "cs01synth/YM10150.h"

#include "cs01synth/WaveformStrategies.h"

#include <cmath>

float YM10150::generateMasterSquareWave(float finalPitch, float sampleRate, float& phase,
                                        float& phaseIncrement) const {
    // Calculate frequency directly from finalPitch using continuous calculation
    // This ensures smooth pitch bend and pitch slider operation
    float frequency =
        440.0f * static_cast<float>(std::exp2(static_cast<double>((finalPitch - 69.0f) / 12.0f)));
    phaseIncrement = frequency / sampleRate;

    // Generate master clock square wave (50% duty cycle)
    float t = phase;
    float baseSquare = (t < 0.5f) ? 1.0f : -1.0f;

    // Apply poly_blep anti-aliasing
    baseSquare += poly_blep(t, phaseIncrement);
    baseSquare -= poly_blep(fmod(t + 0.5f, 1.0f), phaseIncrement);

    // Update phase for next sample
    phase += phaseIncrement;
    if (phase >= 1.0f)
        phase -= 1.0f;

    // Emulate analog circuit characteristics
    return std::tanh(baseSquare * 1.2f);
}

float YM10150::shapeOutput(float value) const {
    return std::tanh(value * 1.2f);
}

YM10150::YM10150() = default;

IWaveformStrategy* YM10150::strategyFor(Waveform waveform) noexcept {
    switch (waveform) {
        case Waveform::Triangle:
            return &triangle;
        case Waveform::Sawtooth:
            return &sawtooth;
        case Waveform::Square:
            return &square;
        case Waveform::Pulse:
            return &pulse;
        case Waveform::Pwm:
            return &pwm;
    }
    return &sawtooth;
}

void YM10150::selectWaveform(Waveform waveform) {
    if (previous == waveform)
        return;
    selected->reset();
    selected = strategyFor(waveform);
    previous = waveform;
}
void YM10150::reset() {
    previousSample = 0.0f;
    triangle.reset();
    sawtooth.reset();
    square.reset();
    pulse.reset();
    pwm.reset();
}
float YM10150::generateWaveform(float masterSquare, float phase, float increment, float sampleRate,
                                cs01::Oscillator& pwmLfo) {
    if (selected == nullptr)
        return masterSquare;
    return shapeOutput(
        selected->generate(masterSquare, phase, increment, sampleRate, previousSample, pwmLfo));
}
