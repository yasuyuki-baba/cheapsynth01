#pragma once

#include <JuceHeader.h>
#include "../Parameters.h"
#include "SynthConstants.h"

class ToneGenerator
{
public:
    ToneGenerator(juce::AudioProcessorValueTreeState& apvts);

    void prepare(const juce::dsp::ProcessSpec &spec);
    void updateBlockRateParameters();
    void reset();

    void setNote(int midiNoteNumber, bool isLegato);
    void setPitchBend(float bendInSemitones);
    
    float getNextSample();
    void setLfoValue(float lfoValue);

private:
    float generateVcoSample();
    void setVcoFrequency(float frequency);
    void calculateSlideParameters(int targetNote);

    juce::AudioProcessorValueTreeState& apvts;

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
