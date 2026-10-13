#include "cs01synth/NoiseGenerator.h"

NoiseGenerator::NoiseGenerator(cs01::ParameterState& parameters) : parameters(parameters) {}

void NoiseGenerator::prepare(double rate) {
    sampleRate = rate;
    noiseFilter.firstOrderLowPass(rate, std::min(12000.0, rate * .45));
    randomState = 0x6d2b79f5u;
    reset();
}
void NoiseGenerator::setSampleRate(double rate) {
    tailOffDuration = static_cast<int>(
        std::lround(std::max(0, tailOffDuration - tailOffCounter) * rate / sampleRate));
    tailOffCounter = 0;
    sampleRate = rate;
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
    if (tailOff && ++tailOffCounter >= tailOffDuration) {
        tailOff = false;
        noteOn = false;
    }
    return output;
}

// MIDI note handling
void NoiseGenerator::startNote(int midiNoteNumber, float velocity, int pitchBendValue) {
    lastNote = midiNoteNumber;
    currentNoteNumber = midiNoteNumber;
    lastPitchBend = pitchBendValue;
    noteOn = true;
    tailOff = false;
}

void NoiseGenerator::stopNote(bool allowReleaseTail) {
    if (allowReleaseTail) {
        // Start tail off
        tailOff = true;

        // Get release time from parameter (convert to samples)
        float releaseSecs = parameters.get(ParameterIds::release);
        tailOffDuration = static_cast<int>(releaseSecs * sampleRate);
        tailOffCounter = 0;
    } else {
        // Stop immediately
        noteOn = false;
        tailOff = false;
    }

    currentNoteNumber = 0;
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

bool NoiseGenerator::isActive() const {
    return noteOn || (tailOff && tailOffCounter < tailOffDuration);
}

int NoiseGenerator::currentNote() const {
    return currentNoteNumber;
}

ISoundGenerator::PlaybackState NoiseGenerator::getPlaybackState() const {
    return {noteOn && !tailOff, lastNote, lastPitchBend,
            tailOff ? std::max(0, tailOffDuration - tailOffCounter) / sampleRate : 0.0};
}

void NoiseGenerator::restorePlaybackState(const PlaybackState& state) {
    stopNote(false);
    if (!state.held && state.releaseSecondsRemaining <= 0.0)
        return;
    startNote(state.note, 1.0f, state.pitchBend);
    if (!state.held) {
        noteOn = false;
        tailOff = true;
        tailOffCounter = 0;
        tailOffDuration =
            static_cast<int>(std::llround(state.releaseSecondsRemaining * sampleRate));
        currentNoteNumber = 0;
    }
}

// The production engine's EG owns the release clock, including live edits.
void NoiseGenerator::setReleaseSamplesRemaining(int samples) {
    if (!tailOff)
        return;
    tailOffCounter = 0;
    tailOffDuration = std::max(0, samples);
    if (samples <= 0) {
        tailOff = false;
        noteOn = false;
    }
}
