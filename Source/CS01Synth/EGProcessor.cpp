#include "EGProcessor.h"

//==============================================================================
EGProcessor::EGProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::mono(), true)),
      apvts(apvts)
{
}

EGProcessor::~EGProcessor()
{
}

//==============================================================================
void EGProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    adsr.setSampleRate(sampleRate);
    updateADSR();
}

void EGProcessor::releaseResources()
{
}

bool EGProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    return true;
}

void EGProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    updateADSR();

    // CS01 is a mono synth, so only generate mono output
    buffer.clear();

    // Skip MIDI message processing (already processed in MidiProcessor)
    // MIDI messages are processed by startEnvelope/releaseEnvelope methods

    // Process mono output (channel 0) only
    auto* channelData = buffer.getWritePointer(0);
    
    // Static variable for envelope shaping
    static float prevSample = 0.0f;
    
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        // Get the raw envelope sample
        float envSample = adsr.getNextSample();
        
        // Apply FET non-linear characteristics (FET1 in the circuit)
        // 1. Slight compression at low levels (FET threshold effect)
        if (envSample < 0.1f)
            envSample = envSample * 0.7f + 0.03f * std::sqrt(envSample);
            
        // 2. Slight expansion at mid levels (FET's square-law region)
        else if (envSample < 0.7f)
            envSample = envSample * (1.0f + (envSample - 0.1f) * 0.15f);
            
        // 3. Soft saturation at high levels (FET saturation region)
        else
            envSample = 0.7f + (1.0f - 0.7f) * std::tanh((envSample - 0.7f) / (1.0f - 0.7f) * 2.0f);
        
        // 4. Apply transistor buffer effect (Tr14)
        // - Slight high-pass characteristic due to coupling
        // - Small time constant for fast transients
        const float alpha = 0.99f; // Time constant
        
        // Simple first-order high-pass filter
        float highPassComponent = (envSample - prevSample) * (1.0f - alpha);
        prevSample = envSample * alpha + prevSample * (1.0f - alpha);
        
        // Add a small amount of high-pass to enhance transients
        envSample = envSample * 0.95f + highPassComponent * 2.0f;
        
        // Ensure the output stays in valid range
        envSample = juce::jlimit(0.0f, 1.0f, envSample);
        
        channelData[sample] = envSample;
    }
}

void EGProcessor::updateADSR()
{
    juce::ADSR::Parameters adsrParams;
    adsrParams.attack  = apvts.getRawParameterValue(ParameterIds::attack)->load();
    adsrParams.decay   = apvts.getRawParameterValue(ParameterIds::decay)->load();
    adsrParams.sustain = apvts.getRawParameterValue(ParameterIds::sustain)->load();
    adsrParams.release = apvts.getRawParameterValue(ParameterIds::release)->load();
    
    adsr.setParameters(adsrParams);
}
