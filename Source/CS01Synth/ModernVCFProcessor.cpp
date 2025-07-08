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
    filter.prepare(
        {sampleRate, static_cast<uint32>(samplesPerBlock), 1});  // Always 1 channel (mono)

    // Initialize temporary buffer
    processingBuffer.setSize(1, samplesPerBlock);
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
    if (processingBuffer.getNumSamples() < buffer.getNumSamples())
        processingBuffer.setSize(1, buffer.getNumSamples(), false, false, true);

    processingBuffer.clear();
    processingBuffer.copyFrom(0, 0, audioData, buffer.getNumSamples());
    float* samples = processingBuffer.getWritePointer(0);

    // Process each sample
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        float egValue = egData[sample];
        // Use LFO data only if available
        float lfoValue = (lfoData != nullptr) ? lfoData[sample] : 0.0f;

        // Limit LFO value to range [-1.0, 1.0]
        lfoValue = juce::jlimit(-1.0f, 1.0f, lfoValue);

        // Calculate modulation effects
        const float egModRangeSemitones = 36.0f;      // 3 octaves
        const float lfoModRangeSemitones = 24.0f;     // 2 octaves
        const float breathModRangeSemitones = 24.0f;  // 2 octaves

        // Base cutoff frequency
        float baseCutoff = cutoff;

        // EG modulation
        float egMod = egValue * egDepth * egModRangeSemitones;
        float egModFreqRatio = std::pow(2.0f, egMod / 12.0f);

        // LFO modulation - using multiplicative method
        float lfoMod = lfoValue * modDepth * lfoModRangeSemitones;
        float lfoModFreqRatio = std::pow(2.0f, lfoMod / 12.0f);

        // Breath modulation - using multiplicative method
        float breathMod = breathInput * breathVcfDepth * breathModRangeSemitones;
        float breathModFreqRatio = std::pow(2.0f, breathMod / 12.0f);

        // Apply all modulations - unified multiplicative method
        float modulatedCutoffHz =
            baseCutoff * egModFreqRatio * lfoModFreqRatio * breathModFreqRatio;

        // Check for NaN or Infinity
        if (std::isnan(modulatedCutoffHz) || std::isinf(modulatedCutoffHz)) {
            modulatedCutoffHz = baseCutoff;  // Use base value if there's a problem
        }

        modulatedCutoffHz = juce::jlimit(20.0f, 20000.0f, modulatedCutoffHz);

        // Update filter parameters
        filter.setCutoffFrequency(modulatedCutoffHz);
        filter.setResonance(resonance);

        // Process sample
        samples[sample] = filter.processSample(0, samples[sample]);
    }

    // Copy processed samples back to output
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        channelData[sample] = samples[sample];
    }
}
