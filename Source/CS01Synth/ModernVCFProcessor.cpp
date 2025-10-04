#include "ModernVCFProcessor.h"

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
    // Initialize filter for mono processing
    filter.reset();
    filter.setType(juce::dsp::StateVariableTPTFilter<float>::Type::lowpass);
    filter.prepare({sampleRate, static_cast<uint32>(samplesPerBlock), 1});  // Always 1 channel (mono)

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

    // Get input and output data pointers (using mono channels)
    const auto* audioData = audioInput.getReadPointer(0);
    const auto* egData = egInput.getReadPointer(0);
    const auto* lfoData = lfoInput.getNumSamples() > 0 ? lfoInput.getReadPointer(0) : nullptr;
    auto* channelData = buffer.getWritePointer(0);

    // Prepare temporary buffer
    if (buffer.getNumSamples() > processingBufferCapacity) {
        processingBuffer.setSize(1, buffer.getNumSamples());
        processingBufferCapacity = buffer.getNumSamples();
    } else if (processingBufferCapacity > 0) {
        processingBuffer.clear();
    }

    processingBuffer.copyFrom(0, 0, audioData, buffer.getNumSamples());
    float* samples = processingBuffer.getWritePointer(0);

    // Compute average modulation in semitones across the block to allow block processing.
    // This reduces per-sample filter coefficient updates while keeping modulation behaviour
    // approximately correct and allowing the filter to use processBlock (SIMD-friendly).
    int numSamples = buffer.getNumSamples();
    float accumSemitone = 0.0f;

    const float egModRangeSemitones = 36.0f;      // 3 octaves
    const float lfoModRangeSemitones = 24.0f;     // 2 octaves
    const float breathModRangeSemitones = 24.0f;  // 2 octaves

    for (int sample = 0; sample < numSamples; ++sample) {
        float egValue = egData[sample];
        float lfoValue = (lfoData != nullptr) ? lfoData[sample] : 0.0f;
        lfoValue = juce::jlimit(-1.0f, 1.0f, lfoValue);

        float egMod = egValue * egDepth * egModRangeSemitones;
        float lfoMod = lfoValue * modDepth * lfoModRangeSemitones;
        float breathMod = breathInput * breathVcfDepth * breathModRangeSemitones;

        accumSemitone += (egMod + lfoMod + breathMod);
    }

    // Average semitone modulation for the block
    float avgSemitone = (numSamples > 0) ? (accumSemitone / static_cast<float>(numSamples)) : 0.0f;

    // Convert average semitone modulation to frequency ratio and compute block cutoff
    float avgFreqRatio = std::exp2f(avgSemitone / 12.0f);
    float blockCutoffHz = juce::jlimit(20.0f, 20000.0f, cutoff * avgFreqRatio);

    // Apply averaged filter parameters (per-block)
    filter.setCutoffFrequency(blockCutoffHz);
    filter.setResonance(resonance);

    // Process the whole block using JUCE DSP block API (SIMD-friendly)
    {
        juce::dsp::AudioBlock<float> audioBlock(reinterpret_cast<float*>(processingBuffer.getWritePointer(0)),
                                                1, static_cast<size_t>(numSamples));
        juce::dsp::ProcessContextReplacing<float> context(audioBlock);
        filter.process(context);
    }

    // Copy processed samples back to output
    for (int sample = 0; sample < numSamples; ++sample) {
        channelData[sample] = processingBuffer.getReadPointer(0)[sample];
    }
}
