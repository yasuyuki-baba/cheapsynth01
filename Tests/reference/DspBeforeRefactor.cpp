#include "DspBeforeRefactor.h"

#include <algorithm>
#include <cmath>

float BeforeIG05630::StateVariableLowpass::processSample(float input, float g, float damping) {
    // TPT SVF with coupled trapezoidal integrators.
    const float normalization = 1.0f / (1.0f + damping * g + g * g);
    const float highpass = normalization * (input - integrator1 * (g + damping) - integrator2);
    const float bandpass = highpass * g + integrator1;
    integrator1 = highpass * g + bandpass;
    const float lowpass = bandpass * g + integrator2;
    integrator2 = bandpass * g + lowpass;
    return lowpass;
}

void BeforeIG05630::StateVariableLowpass::reset() {
    integrator1 = 0.0f;
    integrator2 = 0.0f;
}

void BeforeIG05630::prepare(double newSampleRate) {
    sampleRate = std::max(1.0, newSampleRate);
    reset();
}

void BeforeIG05630::reset() {
    feedbackOutput = 0.0f;
    firstSection.reset();
    secondSection.reset();
}

void BeforeIG05630::setCutoffFrequency(float frequency) {
    cutoff = std::isfinite(frequency) ? frequency : 1000.0f;
    cutoff = std::clamp(cutoff, 20.0f, static_cast<float>(sampleRate * 0.45));
}

void BeforeIG05630::setResonance(float amount) {
    resonance = std::isfinite(amount) ? std::clamp(amount, 0.0f, 1.0f) : 0.0f;
    // The fourth-power map is an explicit behavioral hypothesis that makes the
    // resonance control gradual. Maximum gain is limited to avoid self-oscillation.
    resonanceFeedbackGain = EmpiricalParameters::maximumFeedbackGain *
                            std::pow(resonance, EmpiricalParameters::resonanceCurve);
}

float BeforeIG05630::processSample(float sample) {
    if (!std::isfinite(sample))
        sample = 0.0f;

    constexpr float pi = 3.14159265358979323846f;
    const float g = std::tan(pi * cutoff / static_cast<float>(sampleRate));
    const float firstDamping = 1.0f / EmpiricalParameters::firstSectionQ;
    const float secondDamping = 1.0f / EmpiricalParameters::secondSectionQ;

    // The one-sample feedback state keeps this behavioral loop deterministic.
    // This is an explicit-delay TPT cascade, not a ZDF solution of an IC loop.
    const float feedback =
        resonanceFeedbackGain * std::tanh(feedbackOutput * EmpiricalParameters::feedbackDrive);
    const float integratorInput = std::tanh((sample - feedback) * EmpiricalParameters::inputDrive) /
                                  EmpiricalParameters::inputDrive;

    const float firstOutput = firstSection.processSample(integratorInput, g, firstDamping);
    const float output = secondSection.processSample(firstOutput, g, secondDamping);
    feedbackOutput = output;

    if (!std::isfinite(output)) {
        reset();
        return 0.0f;
    }
    return std::clamp(output, -EmpiricalParameters::maximumOutput,
                      EmpiricalParameters::maximumOutput);
}

BeforeOriginalCircuit::BeforeOriginalCircuit()
    : cutoff(1000.0f),
      resonance(0.1f),
      sampleRate(0.0f)  // Unset state, will be set by prepare()
{
    reset();
}

BeforeOriginalCircuit::BeforeOriginalCircuit(double sampleRate)
    : cutoff(1000.0f), resonance(0.1f), sampleRate(static_cast<float>(sampleRate)) {
    reset();
}

void BeforeOriginalCircuit::reset() {
    model.reset();

    // Reset input and output stages
    inputStage.reset();
    outputStage.reset();
}

void BeforeOriginalCircuit::prepare(double newSampleRate) {
    sampleRate = static_cast<float>(newSampleRate);
    model.prepare(newSampleRate);

    // Prepare input and output stages
    inputStage.prepare(newSampleRate);
    outputStage.prepare(newSampleRate);
}

// Input stage processing - Clean DC blocking based on circuit diagram
float BeforeOriginalCircuit::processInputStage(float sample) {
    // Empirical 20 Hz, second-order DC blocker; not derived from the audio-input RC network.
    sample = inputStage.dcBlocker.processSample(sample);

    return sample;
}

// Output stage processing - Clean DC blocking based on circuit diagram
float BeforeOriginalCircuit::processOutputStage(float sample) {
    // Empirical coupling approximation. The schematic's 1/50 means 1 uF / 50 V,
    // not 0.02 uF. Its effective load has not been established here.
    const float cutoffFreq = 8.0f;  // Uncalibrated model value, not an RC-derived target.
    const float alpha =
        1.0f / (1.0f + 2.0f * juce::MathConstants<float>::pi * cutoffFreq / outputStage.sampleRate);

    // Clean DC blocking filter
    outputStage.prevOutput = alpha * (outputStage.prevOutput + sample - outputStage.prevInput);
    outputStage.prevInput = sample;

    return outputStage.prevOutput;
}

void BeforeOriginalCircuit::setCutoffFrequency(float newCutoff) {
    cutoff = juce::jlimit(20.0f, 20000.0f, newCutoff);
}

void BeforeOriginalCircuit::setResonance(float newResonance) {
    resonance = juce::jlimit(0.0f, 1.0f, newResonance);
}

float BeforeOriginalCircuit::processSample(int channel, float sample) {
    const float coupledInput = processInputStage(sample);
    const float filtered = model.processSample(coupledInput, cutoff, resonance);
    return processOutputStage(filtered);
}

void BeforeOriginalCircuit::processBlock(float* samples, int numSamples) {
    // Process a block of mono samples
    for (int i = 0; i < numSamples; ++i) {
        samples[i] = processSample(0, samples[i]);
    }
}

void BeforeOriginalCircuit::processBlock(float** channelData, int numChannels, int numSamples) {
    // Process each channel separately
    // Note: For true stereo processing, we would need separate state variables per channel
    for (int ch = 0; ch < numChannels; ++ch) {
        float* channelSamples = channelData[ch];

        // Process this channel's samples
        for (int i = 0; i < numSamples; ++i) {
            channelSamples[i] = processSample(ch, channelSamples[i]);
        }
    }
}

void BeforeOriginalCircuit::processBlock(float* samples, int numSamples,
                                         const float* cutoffModulation, float baseResonance) {
    const float originalCutoff = cutoff;
    const float originalResonance = resonance;
    const float boundedResonance = juce::jlimit(0.0f, 1.0f, baseResonance);
    for (int i = 0; i < numSamples; ++i) {
        const float boundedCutoff = juce::jlimit(20.0f, 20000.0f, cutoffModulation[i]);
        samples[i] = processOutputStage(
            model.processSample(processInputStage(samples[i]), boundedCutoff, boundedResonance));
    }
    cutoff = originalCutoff;
    resonance = originalResonance;
}

#include <JuceHeader.h>

#include <cmath>

void BeforeModernCircuit::prepare(double sampleRate) {
    maximumCutoff = juce::jmin(20000.0f, static_cast<float>(sampleRate) * 0.49f);
    model.prepare(sampleRate);
}

void BeforeModernCircuit::reset() {
    model.reset();
}

void BeforeModernCircuit::setCutoffFrequency(float frequency) {
    if (!std::isfinite(frequency))
        frequency = 1000.0f;
    model.setCutoffFrequency(juce::jlimit(20.0f, maximumCutoff, frequency));
}

void BeforeModernCircuit::setResonance(float resonance) {
    model.setResonance(resonance);
}

float BeforeModernCircuit::processSample(int channel, float sample) {
    jassert(channel == 0);
    return model.processSample(sample);
}

#include "MidiParameterValue.h"

#include <cmath>

namespace {
juce::dsp::IIR::Coefficients<float>::Ptr makePreciseHighPass(double sampleRate, double frequency) {
    const auto values =
        juce::dsp::IIR::ArrayCoefficients<double>::makeHighPass(sampleRate, frequency);
    std::array<float, 6> rounded{};
    for (size_t i = 0; i < rounded.size(); ++i)
        rounded[i] = static_cast<float>(values[i]);
    return new juce::dsp::IIR::Coefficients<float>(rounded);
}
}  // namespace

//==============================================================================
BeforeVCAProcessor::BeforeVCAProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor(BusesProperties()
                         .withInput("AudioInput", juce::AudioChannelSet::mono(), true)
                         .withInput("EGInput", juce::AudioChannelSet::mono(), true)
                         .withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(apvts) {}

BeforeVCAProcessor::~BeforeVCAProcessor() {}

//==============================================================================
void BeforeVCAProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    egDepthControl.reset(sampleRate, 0.005);
    egDepthControl.setCurrentAndTargetValue(
        apvts.getRawParameterValue(ParameterIds::vcaEgDepth)->load());
    // Preserve the existing 44.1 kHz time constants; these are not hardware-calibrated.
    bufferCouplingPole =
        static_cast<float>(std::pow(static_cast<double>(0.997f), 44100.0 / sampleRate));
    outputCouplingPole =
        static_cast<float>(std::pow(static_cast<double>(0.9995f), 44100.0 / sampleRate));
    // Empirical second-order 40 Hz high-pass; not derived from the schematic's
    // 1 uF / 50 V coupling capacitor and 82 kohm series resistor.
    inputHighPass.coefficients = makePreciseHighPass(sampleRate, 40.0);
    inputHighPass.reset();
    inputHighPass.prepare({sampleRate, static_cast<juce::uint32>(samplesPerBlock), 1});

    // Initialize DC blocker
    dcBlocker.coefficients = makePreciseHighPass(sampleRate, 20.0);
    dcBlocker.reset();
    dcBlocker.prepare({sampleRate, static_cast<juce::uint32>(samplesPerBlock), 1});

    // Initialize high frequency rolloff filter
    float cutoffFreq = std::min(15000.0f, static_cast<float>(sampleRate * 0.45f));
    highFreqRolloff.coefficients =
        juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, cutoffFreq);
    highFreqRolloff.reset();
    highFreqRolloff.prepare({sampleRate, static_cast<juce::uint32>(samplesPerBlock), 1});

    // Reset state variables
    capacitorState = 0.0f;
    prevOutput = 0.0f;
    outCapacitorState = 0.0f;
}

void BeforeVCAProcessor::releaseResources() {
    capacitorState = prevOutput = outCapacitorState = 0.0f;
    inputHighPass.reset();
    dcBlocker.reset();
    highFreqRolloff.reset();
}

bool BeforeVCAProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& mainIn = layouts.getChannelSet(true, 0);
    const auto& egIn = layouts.getChannelSet(true, 1);
    const auto& mainOut = layouts.getChannelSet(false, 0);

    if (mainIn != juce::AudioChannelSet::mono())
        return false;
    if (egIn != juce::AudioChannelSet::mono())
        return false;
    if (mainOut != juce::AudioChannelSet::mono())
        return false;

    return true;
}

void BeforeVCAProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    // CS01 is a mono synth, so only process mono buffers (channel 0)
    auto audioInput = getBusBuffer(buffer, true, 0);
    auto egInput = getBusBuffer(buffer, true, 1);

    // Get parameters
    auto egDepth = apvts.getRawParameterValue(ParameterIds::vcaEgDepth)->load();
    egDepthControl.setTargetValue(egDepth);
    auto breathInput = getMidiParameterValue(apvts, ParameterIds::breathInput);
    auto breathVcaDepth = apvts.getRawParameterValue(ParameterIds::breathVca)->load();
    auto volume = getMidiParameterValue(apvts, ParameterIds::volume);
    // Precompute nonlinear volume curve once per block
    float volumeGain = std::pow(volume, 2.5f);

    // Get data pointers for mono buffers
    const auto* audioData = audioInput.getReadPointer(0);
    const auto* egData = egInput.getReadPointer(0);
    auto* outputData = buffer.getWritePointer(0);

    // Process each sample
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        // TP3: Get input sample
        float inputSample = audioData[sample];

        // Apply input stage high-pass filter (82K resistor and 1/50 capacitor)
        inputSample = inputHighPass.processSample(inputSample);

        // Apply DC blocking (additional safety)
        inputSample = dcBlocker.processSample(inputSample);

        // Get EG value
        float egValue = egData[sample];

        // Process through BeforeIG02600 VCA chip emulation
        float outputSample =
            vcaModel.processSample(inputSample, egValue, egDepthControl.getNextValue(), breathInput,
                                   breathVcaDepth, volumeGain);

        // Process through Tr7 transistor buffer emulation
        outputSample = processTr7Buffer(outputSample);

        // Process through output coupling capacitor (4.7/25)
        outputSample = processOutputCoupling(outputSample);

        // Apply high frequency rolloff (additional filtering for realism)
        outputSample = highFreqRolloff.processSample(outputSample);

        // TP5: Final output
        outputData[sample] = outputSample;
    }
}

// Tr7 transistor buffer emulation
float BeforeVCAProcessor::processTr7Buffer(float input) {
    // Output coupling capacitor (1/50) - high-pass characteristic
    const float rc1 = bufferCouplingPole;
    capacitorState = capacitorState * rc1 + input * (1.0f - rc1);
    float hpOutput = input - capacitorState;

    // Tr7 transistor non-linear characteristic
    float transistorOutput;
    if (hpOutput > 0) {
        // NPN transistor is more linear for positive signals
        transistorOutput = hpOutput * 0.95f;
    } else {
        // Slight asymmetry for negative signals
        transistorOutput = hpOutput * 0.92f;
    }

    // Transistor high-frequency response (slight treble boost)
    const float rc2 = 0.998f;
    float highFreqComponent = (transistorOutput - prevOutput) * (1.0f - rc2) * 2.0f;
    prevOutput = transistorOutput;

    return transistorOutput + highFreqComponent;
}

// Output coupling capacitor emulation (4.7/25)
float BeforeVCAProcessor::processOutputCoupling(float input) {
    // Empirical output coupling time constant, preserved from the 44.1 kHz model.
    const float rc3 = outputCouplingPole;
    outCapacitorState = outCapacitorState * rc3 + input * (1.0f - rc3);

    return input - outCapacitorState;
}

#include <cmath>

float BeforeIG02600::processSample(float input, float egValue, float egDepth, float breathInput,
                                   float breathDepth, float volumeGain) const {
    // Uncalibrated control composition retained from the existing implementation.
    float controlVoltage = (1.0f - egDepth) + (egValue * egDepth);
    controlVoltage *= (1.0f - breathDepth) + (breathInput * breathDepth);
    float output = input * (controlVoltage * volumeGain);

    // Empirical saturation; threshold and curve are not established IC specifications.
    if (std::abs(output) > 0.7f) {
        float sign = (output > 0.0f) ? 1.0f : -1.0f;
        float excess = std::abs(output) - 0.7f;
        output = sign * (0.7f + excess / (1.0f + excess * 0.5f));
    }
    return output;
}