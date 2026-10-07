#pragma once

#include <JuceHeader.h>

#include "CS01Synth/IG02600BehavioralModel.h"
#include "Parameters.h"
#include "CS01Synth/VCAEmpiricalStages.h"

//==============================================================================
class VCAProcessor : public juce::AudioProcessor {
   public:
    //==============================================================================
    VCAProcessor(juce::AudioProcessorValueTreeState& apvts);
    ~VCAProcessor() override;

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
        return "VCA";
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

   private:
    //==============================================================================
    juce::AudioProcessorValueTreeState& apvts;

    // Empirical second-order 40 Hz input coupling, not an RC reconstruction.
    juce::dsp::IIR::Filter<float> empiricalInputCoupling;

    // Implementation safety: additional 20 Hz DC removal.
    juce::dsp::IIR::Filter<float> safetyDcBlocker;

    // Implementation safety/rolloff: 15 kHz ceiling, bounded to 45% of rate.
    juce::dsp::IIR::Filter<float> safetyHighFreqRolloff;

    // Provisional IC gain/nonlinearity; no internal topology claim.
    IG02600BehavioralModel vcaModel;
    juce::SmoothedValue<float> egDepthControl;

    // External coupling/Tr7 path: empirical poles, not RC-derived targets.
    struct EmpiricalParameters {
        static constexpr double inputCouplingHz = 40.0;
        // Empirical normalized panel-volume curve, not an IC control law.
        static constexpr float volumeExponent = 2.5f;
        static constexpr float bufferCouplingReferencePole = 0.997f;
        static constexpr float outputCouplingReferencePole = 0.9995f;
    };
    struct SafetyParameters {
        static constexpr double dcBlockerHz = 20.0;
        static constexpr float rolloffHz = 15000.0f;
        static constexpr float maximumRateFraction = 0.45f;
    };
    EmpiricalVCACoupling bufferInputCoupling{EmpiricalParameters::bufferCouplingReferencePole};
    Tr7EmpiricalBuffer tr7Buffer;
    EmpiricalVCACoupling outputCoupling{EmpiricalParameters::outputCouplingReferencePole};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCAProcessor)
};
