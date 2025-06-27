#include "SynthVoice.h"
#include "../Parameters.h"
#include "SynthConstants.h"

SynthVoice::SynthVoice(juce::AudioProcessorValueTreeState& apvts)
    : apvts(apvts)
{
    toneGenerator = std::make_unique<ToneGenerator>(apvts);
}

void SynthVoice::prepare(const juce::dsp::ProcessSpec& spec)
{
    toneGenerator->prepare(spec);
    sampleRate = spec.sampleRate;
}

void SynthVoice::startNote(int midiNoteNumber, float velocity, int currentPitchWheelPosition)
{
    currentlyPlayingNote = midiNoteNumber;
    setNote(midiNoteNumber, false); // isLegato = false
    pitchWheelMoved(currentPitchWheelPosition);
    noteOn = true;
}

void SynthVoice::stopNote(bool allowTailOff)
{
    noteOn = false;
    currentlyPlayingNote = 0;
    
    if (allowTailOff)
    {
        tailOff = true;
        // Get release time from parameter (convert to samples)
        float releaseSecs = apvts.getRawParameterValue(ParameterIds::release)->load();
        tailOffDuration = static_cast<int>(releaseSecs * sampleRate);
        tailOffCounter = 0;
    }
    else
    {
        tailOff = false;
    }
}

bool SynthVoice::isActive() const
{
    return noteOn || (tailOff && tailOffCounter < tailOffDuration);
}

void SynthVoice::pitchWheelMoved(int newPitchWheelValue)
{
    auto upRange = apvts.getRawParameterValue(ParameterIds::pitchBendUpRange)->load();
    auto downRange = apvts.getRawParameterValue(ParameterIds::pitchBendDownRange)->load();

    auto bendValue = juce::jmap(static_cast<float>(newPitchWheelValue), 0.0f, 16383.0f, -1.0f, 1.0f);
    
    float pitchOffset = 0.0f;
    if (bendValue > 0)
        pitchOffset = bendValue * upRange;
    else
        pitchOffset = bendValue * downRange;

    if (toneGenerator)
        toneGenerator->setPitchBend(pitchOffset);
}

void SynthVoice::setLfoValue(float lfoValue)
{
    if (toneGenerator)
        toneGenerator->setLfoValue(lfoValue);
}

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (!isActive())
        return;

    updateBlockRateParameters();
    
    juce::dsp::AudioBlock<float> block(outputBuffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    process(context);
    
    // Advance counter if in tail-off
    if (tailOff)
    {
        tailOffCounter += numSamples;
        
        // Gradually reduce volume when tail-off is complete
        if (tailOffCounter >= tailOffDuration)
        {
            tailOff = false;
        }
    }
}

void SynthVoice::changeNote(int midiNoteNumber)
{
    currentlyPlayingNote = midiNoteNumber;
    setNote(midiNoteNumber, true); // isLegato = true
}

int SynthVoice::getCurrentlyPlayingNote() const
{
    return currentlyPlayingNote;
}

// Methods integrated from VCOProcessor
void SynthVoice::updateBlockRateParameters()
{
    if (toneGenerator)
        toneGenerator->updateBlockRateParameters();
}

void SynthVoice::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& outputBlock = context.getOutputBlock();
    auto numSamples = outputBlock.getNumSamples();
    auto numChannels = outputBlock.getNumChannels();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // In noise mode, the audio graph will use NoiseProcessor instead
        // So we only need to handle the tone generator here
        float currentSample = toneGenerator->getNextSample();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            outputBlock.setSample(channel, sample, currentSample);
        }
    }
}

void SynthVoice::reset()
{
    if (toneGenerator)
        toneGenerator->reset();
}

void SynthVoice::setNote(int midiNoteNumber, bool isLegato)
{
    if (toneGenerator)
        toneGenerator->setNote(midiNoteNumber, isLegato);
}
