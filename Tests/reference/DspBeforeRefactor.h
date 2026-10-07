#pragma once

#include <JuceHeader.h>
#include "Parameters.h"

#include <cmath>

// Captured from repository commit 1cbbdd7 (before this refactor).
// Frozen production DSP before the naming/stage refactor. Test oracle only.
// Keep arithmetic unchanged; do not update this alongside production DSP.

// IG02610-inspired behavioral model. The TPT state-variable topology and
// nonlinear feedback are numerical/behavioral choices, not an IC reconstruction.
class BeforeIG02610 {
   public:
    struct EmpiricalParameters {
        // Provisional damping map; replace with measured resonance response.
        static constexpr float minimumDamping = 0.12f;
        static constexpr float maximumDamping = 1.25f;
        // Provisional normalized drives; neither is calibrated to an IC.
        static constexpr float feedbackDrive = 1.0f;
        static constexpr float integratorInputDrive = 2.0f;
        static constexpr float maximumOutput = 1.5f;
    };

    void prepare(double rate) {
        sampleRate = juce::jmax(1.0, rate);
        reset();
    }
    void reset() {
        ic1eq = ic2eq = 0.0;
    }

    float processSample(float input, float cutoffHz, float resonance) {
        if (!std::isfinite(input))
            input = 0.0f;
        const double boundedCutoff =
            juce::jlimit(20.0, sampleRate * 0.45, static_cast<double>(cutoffHz));
        const double g = std::tan(juce::MathConstants<double>::pi * boundedCutoff / sampleRate);
        const double damping = juce::jmap(static_cast<double>(juce::jlimit(0.0f, 1.0f, resonance)),
                                          static_cast<double>(EmpiricalParameters::maximumDamping),
                                          static_cast<double>(EmpiricalParameters::minimumDamping));

        // Behavioral hypothesis based on a two-integrator SVF with soft
        // limiting around the summing/integrator input and resonant feedback.
        // These placements do not claim to reproduce the IC's internal circuit.
        const double feedback = damping * std::tanh(ic2eq * EmpiricalParameters::feedbackDrive);
        const double integratorInput = std::tanh((static_cast<double>(input) - feedback) *
                                                 EmpiricalParameters::integratorInputDrive) /
                                       EmpiricalParameters::integratorInputDrive;
        const double v1 = (ic1eq + g * (integratorInput - ic2eq)) / (1.0 + g * (g + damping));
        const double v2 = ic2eq + g * v1;
        ic1eq = 2.0 * v1 - ic1eq;
        ic2eq = 2.0 * v2 - ic2eq;

        const double output = std::isfinite(v2) ? v2 : 0.0;
        return static_cast<float>(
            juce::jlimit(-static_cast<double>(EmpiricalParameters::maximumOutput),
                         static_cast<double>(EmpiricalParameters::maximumOutput), output));
    }

   private:
    double sampleRate = 44100.0;
    double ic1eq = 0.0, ic2eq = 0.0;
};

// CS-01II / IG05630-inspired behavioral hypothesis.
// Four poles, continuous resonant feedback, and bounded nonlinear stages are
// model choices based on the supplied report, not an internal IC reconstruction.
class BeforeIG05630 {
   public:
    struct EmpiricalParameters {
        // Butterworth-aligned section Q values; used as the zero-resonance target.
        static constexpr float firstSectionQ = 0.5411961f;
        static constexpr float secondSectionQ = 1.3065630f;
        // Provisional normalized saturation and resonance ranges. Not calibrated.
        static constexpr float inputDrive = 2.0f;
        static constexpr float feedbackDrive = 1.0f;
        // Provisional feedback map kept below the model's self-oscillation
        // threshold. Non-oscillating behavior is a conservative hypothesis,
        // not a verified property of the hardware.
        static constexpr float maximumFeedbackGain = 1.2f;
        static constexpr float resonanceCurve = 4.0f;
        static constexpr float maximumOutput = 1.5f;
    };

    void prepare(double sampleRate);
    void reset();
    void setCutoffFrequency(float frequency);
    void setResonance(float resonance);
    float processSample(float sample);

   private:
    struct StateVariableLowpass {
        float integrator1 = 0.0f;
        float integrator2 = 0.0f;

        float processSample(float input, float g, float damping);
        void reset();
    };

    double sampleRate = 44100.0;
    float cutoff = 1000.0f;
    float resonance = 0.0f;
    float resonanceFeedbackGain = 0.0f;
    float feedbackOutput = 0.0f;
    StateVariableLowpass firstSection;
    StateVariableLowpass secondSection;
};

//==============================================================================
// CS-01 VCF signal path: IG02610-inspired behavioral core plus external coupling.
// Includes uncalibrated approximations; not a complete component-level reconstruction.
class BeforeOriginalCircuit {
   public:
    BeforeOriginalCircuit();                   // Default constructor (safe initial values)
    BeforeOriginalCircuit(double sampleRate);  // Constructor with sample rate specification
    ~BeforeOriginalCircuit() = default;

    void reset();
    void prepare(double sampleRate);
    void setCutoffFrequency(float newCutoff);
    void setResonance(float newResonance);

    float processSample(int channel, float sample);

    // Process a block of samples (more efficient)
    void processBlock(float* samples, int numSamples);

    // Process a block of samples with multiple channels
    void processBlock(float** channelData, int numChannels, int numSamples);

    // Process a block with per-sample cutoff modulation
    void processBlock(float* samples, int numSamples, const float* cutoffModulation,
                      float baseResonance);

   private:
    float cutoff, resonance, sampleRate;
    BeforeIG02610 model;

    // Input stage model
    struct InputStage {
        float prevSample = 0.0f;
        juce::dsp::IIR::Filter<float> dcBlocker;

        void prepare(double sampleRate) {
            dcBlocker.reset();
            dcBlocker.coefficients =
                juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 20.0f);
        }

        void reset() {
            prevSample = 0.0f;
            dcBlocker.reset();
        }
    };

    // Output stage model
    struct OutputStage {
        float prevInput = 0.0f;
        float prevOutput = 0.0f;
        double sampleRate = 44100.0;

        void prepare(double newSampleRate) {
            sampleRate = newSampleRate;
        }

        void reset() {
            prevInput = 0.0f;
            prevOutput = 0.0f;
        }
    };

    InputStage inputStage;
    OutputStage outputStage;

    // Input stage processing
    float processInputStage(float sample);

    // Output stage processing
    float processOutputStage(float sample);
};

// Circuit boundary for Modern, analogous to BeforeOriginalCircuit in Original.
// External coupling is currently unity: no uncalibrated stages are added.
class BeforeModernCircuit {
   public:
    void prepare(double sampleRate);
    void reset();
    void setCutoffFrequency(float frequency);
    void setResonance(float resonance);
    float processSample(int channel, float sample);

   private:
    BeforeIG05630 model;
    float maximumCutoff = 20000.0f;
};

//==============================================================================

// Provisional BeforeIG02600 transfer model, not a reconstruction of its internal circuit.
// Controls are normalized software values, not physical pin voltages.
// External coupling, output buffer and panel volume mapping belong to BeforeVCAProcessor.
class BeforeIG02600 {
   public:
    float processSample(float input, float egValue, float egDepth, float breathInput,
                        float breathDepth, float volumeGain) const;
};

class BeforeVCAProcessor : public juce::AudioProcessor {
   public:
    //==============================================================================
    BeforeVCAProcessor(juce::AudioProcessorValueTreeState& apvts);
    ~BeforeVCAProcessor() override;

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

    // Input stage high-pass filter (82K resistor and 1/50 capacitor)
    juce::dsp::IIR::Filter<float> inputHighPass;

    // Simple DC blocking filter
    juce::dsp::IIR::Filter<float> dcBlocker;

    // Simple high frequency rolloff filter
    juce::dsp::IIR::Filter<float> highFreqRolloff;

    // BeforeIG02600 VCA chip emulation
    BeforeIG02600 vcaModel;
    juce::SmoothedValue<float> egDepthControl;

    // Tr7 transistor buffer emulation
    float processTr7Buffer(float input);

    // Output coupling capacitor emulation
    float processOutputCoupling(float input);

    // State variables for analog circuit emulation
    float capacitorState = 0.0f;
    float prevOutput = 0.0f;
    float outCapacitorState = 0.0f;
    float bufferCouplingPole = 0.997f;
    float outputCouplingPole = 0.9995f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BeforeVCAProcessor)
};
