#pragma once

#include "dsp/Primitives.h"

#include <algorithm>
#include <cmath>

/**
 * ISoundGenerator - Interface for sound generation with MIDI note handling
 *
 * This interface provides a unified interface for sound generators like ToneGenerator
 * and NoiseGenerator, including MIDI note handling and sound generation capabilities.
 */
class ISoundGenerator {
   public:
    virtual ~ISoundGenerator() = default;

    // MIDI note handling
    virtual void startNote(int midiNoteNumber, float velocity, int pitchBendValue) = 0;
    virtual void stopNote(bool allowReleaseTail) = 0;
    virtual void changeNote(int midiNoteNumber) = 0;
    // Raw MIDI pitch bend: 0..16383, with 8192 at the center.
    virtual void setPitchBend(int pitchBendValue) = 0;
    virtual bool isActive() const {
        return noteOn || (tailOff && tailOffCounter < tailOffDuration);
    }
    virtual int currentNote() const {
        return currentNoteNumber;
    }
    struct PlaybackState {
        bool held = false;
        int note = 0;
        int pitchBend = 8192;
        double releaseSecondsRemaining = 0.0;
    };
    virtual PlaybackState getPlaybackState() const {
        return {noteOn, lastNote, lastPitchBend,
                tailOff && gateSampleRate > 0.0
                    ? std::max(0, tailOffDuration - tailOffCounter) / gateSampleRate
                    : 0.0};
    }
    virtual void restorePlaybackState(const PlaybackState& state) {
        stopNote(false);
        if (!state.held && state.releaseSecondsRemaining <= 0.0)
            return;
        startNote(state.note, 1.0f, state.pitchBend);
        if (!state.held) {
            noteOn = false;
            tailOff = true;
            tailOffCounter = 0;
            tailOffDuration =
                static_cast<int>(std::llround(state.releaseSecondsRemaining * gateSampleRate));
            currentNoteNumber = 0;
        }
    }
    // The production engine's EG owns the release clock, including live edits.
    virtual void setReleaseSamplesRemaining(int samples) {
        if (!tailOff)
            return;
        tailOffCounter = 0;
        tailOffDuration = std::max(0, samples);
        if (samples <= 0) {
            tailOff = false;
            noteOn = false;
        }
    }

    // Sound generation methods
    virtual void prepare(double sampleRate) = 0;
    virtual float renderSample() = 0;

    // LFO modulation - with default implementation
    virtual void setLfoValue(float value) {}

   protected:
    void resetNoteGateState() {
        noteOn = false;
        tailOff = false;
        tailOffCounter = 0;
        tailOffDuration = 0;
        currentNoteNumber = 0;
    }

    void beginNoteGate(int midiNoteNumber, int pitchBendValue) {
        lastNote = midiNoteNumber;
        currentNoteNumber = midiNoteNumber;
        lastPitchBend = pitchBendValue;
        noteOn = true;
        tailOff = false;
    }

    template <typename Rate>
    void endNoteGate(bool allowReleaseTail, float releaseSecs, Rate rate) {
        noteOn = false;
        currentNoteNumber = 0;
        if (allowReleaseTail) {
            tailOff = true;
            tailOffDuration = static_cast<int>(releaseSecs * rate);
            tailOffCounter = 0;
        } else {
            tailOff = false;
        }
    }

    void rescaleGateSampleRate(double newSampleRate) {
        if (gateSampleRate > 0.0) {
            tailOffDuration = static_cast<int>(std::lround(
                std::max(0, tailOffDuration - tailOffCounter) * (newSampleRate / gateSampleRate)));
            tailOffCounter = 0;
        }
        gateSampleRate = newSampleRate;
    }

    void advanceTailOff() {
        if (tailOff && ++tailOffCounter >= tailOffDuration) {
            tailOff = false;
            noteOn = false;
        }
    }

    int currentNoteNumber = 0;
    int lastNote = 0;
    int lastPitchBend = 8192;
    bool noteOn = false;
    bool tailOff = false;
    int tailOffCounter = 0;
    int tailOffDuration = 0;
    double gateSampleRate = 44100.0;
};
