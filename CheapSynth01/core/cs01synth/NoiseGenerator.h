#pragma once

#include "dsp/Primitives.h"

#include "cs01synth/ISoundGenerator.h"
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
    void setSampleRate(double rate);
    void reset() {
        noiseFilter.reset();
        stopNote(false);
    }
    float renderSample() override;
    float renderContinuousSample();

    // ISoundGenerator implementation - note handling methods
    void startNote(int midiNoteNumber, float velocity, int pitchBendValue) override;
    void stopNote(bool allowReleaseTail) override;
    PlaybackState getPlaybackState() const override;
    void setReleaseSamplesRemaining(int samples) override;
    void restorePlaybackState(const PlaybackState& state) override;
    int lastNote = 0;
    void changeNote(int midiNoteNumber) override;
    void setPitchBend(int pitchBendValue) override;
    bool isActive() const override;
    int currentNote() const override;

   private:
    cs01::ParameterState& parameters;
    uint32_t randomState = 0x6d2b79f5u;
    cs01::Biquad noiseFilter;

    // Note state
    bool noteOn = false;
    bool tailOff = false;
    int tailOffCounter = 0;
    int tailOffDuration = 0;
    int currentNoteNumber = 0;
    double sampleRate = 44100.0;
    int lastPitchBend = 8192;  // Center value
};
