#pragma once

#include "DSP/Primitives.h"

/**
 * ISoundGenerator - Interface for sound generation with MIDI note handling
 *
 * This interface provides a unified interface for sound generators like ToneGenerator
 * and NoiseGenerator, including MIDI note handling and sound generation capabilities.
 */
class ISoundGenerator {
   public:
    virtual ~ISoundGenerator() = default;

    // Note handling methods from INoteHandler
    virtual void startNote(int midiNoteNumber, float velocity, int currentPitchWheelPosition) = 0;
    virtual void stopNote(bool allowTailOff) = 0;
    virtual void changeNote(int midiNoteNumber) = 0;
    virtual void pitchWheelMoved(int newPitchWheelValue) = 0;
    virtual bool isActive() const = 0;
    virtual int getCurrentlyPlayingNote() const = 0;
    struct PlaybackState {
        bool held = false;
        int note = 0;
        int pitchWheel = 8192;
        double releaseSecondsRemaining = 0.0;
    };
    virtual PlaybackState getPlaybackState() const {
        return {};
    }
    virtual void restorePlaybackState(const PlaybackState&) {}

    // Sound generation methods
    virtual void prepare(double sampleRate) = 0;
    virtual float renderSample() = 0;

    // LFO modulation - with default implementation
    virtual void setLfoValue(float value) {}
};
