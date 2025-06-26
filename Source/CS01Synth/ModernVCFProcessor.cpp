#include "ModernVCFProcessor.h"

//==============================================================================
ModernVCFProcessor::ModernVCFProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor (BusesProperties()
                          .withInput  ("AudioInput",  juce::AudioChannelSet::mono(), true)
                          .withInput  ("EGInput", juce::AudioChannelSet::mono(), true)
                          .withInput  ("LFOInput", juce::AudioChannelSet::mono(), true)
                          .withOutput ("Output", juce::AudioChannelSet::mono(), true)),
      apvts(apvts)
{
}

ModernVCFProcessor::~ModernVCFProcessor()
{
}

//==============================================================================
void ModernVCFProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Prepare filters according to channel count
    const int numChannels = getMainBusNumOutputChannels();
    filters.resize(numChannels);
    
    // Initialize each filter
    for (auto& filter : filters)
    {
        filter.reset();
        filter.setType(juce::dsp::StateVariableTPTFilter<float>::Type::lowpass);
        filter.prepare({sampleRate, static_cast<uint32>(samplesPerBlock), static_cast<uint32>(numChannels)});
    }
}

void ModernVCFProcessor::releaseResources()
{
    // Reset each filter
    for (auto& filter : filters)
    {
        filter.reset();
    }
}

bool ModernVCFProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getChannelSet(true, 0);
    const auto& egIn   = layouts.getChannelSet(true, 1);
    const auto& lfoIn  = layouts.getChannelSet(true, 2);
    const auto& mainOut = layouts.getChannelSet(false, 0);

    if (mainIn != juce::AudioChannelSet::mono()) return false;
    if (egIn != juce::AudioChannelSet::mono()) return false;
    if (lfoIn != juce::AudioChannelSet::mono()) return false;
    if (mainOut != juce::AudioChannelSet::mono()) return false;

    return true;
}

void ModernVCFProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
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

    // Cutoff frequency mapping
    float cutoff = mapCutoffFrequency(cutoffParam);
    
    // Ensure minimum cutoff frequency
    cutoff = juce::jmax(20.0f, cutoff);
    
    // Resonance mapping
    float resonance = mapResonance(resonanceParam);

    // Check sample count of LFO input buffer
    const int lfoSamples = lfoInput.getNumSamples();

    const auto* egData = egInput.getReadPointer(0);
    const auto* lfoData = lfoSamples > 0 ? lfoInput.getReadPointer(0) : nullptr;

    // Process each channel
    for (int channel = 0; channel < audioInput.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        const auto* audioData = audioInput.getReadPointer(channel);
        
        // Ensure filter instance
        if (channel >= filters.size())
            filters.resize(channel + 1);
            
        // Create temporary buffer for audio processing
        juce::AudioBuffer<float> tempBuffer(1, buffer.getNumSamples());
        tempBuffer.copyFrom(0, 0, audioData, buffer.getNumSamples());
        float* samples = tempBuffer.getWritePointer(0);
        
        // Process each sample
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float egValue = egData[sample];
            // Use LFO data only if available
            float lfoValue = (lfoData != nullptr) ? lfoData[sample] : 0.0f;

            // Calculate modulation effects
            const float egModRangeSemitones = 36.0f; // 3 octaves
            const float lfoModRangeSemitones = 18.0f; // 1.5 octaves
            const float breathModRangeSemitones = 24.0f; // 2 octaves

            // Base cutoff frequency
            float baseCutoff = cutoff;
            
            // EG modulation
            float egMod = egValue * egDepth * egModRangeSemitones;
            float egModFreqRatio = std::pow(2.0f, egMod / 12.0f);
            
            // LFO modulation
            float lfoMod = lfoValue * modDepth * lfoModRangeSemitones;
            float lfoModFreqRatio = std::pow(2.0f, lfoMod / 12.0f);
            
            // Breath modulation
            float breathMod = breathInput * breathVcfDepth * breathModRangeSemitones;
            float breathModFreqRatio = std::pow(2.0f, breathMod / 12.0f);
            
            // Apply all modulations
            float modulatedCutoffHz = baseCutoff * egModFreqRatio * lfoModFreqRatio * breathModFreqRatio;
            
            // Check for NaN or Infinity
            if (std::isnan(modulatedCutoffHz) || std::isinf(modulatedCutoffHz)) {
                modulatedCutoffHz = baseCutoff; // Use base value if there's a problem
            }
            
            modulatedCutoffHz = juce::jlimit(20.0f, 20000.0f, modulatedCutoffHz);
            
            // Update filter parameters
            filters[channel].setCutoffFrequency(modulatedCutoffHz);
            filters[channel].setResonance(resonance);
            
            // Process sample
            samples[sample] = filters[channel].processSample(0, samples[sample]);
        }
        
        // Copy processed samples back to output
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] = samples[sample];
        }
    }
}
