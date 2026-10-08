#include "CS01Synth/ModernVCFProcessor.h"

#include "MidiParameterValue.h"

#include <cmath>

//==============================================================================
ModernVCFProcessor::ModernVCFProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor(BusesProperties()
                         .withInput("AudioInput", juce::AudioChannelSet::mono(), true)
                         .withInput("EGInput", juce::AudioChannelSet::mono(), true)
                         .withInput("LFOInput", juce::AudioChannelSet::mono(), true)
                         .withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(apvts) {}

ModernVCFProcessor::~ModernVCFProcessor() {}

//==============================================================================
void ModernVCFProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    processingSampleRate = sampleRate;
    // Initialize filter for mono processing
    filter.reset();
    filter.prepare(sampleRate);

    // Pre-allocate temporary buffer to avoid reallocations per block
    if (samplesPerBlock > processingBufferCapacity) {
        processingBuffer.setSize(1, samplesPerBlock);
        processingBufferCapacity = samplesPerBlock;
    } else if (processingBufferCapacity > 0) {
        processingBuffer.clear();
    }
}

void ModernVCFProcessor::releaseResources() {
    // Reset filter
    filter.reset();
}

bool ModernVCFProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& mainIn = layouts.getChannelSet(true, 0);
    const auto& egIn = layouts.getChannelSet(true, 1);
    const auto& lfoIn = layouts.getChannelSet(true, 2);
    const auto& mainOut = layouts.getChannelSet(false, 0);

    if (mainIn != juce::AudioChannelSet::mono())
        return false;
    if (egIn != juce::AudioChannelSet::mono())
        return false;
    if (lfoIn != juce::AudioChannelSet::mono())
        return false;
    if (mainOut != juce::AudioChannelSet::mono())
        return false;

    return true;
}

void ModernVCFProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    auto audioInput = getBusBuffer(buffer, true, 0);
    auto egInput = getBusBuffer(buffer, true, 1);
    auto lfoInput = getBusBuffer(buffer, true, 2);

    // Get parameters
    auto cutoffParam = getMidiParameterValue(apvts, ParameterIds::cutoff);
    auto resonanceParam = getMidiParameterValue(apvts, ParameterIds::resonance);
    auto egDepth = getCurrentParameterValue(apvts, ParameterIds::vcfEgDepth);
    auto modDepth = getMidiParameterValue(apvts, ParameterIds::modDepth);
    auto breathInput = getMidiParameterValue(apvts, ParameterIds::breathInput);
    auto breathVcfDepth = getCurrentParameterValue(apvts, ParameterIds::breathVcf);

    // Cutoff frequency calculation
    float cutoff = calculateCutoffFrequency(cutoffParam);

    // Ensure minimum cutoff frequency
    cutoff = juce::jmax(20.0f, cutoff);

    // Resonance calculation
    float resonance = calculateResonance(resonanceParam);

    // Get input and output data pointers (using mono channels)
    const auto* audioData = audioInput.getReadPointer(0);
    const auto* egData = egInput.getReadPointer(0);
    const auto* lfoData = lfoInput.getNumSamples() > 0 ? lfoInput.getReadPointer(0) : nullptr;

    // Expect processingBuffer to be preallocated in prepareToPlay; avoid reallocating on audio
    // thread
    jassert(buffer.getNumSamples() <= processingBufferCapacity);
    if (processingBufferCapacity > 0) {
        processingBuffer.clear();
    }

    // Copy input into the preallocated processing buffer
    processingBuffer.copyFrom(0, 0, audioData, buffer.getNumSamples());

    // The schematic's EG/LFO/breath controls act on the VCF continuously.
    // Follow Original's sample-wise control path rather than averaging a block.
    const int numSamples = buffer.getNumSamples();
    const float egModRangeSemitones = 36.0f;  // Empirical, not circuit-calibrated.
    const float lfoModRangeSemitones = 24.0f;
    const float breathModRangeSemitones = 24.0f;
    // Implementation safety bound, separate from the empirical modulation spans.
    const float maximumCutoff =
        juce::jmin(20000.0f, static_cast<float>(processingSampleRate) * 0.49f);
    auto* samples = processingBuffer.getWritePointer(0);
    filter.setResonance(resonance);

    for (int sample = 0; sample < numSamples; ++sample) {
        const float lfoValue =
            lfoData != nullptr ? juce::jlimit(-1.0f, 1.0f, lfoData[sample]) : 0.0f;
        const float semitones = egData[sample] * egDepth * egModRangeSemitones +
                                lfoValue * modDepth * lfoModRangeSemitones +
                                breathInput * breathVcfDepth * breathModRangeSemitones;
        float modulatedCutoff = cutoff * std::exp2(semitones / 12.0f);
        // Numerical safety for modulation; not a hardware control law.
        if (!std::isfinite(modulatedCutoff))
            modulatedCutoff = cutoff;
        const float frequency = juce::jlimit(20.0f, maximumCutoff, modulatedCutoff);
        filter.setCutoffFrequency(frequency);
        samples[sample] = filter.processSample(0, samples[sample]);
    }

    // Copy processed samples back to output efficiently
    buffer.copyFrom(0, 0, processingBuffer, 0, 0, numSamples);
}
