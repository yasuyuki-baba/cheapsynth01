#include "CS01Synth/NoiseGenerator.h"

NoiseGenerator::NoiseGenerator(cs01::ParameterState& parameters) : parameters(parameters) {}

void NoiseGenerator::prepare(double rate) {
    sampleRate = rate;
    noiseFilter.firstOrderLowPass(rate, std::min(12000.0, rate * .45));
    randomState = 0x6d2b79f5u;
    reset();
}
float NoiseGenerator::renderSample() {
    if (!isActive())
        return 0;
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

// INoteHandler implementation
void NoiseGenerator::startNote(int midiNoteNumber, float velocity, int currentPitchWheelPosition) {
    lastNote = midiNoteNumber;
    currentlyPlayingNote = midiNoteNumber;
    pitchWheelValue = currentPitchWheelPosition;
    noteOn = true;
    tailOff = false;
}

void NoiseGenerator::stopNote(bool allowTailOff) {
    if (allowTailOff) {
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

    currentlyPlayingNote = 0;
}

void NoiseGenerator::changeNote(int midiNoteNumber) {
    lastNote = midiNoteNumber;
    currentlyPlayingNote = midiNoteNumber;
    // For noise, we don't need to change anything else when the note changes
}

void NoiseGenerator::pitchWheelMoved(int newPitchWheelValue) {
    pitchWheelValue = newPitchWheelValue;
    // For noise, pitch wheel doesn't affect the sound
}

bool NoiseGenerator::isActive() const {
    return noteOn || (tailOff && tailOffCounter < tailOffDuration);
}

int NoiseGenerator::getCurrentlyPlayingNote() const {
    return currentlyPlayingNote;
}

ISoundGenerator::PlaybackState NoiseGenerator::getPlaybackState() const {
    return {noteOn && !tailOff, lastNote, pitchWheelValue,
            tailOff ? std::max(0, tailOffDuration - tailOffCounter) / sampleRate : 0.0};
}

void NoiseGenerator::restorePlaybackState(const PlaybackState& state) {
    stopNote(false);
    if (!state.held && state.releaseSecondsRemaining <= 0.0)
        return;
    startNote(state.note, 1.0f, state.pitchWheel);
    if (!state.held) {
        noteOn = false;
        tailOff = true;
        tailOffCounter = 0;
        tailOffDuration =
            static_cast<int>(std::llround(state.releaseSecondsRemaining * sampleRate));
        currentlyPlayingNote = 0;
    }
}
