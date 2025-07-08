#pragma once

#include <JuceHeader.h>
#include "ISoundGenerator.h"
#include "../Parameters.h"

/**
 * NoiseGenerator - Responsible for noise generation
 *
 * This class generates white noise and processes it through a filter.
 * It implements the ISoundGenerator interface.
 */
class NoiseGenerator : public ISoundGenerator {
   public:
    NoiseGenerator(juce::AudioProcessorValueTreeState& apvts);
    ~NoiseGenerator() override = default;

    // ISoundGenerator implementation - sound generation methods
    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample,
                         int numSamples) override;

    // ISoundGenerator implementation - note handling methods
    void startNote(int midiNoteNumber, float velocity, int currentPitchWheelPosition) override;
    void stopNote(bool allowTailOff) override;
    void changeNote(int midiNoteNumber) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    bool isActive() const override;
    int getCurrentlyPlayingNote() const override;

   private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::Random random;
    juce::dsp::IIR::Filter<float> noiseFilter;

    // Note state
    bool noteOn = false;
    bool tailOff = false;
    int tailOffCounter = 0;
    int tailOffDuration = 0;
    int currentlyPlayingNote = 0;
    double sampleRate = 44100.0;
    int pitchWheelValue = 8192;  // Center value
};
