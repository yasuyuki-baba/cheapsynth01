#include "CS01Synth/VCAProcessor.h"

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

void VCAProcessor::releaseResources() {
    capacitorState = prevOutput = outCapacitorState = 0.0f;
    inputHighPass.reset();
    dcBlocker.reset();
    highFreqRolloff.reset();
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

        // Process through IG02600 VCA chip emulation
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
float VCAProcessor::processTr7Buffer(float input) {
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
float VCAProcessor::processOutputCoupling(float input) {
    // Empirical output coupling time constant, preserved from the 44.1 kHz model.
    const float rc3 = outputCouplingPole;
    outCapacitorState = outCapacitorState * rc3 + input * (1.0f - rc3);

    return input - outCapacitorState;
}
