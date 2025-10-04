#include "OriginalVCFProcessor.h"

//==============================================================================
OriginalVCFProcessor::OriginalVCFProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor(BusesProperties()
                         .withInput("AudioInput", juce::AudioChannelSet::mono(), true)
                         .withInput("EGInput", juce::AudioChannelSet::mono(), true)
                         .withInput("LFOInput", juce::AudioChannelSet::mono(), true)
                         .withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(apvts) {}

OriginalVCFProcessor::~OriginalVCFProcessor() {}

//==============================================================================
void OriginalVCFProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    // Initialize filter
    filter.reset();
    filter.prepare(sampleRate);

    // Pre-allocate buffer for modulation values to avoid reallocations per block
    if (samplesPerBlock > modulationBufferCapacity) {
        modulationBuffer.free();
        modulationBuffer.allocate(samplesPerBlock, true);
        modulationBufferCapacity = samplesPerBlock;
    } else if (modulationBufferCapacity > 0) {
        // clear previously allocated portion (only need to clear if reused)
        modulationBuffer.clear(samplesPerBlock);
    }
}

void OriginalVCFProcessor::releaseResources() {
    // Reset filter
    filter.reset();
}

bool OriginalVCFProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
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

void OriginalVCFProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    // Original filter is completely mono, so only process channel 0
    auto audioInput = getBusBuffer(buffer, true, 0);
    auto egInput = getBusBuffer(buffer, true, 1);
    auto lfoInput = getBusBuffer(buffer, true, 2);

    // Get parameters
    auto cutoffParam = apvts.getRawParameterValue(ParameterIds::cutoff)->load();
    auto resonanceParam = apvts.getRawParameterValue(ParameterIds::resonance)->load();
    auto egDepth = apvts.getRawParameterValue(ParameterIds::vcfEgDepth)->load();
    auto modDepth = apvts.getRawParameterValue(ParameterIds::modDepth)->load();
    auto breathInput = apvts.getRawParameterValue(ParameterIds::breathInput)->load();
    auto breathVcfDepth = apvts.getRawParameterValue(ParameterIds::breathVcf)->load();

    // Cutoff frequency calculation
    float cutoff = calculateCutoffFrequency(cutoffParam);

    // Ensure minimum cutoff frequency
    cutoff = juce::jmax(20.0f, cutoff);

    // Resonance calculation
    float resonance = calculateResonance(resonanceParam);

    // Get pointers for mono processing
    const auto* egData = egInput.getReadPointer(0);
    const auto* lfoData = lfoInput.getNumSamples() > 0 ? lfoInput.getReadPointer(0) : nullptr;
    const auto* audioData = audioInput.getReadPointer(0);
    auto* outputData = buffer.getWritePointer(0);

    int numSamples = buffer.getNumSamples();

    // Ensure modulation buffer capacity
    if (numSamples > modulationBufferCapacity) {
        modulationBuffer.free();
        modulationBuffer.allocate(numSamples, true);
        modulationBufferCapacity = numSamples;
    } else if (modulationBufferCapacity > 0) {
        modulationBuffer.clear(numSamples);
    }

    // Calculate cutoff frequency for each sample
    for (int sample = 0; sample < numSamples; ++sample) {
        float egValue = egData[sample];
        float lfoValue = (lfoData != nullptr) ? lfoData[sample] : 0.0f;

        // Modulation effect calculation constants
        const float egModRangeSemitones = 36.0f;      // 3 octaves
        const float lfoModRangeSemitones = 24.0f;     // 2 octaves
        const float breathModRangeSemitones = 24.0f;  // 2 octaves

        // Base cutoff frequency
        float baseCutoff = cutoff;

        // EG modulation
        float egMod = egValue * egDepth * egModRangeSemitones;
        float egModFreqRatio = std::exp2f(egMod / 12.0f);

        // LFO modulation
        float lfoMod = lfoValue * modDepth * lfoModRangeSemitones;
        float lfoModFreqRatio = std::exp2f(lfoMod / 12.0f);

        // Breath modulation
        float breathMod = breathInput * breathVcfDepth * breathModRangeSemitones;
        float breathModFreqRatio = std::exp2f(breathMod / 12.0f);

        // Apply all modulations
        float modulatedCutoffHz =
            baseCutoff * egModFreqRatio * lfoModFreqRatio * breathModFreqRatio;

        // Check for NaN or Infinity
        if (std::isnan(modulatedCutoffHz) || std::isinf(modulatedCutoffHz)) {
            modulatedCutoffHz = baseCutoff;
        }

        modulatedCutoffHz = juce::jlimit(20.0f, 20000.0f, modulatedCutoffHz);

        // Store modulated cutoff for this sample
        modulationBuffer[sample] = modulatedCutoffHz;
    }

    // Copy audio input to output buffer
    buffer.copyFrom(0, 0, audioData, buffer.getNumSamples());

    // Process using filter
    filter.processBlock(outputData, buffer.getNumSamples(), modulationBuffer, resonance);
}
