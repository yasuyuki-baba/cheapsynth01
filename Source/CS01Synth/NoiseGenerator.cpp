#include "CS01Synth/NoiseGenerator.h"

#include "MidiParameterValue.h"

#include <cmath>

NoiseGenerator::NoiseGenerator(juce::AudioProcessorValueTreeState& apvts) : apvts(apvts) {}

void NoiseGenerator::prepare(const juce::dsp::ProcessSpec& spec) {
    noiseFilter.prepare(spec);
    sampleRate = spec.sampleRate;

    // Limit frequency to not exceed Nyquist frequency
    float cutoffFreq = std::min(12000.0f, static_cast<float>(spec.sampleRate * 0.45f));

    *noiseFilter.coefficients =
        *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(spec.sampleRate, cutoffFreq);
}

void NoiseGenerator::renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample,
                                     int numSamples) {
    renderBlock(buffer, startSample, numSamples, false);
}

void NoiseGenerator::renderContinuousBlock(juce::AudioBuffer<float>& buffer, int startSample,
                                           int numSamples) {
    renderBlock(buffer, startSample, numSamples, true);
}

void NoiseGenerator::renderBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                                 bool freeRunning) {
    updateReleaseDuration();
    const int activeSamples =
        !isActive() ? 0
        : tailOff   ? juce::jmin(numSamples, juce::jmax(0, tailOffDuration - tailOffCounter))
                    : numSamples;
    const int renderedSamples = freeRunning ? numSamples : activeSamples;
    for (int sample = 0; sample < renderedSamples; ++sample) {
        const float filteredNoise = getNextSample();

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, startSample + sample, filteredNoise);
    }
    if (tailOff) {
        tailOffCounter += activeSamples;
        if (tailOffCounter >= tailOffDuration) {
            tailOff = false;
            noteOn = false;
        }
    }
}

float NoiseGenerator::getNextSample() {
    return noiseFilter.processSample(random.nextFloat() * 2.0f - 1.0f);
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
    graphOwnsRelease = false;
    noteOn = false;
    if (allowTailOff) {
        // Start tail off
        tailOff = true;

        // Get release time from parameter (convert to samples)
        releaseSeconds = getMidiParameterValue(apvts, ParameterIds::release);
        tailOffDuration =
            static_cast<int>(std::ceil(static_cast<double>(releaseSeconds) * sampleRate));
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
        releaseSeconds = getMidiParameterValue(apvts, ParameterIds::release);
        tailOffCounter = 0;
        tailOffDuration =
            static_cast<int>(std::llround(state.releaseSecondsRemaining * sampleRate));
        currentlyPlayingNote = 0;
    }
}

void NoiseGenerator::setReleaseSamplesRemaining(int samples) {
    graphOwnsRelease = true;
    if (!tailOff)
        return;
    releaseSeconds = getMidiParameterValue(apvts, ParameterIds::release);
    tailOffCounter = 0;
    tailOffDuration = juce::jmax(0, samples);
    if (samples <= 0) {
        tailOff = false;
        noteOn = false;
    }
}

void NoiseGenerator::updateReleaseDuration() {
    if (graphOwnsRelease)
        return;
    if (tailOff) {
        const float requested = getMidiParameterValue(apvts, ParameterIds::release);
        if (requested != releaseSeconds) {
            releaseSeconds = requested;
            tailOffCounter = 0;
            tailOffDuration =
                static_cast<int>(std::ceil(static_cast<double>(requested) * sampleRate));
        }
    }
}
