#pragma once

#include <JuceHeader.h>

#include "CS01Synth/CS01VCFCircuit.h"  // Include the CS01VCFCircuit filter
#include "CS01Synth/ExperimentalOriginalVCF.h"
#include "CS01Synth/IFilter.h"         // Updated interface
#include "Parameters.h"

#include <atomic>

//==============================================================================
class OriginalVCFProcessor : public juce::AudioProcessor, public IFilter {
   public:
    enum class Model { Legacy, Experimental };
    void setModel(Model newModel) { model.store(newModel, std::memory_order_relaxed); }
    Model getModel() const { return model.load(std::memory_order_relaxed); }
    //==============================================================================
    OriginalVCFProcessor(juce::AudioProcessorValueTreeState& apvts);
    ~OriginalVCFProcessor() override;

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
        return "Original VCF";
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
        return ResonanceMode::Toggle;
    }

   private:
    //==============================================================================
    juce::AudioProcessorValueTreeState& apvts;
    CS01VCFCircuit filter;  // Using CS01VCFCircuit instead of StateVariableTPTFilter
    std::atomic<Model> model{Model::Legacy};  // Preserve the existing sound by default.
    juce::HeapBlock<float> modulationBuffer;  //  Buffer preallocated for reuse
    int modulationBufferCapacity = 0;         // Capacity (in samples) of allocated modulationBuffer
    juce::SmoothedValue<float> egDepthControl;

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
        // For original filter, use threshold to binarize the value
        // Treat as High Resonance if value is 0.5 or higher
        if (resonanceParam >= 0.5f) {
            // High resonance setting - CS01VCFCircuit has max resonance of 0.8f
            return 0.7f;
        } else {
            // Low resonance setting
            return 0.2f;
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OriginalVCFProcessor)
};
