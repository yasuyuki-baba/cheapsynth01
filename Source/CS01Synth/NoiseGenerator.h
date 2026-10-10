#pragma once

#include "DSP/Primitives.h"

#include "CS01Synth/ISoundGenerator.h"
#include "Parameters.h"

/**
 * NoiseGenerator - Responsible for noise generation
 *
 * This class generates white noise and processes it through a filter.
 * It implements the ISoundGenerator interface.
 */
class NoiseGenerator : public ISoundGenerator {
   public:
    NoiseGenerator(cs01::ParameterState& parameters);
    ~NoiseGenerator() override = default;

    // ISoundGenerator implementation - sound generation methods
    void prepare(double sampleRate) override;
    void reset() {
        noiseFilter.reset();
        stopNote(false);
    }
    float renderSample() override;

    // ISoundGenerator implementation - note handling methods
    void startNote(int midiNoteNumber, float velocity, int currentPitchWheelPosition) override;
    void stopNote(bool allowTailOff) override;
    PlaybackState getPlaybackState() const override;
    void restorePlaybackState(const PlaybackState& state) override;
    int lastNote = 0;
    void changeNote(int midiNoteNumber) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    bool isActive() const override;
    int getCurrentlyPlayingNote() const override;

   private:
    cs01::ParameterState& parameters;
    uint32_t randomState = 0x6d2b79f5u;
    cs01::Biquad noiseFilter;

    // Note state
    bool noteOn = false;
    bool tailOff = false;
    int tailOffCounter = 0;
    int tailOffDuration = 0;
    int currentlyPlayingNote = 0;
    double sampleRate = 44100.0;
    int pitchWheelValue = 8192;  // Center value
};
