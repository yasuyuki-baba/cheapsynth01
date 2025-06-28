#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include "ToneGenerator.h"
#include "INoteHandler.h"

class SynthVoice : public INoteHandler
{
public:
    SynthVoice(juce::AudioProcessorValueTreeState& apvts);

    // Existing methods of SynthVoice
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
    
    // Existing members of SynthVoice
    int currentlyPlayingNote = 0;
    bool noteOn = false;
    bool tailOff = false;
    int tailOffCounter = 0;
    int tailOffDuration = 0;
    double sampleRate = 44100.0;
    
    // Members integrated from VCOProcessor
    std::unique_ptr<ToneGenerator> toneGenerator;
    
    // Helper methods
};
