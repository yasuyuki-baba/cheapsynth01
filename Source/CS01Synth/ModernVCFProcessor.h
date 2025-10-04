#pragma once

#include <JuceHeader.h>
#include "../Parameters.h"
#include "IFilter.h"  // Interface

//==============================================================================
// ModernVCFProcessor - Filter using JUCE's StateVariableTPTFilter (with less distortion)
class ModernVCFProcessor : public juce::AudioProcessor, public IFilter {
   public:
    //==============================================================================
    ModernVCFProcessor(juce::AudioProcessorValueTreeState& apvts);
    ~ModernVCFProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override {
        return nullptr;
    }
    bool hasEditor() const override {
        return false;
    }

    //==============================================================================
    const juce::String getName() const override {
        return "Modern VCF";
    }

    bool acceptsMidi() const override {
        return false;
    }
    bool producesMidi() const override {
        return false;
    }
    bool isMidiEffect() const override {
        return false;
    }
    double getTailLengthSeconds() const override {
        return 0.0;
    }

    //==============================================================================
    int getNumPrograms() override {
        return 1;
    }
    int getCurrentProgram() override {
        return 0;
    }
    void setCurrentProgram(int index) override {}
    const juce::String getProgramName(int index) override {
        return {};
    }
    void changeProgramName(int index, const juce::String& newName) override {}

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override {}
    void setStateInformation(const void* data, int sizeInBytes) override {}

    // Implementation of IFilter interface
    ResonanceMode getResonanceMode() const override {
        return ResonanceMode::Continuous;
    }

   private:
    //==============================================================================
    juce::AudioProcessorValueTreeState& apvts;
    juce::dsp::StateVariableTPTFilter<float> filter;  // Single filter for mono processing
    juce::AudioBuffer<float> processingBuffer;  // Reusable temporary buffer for audio processing
    int processingBufferCapacity = 0; // Capacity (in samples) of processingBuffer

    // Cutoff frequency calculation function
    float calculateCutoffFrequency(float cutoffParam) {
        // Range covering the entire audible spectrum
        const float minFreq = 20.0f;     // Minimum cutoff frequency
        const float maxFreq = 20000.0f;  // Maximum cutoff frequency

        // Verify input range (actual frequency value)
        float cutoffFreq = juce::jlimit(minFreq, maxFreq, cutoffParam);

        return cutoffFreq;
    }

    // Resonance calculation function
    float calculateResonance(float resonanceParam) {
        // Map normalized input (0.0 - 1.0) to useful resonance range
        // Resonance for StateVariableTPTFilter is effective in the range of 0.1 to 0.9
        return 0.1f + (resonanceParam * 0.8f);  // Map to range 0.1 to 0.9
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModernVCFProcessor)
};
