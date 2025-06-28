#include "NoiseProcessor.h"

NoiseProcessor::NoiseProcessor(juce::AudioProcessorValueTreeState& vts)
    : AudioProcessor(BusesProperties()
                        .withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(vts)
{
}

void NoiseProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumOutputChannels() };
    noiseFilter.prepare(spec);
    
    // Limit frequency to not exceed Nyquist frequency
    float cutoffFreq = std::min(12000.0f, static_cast<float>(spec.sampleRate * 0.45f));
    
    *noiseFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(spec.sampleRate, cutoffFreq);
}

bool NoiseProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getChannelSet(false, 0);
    
    // Only support mono output
    if (mainOut != juce::AudioChannelSet::mono())
        return false;
    
    return true;
}

void NoiseProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    
    // Clear buffer first
    buffer.clear();
    
    // Only generate noise if note is on
    if (isActive())
    {
        // Process tail off if needed
        if (tailOff)
        {
            tailOffCounter += buffer.getNumSamples();
            
            // Check if tail off is complete
            if (tailOffCounter >= tailOffDuration)
            {
                tailOff = false;
                noteOn = false;
            }
        }
        
        // Generate white noise
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float whiteNoise = random.nextFloat() * 2.0f - 1.0f;
            float filteredNoise = noiseFilter.processSample(whiteNoise);
            
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                buffer.setSample(channel, sample, filteredNoise);
            }
        }
    }
}

// INoteHandler implementation
void NoiseProcessor::startNote(int midiNoteNumber, float velocity, int currentPitchWheelPosition)
{
    currentlyPlayingNote = midiNoteNumber;
    pitchWheelValue = currentPitchWheelPosition;
    noteOn = true;
    tailOff = false;
}

void NoiseProcessor::stopNote(bool allowTailOff)
{
    if (allowTailOff)
    {
        // Start tail off
        tailOff = true;
        
        // Get release time from parameter (convert to samples)
        float releaseSecs = apvts.getRawParameterValue(ParameterIds::release)->load();
        tailOffDuration = static_cast<int>(releaseSecs * sampleRate);
        tailOffCounter = 0;
    }
    else
    {
        // Stop immediately
        noteOn = false;
        tailOff = false;
    }
    
    currentlyPlayingNote = 0;
}

void NoiseProcessor::changeNote(int midiNoteNumber)
{
    currentlyPlayingNote = midiNoteNumber;
    // For noise, we don't need to change anything else when the note changes
}

void NoiseProcessor::pitchWheelMoved(int newPitchWheelValue)
{
    pitchWheelValue = newPitchWheelValue;
    // For noise, pitch wheel doesn't affect the sound
}

bool NoiseProcessor::isActive() const
{
    return noteOn || (tailOff && tailOffCounter < tailOffDuration);
}

int NoiseProcessor::getCurrentlyPlayingNote() const
{
    return currentlyPlayingNote;
}
