#pragma once

#include <JuceHeader.h>
#include "../Parameters.h"
#include "SynthConstants.h"
#include "ISoundGenerator.h"
#include "IWaveformStrategy.h"

/**
 * ToneGenerator - Responsible for sound generation and MIDI note handling
 *
 * This class implements the ISoundGenerator interface and generates audio samples
 * based on MIDI note input and various synthesis parameters.
 */
class ToneGenerator : public ISoundGenerator {
   public:
    ToneGenerator(juce::AudioProcessorValueTreeState& apvts);

    // ISoundGenerator implementation - note handling methods
    void startNote(int midiNoteNumber, float velocity, int currentPitchWheelPosition) override;
    void stopNote(bool allowTailOff) override;
    void changeNote(int midiNoteNumber) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    bool isActive() const override;
    int getCurrentlyPlayingNote() const override;

    // Audio processing methods
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void updateBlockRateParameters();
    void reset();
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample,
                         int numSamples) override;
    void process(const juce::dsp::ProcessContextReplacing<float>& context);

    // Sound generation methods
    float getNextSample();
    void setLfoValue(float lfoValue) override;
    void setNote(int midiNoteNumber, bool isLegato);
    void setPitchBend(float bendInSemitones);

   private:
    float generateVcoSampleFromMaster(float masterSquare);
    void calculateSlideParameters(int targetNote);

    // Base waveform generation methods
    float generateMasterSquareWave(float finalPitch);

    juce::AudioProcessorValueTreeState& apvts;

    // Note state
    int currentlyPlayingNote = 0;
    bool noteOn = false;
    bool tailOff = false;
    int tailOffCounter = 0;
    int tailOffDuration = 0;

    // Pitch State
    float currentPitch = 60.0f;
    float targetPitch = 60.0f;
    float pitchBend = 0.0f;
    bool isSliding = false;
    int samplesPerStep = 0;
    int stepCounter = 0;

    // VCO
    float sampleRate = 44100.0f;
    float phase = 0.0f;
    float phaseIncrement = 0.0f;
    float leakyIntegratorState = 0.0f;
    float dcBlockerState = 0.0f;

    // Base square wave generation (master clock)
    float previousBaseSquare = 0.0f;

    // Cached Parameters
    float currentModDepth = 0.0f;
    float pitchBendOffset = 0.0f;
    float pitchOffset = 0.0f;
    Waveform currentWaveform = Waveform::Sawtooth;
    Feet currentFeet = Feet::Feet8;

    // LFOs
    juce::dsp::Oscillator<float> pwmLfo;
    float lfoValue = 0.0f;

    // Waveform Strategy Pattern - simplified with direct mapping
    std::map<Waveform, std::unique_ptr<IWaveformStrategy>> waveformStrategies;
    IWaveformStrategy* currentWaveformStrategy = nullptr;
    Waveform previousWaveform = Waveform::Sawtooth;

    // Helper methods for strategy pattern
    void initializeWaveformStrategies();
    void updateWaveformStrategy();
};
