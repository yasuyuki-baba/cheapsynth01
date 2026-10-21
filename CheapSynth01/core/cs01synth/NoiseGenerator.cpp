#include "cs01synth/NoiseGenerator.h"

NoiseGenerator::NoiseGenerator(cs01::ParameterState& parameters) : parameters(parameters) {}

void NoiseGenerator::prepare(double rate) {
    gateSampleRate = rate;
    noiseFilter.firstOrderLowPass(rate, std::min(12000.0, rate * .45));
    randomState = 0x6d2b79f5u;
    reset();
}
void NoiseGenerator::setSampleRate(double rate) {
    rescaleGateSampleRate(rate);
    noiseFilter.firstOrderLowPass(rate, std::min(12000.0, rate * .45));
}
float NoiseGenerator::renderSample() {
    if (!isActive())
        return 0;
    return renderContinuousSample();
}
float NoiseGenerator::renderContinuousSample() {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    const float noise = static_cast<float>(static_cast<double>(randomState) / 4294967295.0 * 2 - 1);
    const float output = noiseFilter.processSample(noise);
    advanceTailOff();
    return output;
}

// MIDI note handling
void NoiseGenerator::startNote(int midiNoteNumber, float velocity, int pitchBendValue) {
    beginNoteGate(midiNoteNumber, pitchBendValue);
}

void NoiseGenerator::stopNote(bool allowReleaseTail) {
    endNoteGate(allowReleaseTail, parameters.get(ParameterIds::release), gateSampleRate);
}

void NoiseGenerator::changeNote(int midiNoteNumber) {
    lastNote = midiNoteNumber;
    currentNoteNumber = midiNoteNumber;
    // For noise, we don't need to change anything else when the note changes
}

void NoiseGenerator::setPitchBend(int pitchBendValue) {
    lastPitchBend = pitchBendValue;
    // For noise, pitch bend doesn't affect the sound
}
