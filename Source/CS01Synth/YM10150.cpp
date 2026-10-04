#include "YM10150.h"
#include "WaveformStrategies.h"
#include <cmath>

float YM10150::generateMasterSquareWave(float finalPitch, float sampleRate,
                                       float& phase, float& phaseIncrement) const {
    // Calculate frequency directly from finalPitch using continuous calculation
    // This ensures smooth pitch bend and pitch slider operation
    float frequency = 440.0f * static_cast<float>(std::exp2(static_cast<double>((finalPitch - 69.0f) / 12.0f)));
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

YM10150::YM10150() {
    // Initialize waveform strategy mapping directly
    strategies[Waveform::Triangle] = std::make_unique<TriangleWaveformStrategy>();
    strategies[Waveform::Sawtooth] = std::make_unique<SawtoothWaveformStrategy>();
    strategies[Waveform::Square] = std::make_unique<SquareWaveformStrategy>();
    strategies[Waveform::Pulse] = std::make_unique<PulseWaveformStrategy>();
    strategies[Waveform::Pwm] = std::make_unique<PWMWaveformStrategy>();

    // Set default strategy
    selected = strategies[Waveform::Sawtooth].get();
}

void YM10150::selectWaveform(Waveform waveform) {
    if (previous != waveform)
        selected->reset();
    selected = strategies[waveform].get();
    previous = waveform;
}
void YM10150::reset() {
    previousSample = 0.0f;
    for (auto& entry : strategies)
        entry.second->reset();
}
float YM10150::generateWaveform(float masterSquare, float phase, float increment,
                               float sampleRate, juce::dsp::Oscillator<double>& pwmLfo) {
    if (selected == nullptr)
        return masterSquare;
    return shapeOutput(selected->generate(masterSquare, phase, increment, sampleRate,
                                          previousSample, pwmLfo));
}
