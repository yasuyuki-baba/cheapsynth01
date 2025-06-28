#pragma once

#include <JuceHeader.h>
#include "../Parameters.h"
#include "SynthConstants.h"
#include "INoteHandler.h"

/**
 * ToneGenerator - Responsible for sound generation and MIDI note handling
 * 
 * This class implements the INoteHandler interface and generates audio samples
 * based on MIDI note input and various synthesis parameters.
 */
class ToneGenerator : public INoteHandler
{
public:
    ToneGenerator(juce::AudioProcessorValueTreeState& apvts);

    // INoteHandler interface implementation
    void startNote(int midiNoteNumber, float velocity, int currentPitchWheelPosition) override;
    void stopNote(bool allowTailOff) override;
    void changeNote(int midiNoteNumber) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    bool isActive() const override;
    int getCurrentlyPlayingNote() const override;

    // Audio processing methods
    void prepare(const juce::dsp::ProcessSpec &spec);
    void updateBlockRateParameters();
    void reset();
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples);
    void process(const juce::dsp::ProcessContextReplacing<float>& context);
    
    // Sound generation methods
    float getNextSample();
    void setLfoValue(float lfoValue);
    void setNote(int midiNoteNumber, bool isLegato);
    void setPitchBend(float bendInSemitones);

private:
    float generateVcoSample();
    void setVcoFrequency(float frequency);
    void calculateSlideParameters(int targetNote);

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
    float midiPitchBendValue = 0.0f;
    bool isSliding = false;
    int slideSamplesRemaining = 0;
    float slideIncrement = 0.0f;
    int samplesPerStep = 0;
    int stepCounter = 0;

    // VCO
    float sampleRate = 44100.0f;
    float phase = 0.0f;
    float phaseIncrement = 0.0f;
    float leakyIntegratorState = 0.0f;
    float dcBlockerState = 0.0f;
    
    // Cached Parameters
    float currentModDepth = 0.0f;
    Waveform currentWaveform = Waveform::Sawtooth;
    LfoTarget currentLfoTarget = LfoTarget::Vco;
    Feet currentFeet = Feet::Feet8;

    // LFOs
    juce::dsp::Oscillator<float> pwmLfo;
    float lfoValue = 0.0f;
};
