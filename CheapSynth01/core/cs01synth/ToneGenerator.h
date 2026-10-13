#pragma once

#include "dsp/Primitives.h"

#include "cs01synth/ISoundGenerator.h"
#include "cs01synth/IWaveformStrategy.h"
#include "cs01synth/SynthConstants.h"
#include "cs01synth/YM10150.h"
#include "Parameters.h"

/**
 * ToneGenerator - Responsible for sound generation and MIDI note handling
 *
 * This class implements the ISoundGenerator interface and generates audio samples
 * based on MIDI note input and various synthesis parameters.
 */
class ToneGenerator : public ISoundGenerator {
   public:
    ToneGenerator(cs01::ParameterState& parameters);

    // ISoundGenerator implementation - note handling methods
    void startNote(int midiNoteNumber, float velocity, int pitchBendValue) override;
    void stopNote(bool allowReleaseTail) override;
    PlaybackState getPlaybackState() const override;
    void setReleaseSamplesRemaining(int samples) override;
    void restorePlaybackState(const PlaybackState& state) override;
    int lastNote = 0;
    int lastPitchBend = 8192;
    void changeNote(int midiNoteNumber) override;
    void setPitchBend(int pitchBendValue) override;
    bool isActive() const override;
    int currentNote() const override;

    // Audio processing methods
    void prepare(double sampleRate) override;
    void setSampleRate(double rate);
    void updateBlockRateParameters();
    void reset();
    float renderSample() override;
    float renderContinuousSample();

    // Sound generation methods
    float getNextSample();
    void setLfoValue(float lfoValue) override;
    void setNote(int midiNoteNumber, bool isLegato);
    void setPitchBendSemitones(float bendInSemitones);

   private:
    float generateVcoSampleFromMaster(float masterSquare);
    void calculateSlideParameters(int targetNote);

    // Base waveform generation methods
    float generateMasterSquareWave(float finalPitch);

    cs01::ParameterState& parameters;

    // Note state
    int currentNoteNumber = 0;
    bool noteOn = false;
    bool tailOff = false;
    int tailOffCounter = 0;
    int tailOffDuration = 0;

    // Pitch State
    float currentPitch = 60.0f;
    float targetPitch = 60.0f;
    float pitchBendSemitones = 0.0f;
    float appliedBendUpRange = -1.0f;
    float appliedBendDownRange = -1.0f;
    bool isSliding = false;
    int samplesPerStep = 0;
    int stepCounter = 0;

    // VCO
    YM10150 waveformModel;
    float sampleRate = 44100.0f;
    float internalSampleRate = 176400.0f;
    float phase = 0.0f;
    float phaseIncrement = 0.0f;
    float leakyIntegratorState = 0.0f;
    float dcBlockerState = 0.0f;

    // Base square wave generation (master clock)

    // Cached Parameters
    float currentModDepth = 0.0f;
    float pitchBendOffset = 0.0f;
    float pitchOffset = 0.0f;
    Waveform currentWaveform = Waveform::Sawtooth;
    Feet currentFeet = Feet::Feet8;

    // LFOs
    cs01::Oscillator pwmLfo;
    float lfoValue = 0.0f;
};
