#include "CS01Synth/VCAProcessor.h"
#include "CS01Synth/EGProcessor.h"

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
VCAProcessor::VCAProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor(BusesProperties()
                         .withInput("AudioInput", juce::AudioChannelSet::mono(), true)
                         .withInput("EGInput", juce::AudioChannelSet::mono(), true)
                         .withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(apvts) {}

VCAProcessor::~VCAProcessor() {}

//==============================================================================
void VCAProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    egDepthControl.reset(sampleRate, 0.005);
    egDepthControl.setCurrentAndTargetValue(
        getCurrentParameterValue(apvts, ParameterIds::vcaEgDepth));
    // Preserve the existing 44.1 kHz time constants; these are not hardware-calibrated.
    bufferInputCoupling.prepare(sampleRate, EmpiricalParameters::bufferCouplingReferencePole);
    outputCoupling.prepare(sampleRate, EmpiricalParameters::outputCouplingReferencePole);
    // Empirical second-order 40 Hz high-pass; not derived from the schematic's
    // 1 uF / 50 V coupling capacitor and 82 kohm series resistor.
    empiricalInputCoupling.coefficients =
        makePreciseHighPass(sampleRate, EmpiricalParameters::inputCouplingHz);
    empiricalInputCoupling.reset();
    empiricalInputCoupling.prepare({sampleRate, static_cast<juce::uint32>(samplesPerBlock), 1});

    // Initialize DC blocker
    safetyDcBlocker.coefficients = makePreciseHighPass(sampleRate, SafetyParameters::dcBlockerHz);
    safetyDcBlocker.reset();
    safetyDcBlocker.prepare({sampleRate, static_cast<juce::uint32>(samplesPerBlock), 1});

    // Initialize implementation safety/rolloff
    float cutoffFreq =
        std::min(SafetyParameters::rolloffHz,
                 static_cast<float>(sampleRate * SafetyParameters::maximumRateFraction));
    safetyHighFreqRolloff.coefficients =
        juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, cutoffFreq);
    safetyHighFreqRolloff.reset();
    safetyHighFreqRolloff.prepare({sampleRate, static_cast<juce::uint32>(samplesPerBlock), 1});

    // Reset state variables
    bufferInputCoupling.reset();
    tr7Buffer.reset();
    outputCoupling.reset();
}

void VCAProcessor::releaseResources() {
    bufferInputCoupling.reset();
    tr7Buffer.reset();
    outputCoupling.reset();
    empiricalInputCoupling.reset();
    safetyDcBlocker.reset();
    safetyHighFreqRolloff.reset();
}

bool VCAProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
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

void VCAProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    // CS01 is a mono synth, so only process mono buffers (channel 0)
    auto audioInput = getBusBuffer(buffer, true, 0);
    auto egInput = getBusBuffer(buffer, true, 1);

    // Get parameters
    auto egDepth = getCurrentParameterValue(apvts, ParameterIds::vcaEgDepth);
    if (!std::isfinite(egDepth))
        egDepth = 0.0f;
    egDepthControl.setTargetValue(egDepth);
    auto breathInput = getMidiParameterValue(apvts, ParameterIds::breathInput);
    auto breathVcaDepth = getCurrentParameterValue(apvts, ParameterIds::breathVca);
    auto volume = getMidiParameterValue(apvts, ParameterIds::volume);
    // Precompute nonlinear volume curve once per block
    float volumeGain = std::pow(volume, EmpiricalParameters::volumeExponent);

    // Get data pointers for mono buffers
    const auto* audioData = audioInput.getReadPointer(0);
    const auto* egData = egInput.getReadPointer(0);
    auto* outputData = buffer.getWritePointer(0);

    // Process each sample
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        // TP3: Get input sample
        float inputSample = audioData[sample];

        if (!std::isfinite(inputSample) || !std::isfinite(egData[sample])) {
            releaseResources();
            outputData[sample] = 0.0f;
            continue;
        }

        // Apply empirical input coupling
        inputSample = empiricalInputCoupling.processSample(inputSample);

        // Apply DC blocking (additional safety)
        inputSample = safetyDcBlocker.processSample(inputSample);

        // Get EG value
        float egValue = egData[sample];

        // Process through IG02600 behavioral gain/nonlinearity
        float outputSample = vcaModel.processSample(
            inputSample, egValue, egDepthControl.getNextValue(), breathInput, breathVcaDepth,
            volumeGain,
            noteGateSource != nullptr ? noteGateSource->getNoteGateForSample(sample) : 1.0f);

        // Process through empirical buffer-input coupling and Tr7 coloration
        outputSample = tr7Buffer.processSample(bufferInputCoupling.processSample(outputSample));

        // Process through empirical output coupling (external 4.7 uF / 25 V capacitor)
        outputSample = outputCoupling.processSample(outputSample);

        // Apply implementation safety/rolloff (not a verified hardware response)
        outputSample = safetyHighFreqRolloff.processSample(outputSample);

        // TP5: Final output
        if (!std::isfinite(outputSample)) {
            releaseResources();
            outputSample = 0.0f;
        }
        outputData[sample] = outputSample;
    }
}
