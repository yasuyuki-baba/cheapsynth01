#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include "CS01ToneGenerator.h"
#include "CS01NoiseGenerator.h"

class CS01SynthVoice
{
public:
    CS01SynthVoice(juce::AudioProcessorValueTreeState& apvts);

    // Existing methods of CS01SynthVoice
    void startNote(int midiNoteNumber, float velocity, int currentPitchWheelPosition);
    void stopNote(bool allowTailOff);
    void pitchWheelMoved(int newPitchWheelValue);
    void setLfoValue(float lfoValue);
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples);
    bool isActive() const;
    int getCurrentlyPlayingNote() const;
    void prepare(const juce::dsp::ProcessSpec& spec);
    void changeNote(int midiNoteNumber);

    // Methods integrated from CS01VCOProcessor
    void updateBlockRateParameters();
    void process(const juce::dsp::ProcessContextReplacing<float>& context);
    void reset();
    void setNote(int midiNoteNumber, bool isLegato);

private:
    juce::AudioProcessorValueTreeState& apvts;
    
    // Existing members of CS01SynthVoice
    int currentlyPlayingNote = 0;
    bool noteOn = false;
    bool tailOff = false;
    int tailOffCounter = 0;
    int tailOffDuration = 0;
    double sampleRate = 44100.0;
    
    // Members integrated from CS01VCOProcessor
    std::unique_ptr<CS01ToneGenerator> toneGenerator;
    CS01NoiseGenerator noiseGenerator;
    
    // Helper methods
    bool isNoiseMode() const;
};
