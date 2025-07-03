#include "NoiseGenerator.h"

NoiseGenerator::NoiseGenerator(juce::AudioProcessorValueTreeState& apvts)
    : apvts(apvts)
{
}

void NoiseGenerator::prepare(const juce::dsp::ProcessSpec& spec)
{
    noiseFilter.prepare(spec);
    sampleRate = spec.sampleRate;
    
    // Limit frequency to not exceed Nyquist frequency
    float cutoffFreq = std::min(12000.0f, static_cast<float>(spec.sampleRate * 0.45f));
    
    *noiseFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(spec.sampleRate, cutoffFreq);
}

void NoiseGenerator::renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    // Only generate noise if note is on
    if (isActive())
    {
        // Process tail off if needed
        if (tailOff)
        {
            tailOffCounter += numSamples;
            
            // Check if tail off is complete
            if (tailOffCounter >= tailOffDuration)
            {
                tailOff = false;
                noteOn = false;
            }
        }
        
        // Generate white noise
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float whiteNoise = random.nextFloat() * 2.0f - 1.0f;
            float filteredNoise = noiseFilter.processSample(whiteNoise);
            
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                buffer.setSample(channel, startSample + sample, filteredNoise);
            }
        }
    }
    // If not active, nothing to do
}

// INoteHandler implementation
void NoiseGenerator::startNote(int midiNoteNumber, float velocity, int currentPitchWheelPosition)
{
    currentlyPlayingNote = midiNoteNumber;
    pitchWheelValue = currentPitchWheelPosition;
    noteOn = true;
    tailOff = false;
    
}

void NoiseGenerator::stopNote(bool allowTailOff)
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

void NoiseGenerator::changeNote(int midiNoteNumber)
{
    currentlyPlayingNote = midiNoteNumber;
    // For noise, we don't need to change anything else when the note changes
}

void NoiseGenerator::pitchWheelMoved(int newPitchWheelValue)
{
    pitchWheelValue = newPitchWheelValue;
    // For noise, pitch wheel doesn't affect the sound
}

bool NoiseGenerator::isActive() const
{
    return noteOn || (tailOff && tailOffCounter < tailOffDuration);
}

int NoiseGenerator::getCurrentlyPlayingNote() const
{
    return currentlyPlayingNote;
}
