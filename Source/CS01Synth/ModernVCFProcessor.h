#pragma once

#include <JuceHeader.h>

#include "CS01Synth/CS01IIVCFCircuit.h"
#include "CS01Synth/IFilter.h"  // Interface
#include "Parameters.h"

//==============================================================================
// ModernVCFProcessor - CS01II-inspired four-pole lowpass approximation.
class ModernVCFProcessor : public juce::AudioProcessor, public IFilter {
   public:
    using Model = CS01IIVCFCircuit::Model;
    void setModel(Model newModel) { filter.setModel(newModel); }
    Model getModel() const { return filter.getModel(); }

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
    CS01IIVCFCircuit filter;
    juce::AudioBuffer<float> processingBuffer;  // Reusable temporary buffer for audio processing
    int processingBufferCapacity = 0;           // Capacity (in samples) of processingBuffer
    double processingSampleRate = 44100.0;

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
        return juce::jlimit(0.0f, 1.0f, resonanceParam);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModernVCFProcessor)
};
