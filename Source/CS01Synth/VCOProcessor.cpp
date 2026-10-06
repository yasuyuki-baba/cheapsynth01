#include "CS01Synth/VCOProcessor.h"

#include "MidiParameterValue.h"

#include "CS01Synth/SynthConstants.h"

VCOProcessor::VCOProcessor(juce::AudioProcessorValueTreeState& vts, bool isNoiseMode)
    : AudioProcessor(BusesProperties()
                         .withInput("LFOInput", juce::AudioChannelSet::mono(), true)
                         .withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(vts),
      toneGenerator(std::make_unique<ToneGenerator>(apvts)),
      noiseGenerator(std::make_unique<NoiseGenerator>(apvts)),
      currentGenerator(nullptr) {
    // Register as listener for feet parameter
    apvts.addParameterListener(ParameterIds::feet, this);

    // Set the initial generator type based on current parameter value
    auto* feetParam =
        dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParameterIds::feet));
    if (feetParam != nullptr) {
        isNoiseMode = feetParam->getIndex() == static_cast<int>(Feet::WhiteNoise);
    }
    requestedNoiseMode.store(isNoiseMode);
    currentGenerator = isNoiseMode ? static_cast<ISoundGenerator*>(noiseGenerator.get())
                                   : static_cast<ISoundGenerator*>(toneGenerator.get());
}

VCOProcessor::~VCOProcessor() {
    // Remove parameter listener
    apvts.removeParameterListener(ParameterIds::feet, this);
}

void VCOProcessor::parameterChanged(const juce::String& parameterID, float newValue) {
    if (parameterID == ParameterIds::feet)
        requestedNoiseMode.store(static_cast<int>(newValue) == static_cast<int>(Feet::WhiteNoise));
}

void VCOProcessor::applyPendingGeneratorChange() {
    ISoundGenerator* next = requestedNoiseMode.load()
                                ? static_cast<ISoundGenerator*>(noiseGenerator.get())
                                : static_cast<ISoundGenerator*>(toneGenerator.get());
    if (next == currentGenerator)
        return;

    const auto state = currentGenerator->getPlaybackState();
    currentGenerator->stopNote(false);
    // Both sources are prepared in advance. Reset DSP state without reallocating.
    if (next == toneGenerator.get())
        toneGenerator->reset();
    else
        noiseGenerator->reset();
    currentGenerator = next;
    currentGenerator->restorePlaybackState(state);
    if (onGeneratorTypeChanged)
        onGeneratorTypeChanged();
}

void VCOProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    const juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32)samplesPerBlock,
                                      (juce::uint32)getTotalNumOutputChannels()};

    // Prepare both generators
    if (toneGenerator)
        toneGenerator->prepare(spec);

    if (noiseGenerator)
        noiseGenerator->prepare(spec);

    noiseGenerator->reset();
    applyPendingGeneratorChange();
}

bool VCOProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& mainOut = layouts.getChannelSet(false, 0);

    // Output must always be mono
    if (mainOut != juce::AudioChannelSet::mono())
        return false;

    // For Tone type, check LFO input
    const auto& mainIn = layouts.getChannelSet(true, 0);
    if (mainIn != juce::AudioChannelSet::mono())
        return false;

    return true;
}

void VCOProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    applyPendingGeneratorChange();
    if (!currentGenerator) {
        return;
    }

    // Process audio block - CS01 is a mono synth, so processing is simplified

    // Process LFO input for Tone generator
    if (currentGenerator == toneGenerator.get()) {
        // LFO input is always mono (channel 0)
        auto lfoInput = getBusBuffer(buffer, true, 0);
        auto modDepth = getMidiParameterValue(apvts, ParameterIds::modDepth);
        const float lfoModRangeSemitones = 1.0f;
        toneGenerator->updateBlockRateParameters();
        auto* output = buffer.getWritePointer(0);
        // Input and output alias: consume each modulation sample before replacing it.
        const auto* modulation = lfoInput.getReadPointer(0);
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            toneGenerator->setLfoValue(modulation[i] * modDepth * lfoModRangeSemitones);
            output[i] = 0.0f;
            if (toneGenerator->isActive())
                toneGenerator->renderNextBlock(buffer, i, 1);
        }
        return;
    }

    // Clear the buffer
    buffer.clear();

    // Sound generation using the current generator
    if (currentGenerator->isActive()) {
        // Process mono output
        currentGenerator->renderNextBlock(buffer, 0, buffer.getNumSamples());
    }
    // If not active, buffer remains cleared

    // Pass MIDI buffer through (processing is done in MidiProcessor)
}
