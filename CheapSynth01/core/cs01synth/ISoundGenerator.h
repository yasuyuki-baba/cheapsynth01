#pragma once

#include "dsp/Primitives.h"

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
    virtual bool isActive() const = 0;
    virtual int currentNote() const = 0;
    struct PlaybackState {
        bool held = false;
        int note = 0;
        int pitchBend = 8192;
        double releaseSecondsRemaining = 0.0;
    };
    virtual PlaybackState getPlaybackState() const {
        return {};
    }
    virtual void restorePlaybackState(const PlaybackState&) {}
    virtual void setReleaseSamplesRemaining(int samples) {}

    // Sound generation methods
    virtual void prepare(double sampleRate) = 0;
    virtual float renderSample() = 0;

    // LFO modulation - with default implementation
    virtual void setLfoValue(float value) {}
};
