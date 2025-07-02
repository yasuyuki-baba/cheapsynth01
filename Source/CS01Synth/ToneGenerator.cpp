#include "ToneGenerator.h"

namespace
{
    float poly_blep(float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }
}

ToneGenerator::ToneGenerator(juce::AudioProcessorValueTreeState& apvts) : apvts(apvts) {}

void ToneGenerator::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    pwmLfo.prepare(spec);
    pwmLfo.initialise([](float x) { return std::asin(std::sin(x)) * (2.0f / juce::MathConstants<float>::pi); }, 128);
    reset();
}

// INoteHandler interface implementation
void ToneGenerator::startNote(int midiNoteNumber, float velocity, int currentPitchWheelPosition)
{
    currentlyPlayingNote = midiNoteNumber;
    setNote(midiNoteNumber, false); // isLegato = false
    pitchWheelMoved(currentPitchWheelPosition);
    noteOn = true;
}

void ToneGenerator::stopNote(bool allowTailOff)
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

void ToneGenerator::changeNote(int midiNoteNumber)
{
    currentlyPlayingNote = midiNoteNumber;
    setNote(midiNoteNumber, true); // isLegato = true
}

void ToneGenerator::pitchWheelMoved(int newPitchWheelValue)
{
    auto upRange = apvts.getRawParameterValue(ParameterIds::pitchBendUpRange)->load();
    auto downRange = apvts.getRawParameterValue(ParameterIds::pitchBendDownRange)->load();

    auto bendValue = juce::jmap(static_cast<float>(newPitchWheelValue), 0.0f, 16383.0f, -1.0f, 1.0f);
    
    float pitchOffset = 0.0f;
    if (bendValue > 0)
        pitchOffset = bendValue * upRange;
    else
        pitchOffset = bendValue * downRange;

    midiPitchBendValue = pitchOffset;
}

bool ToneGenerator::isActive() const
{
    return noteOn || (tailOff && tailOffCounter < tailOffDuration);
}

int ToneGenerator::getCurrentlyPlayingNote() const
{
    return currentlyPlayingNote;
}

// Audio processing methods
void ToneGenerator::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (!isActive())
        return;

    updateBlockRateParameters();
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float currentSample = getNextSample();
        
        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
        {
            outputBuffer.addSample(channel, startSample + sample, currentSample);
        }
    }
    
    // Advance counter if in tail-off
    if (tailOff)
    {
        tailOffCounter += numSamples;
        
        if (tailOffCounter >= tailOffDuration)
        {
            tailOff = false;
        }
    }
}

void ToneGenerator::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& outputBlock = context.getOutputBlock();
    auto numSamples = outputBlock.getNumSamples();
    auto numChannels = outputBlock.getNumChannels();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float currentSample = getNextSample();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            outputBlock.setSample(channel, sample, currentSample);
        }
    }
}

// Existing methods from ToneGenerator
void ToneGenerator::updateBlockRateParameters()
{
    currentFeet = static_cast<Feet>(static_cast<int>(*apvts.getRawParameterValue(ParameterIds::feet)));
    currentWaveform = static_cast<Waveform>(static_cast<int>(*apvts.getRawParameterValue(ParameterIds::waveType)));
    
    // PWM LFO frequency setting with hardware-accurate range (0-60Hz)
    float pwmSpeed = apvts.getRawParameterValue(ParameterIds::pwmSpeed)->load();
    pwmLfo.setFrequency(pwmSpeed);
    
    currentModDepth = apvts.getRawParameterValue(ParameterIds::modDepth)->load();
}

void ToneGenerator::reset()
{
    currentPitch = 60.0f;
    targetPitch = 60.0f;
    isSliding = false;
    slideSamplesRemaining = 0;
    slideIncrement = 0.0f;
    samplesPerStep = 0;
    stepCounter = 0;
    phase = 0.0f;
    leakyIntegratorState = 0.0f;
    dcBlockerState = 0.0f;
    
    // Reset note state
    noteOn = false;
    tailOff = false;
    tailOffCounter = 0;
    tailOffDuration = 0;
    currentlyPlayingNote = 0;
}

void ToneGenerator::setNote(int midiNoteNumber, bool isLegato)
{
    if (isLegato)
    {
        if (std::abs(midiNoteNumber - currentPitch) > 0.1f)
        {
            calculateSlideParameters(midiNoteNumber);
        }
    }
    else
    {
        isSliding = false;
        currentPitch = static_cast<float>(midiNoteNumber);
        targetPitch = currentPitch;
    }
}

void ToneGenerator::calculateSlideParameters(int targetNote)
{
    targetPitch = static_cast<float>(targetNote);
    auto timePerSemitone = apvts.getRawParameterValue(ParameterIds::glissando)->load();
    
    if (timePerSemitone < 0.001f) // No slide
    {
        isSliding = false;
        currentPitch = targetPitch;
        return;
    }

    float pitchDifference = std::abs(targetPitch - currentPitch);
    if (pitchDifference == 0)
    {
        isSliding = false;
        return;
    }

    // Use time per semitone directly
    samplesPerStep = static_cast<int>(timePerSemitone * sampleRate);
    if (samplesPerStep < 1) samplesPerStep = 1;

    stepCounter = 0;
    isSliding = true;
}

float ToneGenerator::getNextSample()
{
    if (isSliding)
    {
        stepCounter++;
        if (stepCounter >= samplesPerStep)
        {
            stepCounter = 0;
            if (targetPitch > currentPitch)
                currentPitch += 1.0f;
            else
                currentPitch -= 1.0f;

            if (std::abs(targetPitch - currentPitch) < 0.1f)
            {
                currentPitch = targetPitch;
                isSliding = false;
            }
        }
    }

    float finalPitch = currentPitch;
    
    // Add pitch bend from MIDI and GUI
    finalPitch += midiPitchBendValue;
    finalPitch += apvts.getRawParameterValue(ParameterIds::pitchBend)->load();

    // Add continuous pitch offsets
    finalPitch += apvts.getRawParameterValue(ParameterIds::pitch)->load();
    
    finalPitch += lfoValue;

    // Add octave offset
    int octaveOffset = 0;
    switch (currentFeet)
    {
        case Feet::Feet32: octaveOffset = -24; break;
        case Feet::Feet16: octaveOffset = -12; break;
        case Feet::Feet8:  octaveOffset = 0;   break;
        case Feet::Feet4:  octaveOffset = 12;  break;
        default: break;
    }
    finalPitch += octaveOffset;

    float frequency = juce::MidiMessage::getMidiNoteInHertz(finalPitch);
    setVcoFrequency(frequency);
    
    return generateVcoSample();
}

float ToneGenerator::generateVcoSample()
{
    // Pulse width setting (adjusted to match CS-01 characteristics)
    float pulseWidth = 0.5f;
    if (currentWaveform == Waveform::Pulse) pulseWidth = 0.25f;
    else if (currentWaveform == Waveform::Pwm) pulseWidth = 0.5f * (1.0f + pwmLfo.processSample(0.0f));

    float t = phase;
    
    // Square wave generation (with characteristics similar to CS-01 master clock oscillator)
    float pulseWave = (t < pulseWidth) ? 1.0f : -1.0f;
    
    // Poly_blep processing to reduce aliasing
    pulseWave += poly_blep(t, phaseIncrement);
    pulseWave -= poly_blep(fmod(t + (1.0f - pulseWidth), 1.0f), phaseIncrement);
    
    // Emulate nonlinearity of analog circuit
    pulseWave = std::tanh(pulseWave * 1.2f);

    float value = 0.0f;
    switch (currentWaveform)
    {
        case Waveform::Sawtooth:
            // Approximate CS-01 sawtooth wave characteristics
            value = 1.0f - (phase * 2.0f); // Downward sawtooth wave (from +1 to -1)
            value += poly_blep(phase, phaseIncrement);
            
            // Emphasize higher harmonics (CS-01 characteristic)
            value = value * 0.7f + std::sin(value * juce::MathConstants<float>::pi) * 0.3f;
            break;
            
        case Waveform::Triangle:
        {
            // CS-01 triangle wave generation (integration of square wave)
            leakyIntegratorState += pulseWave * phaseIncrement * 4.0f;
            float output = leakyIntegratorState - dcBlockerState;
            dcBlockerState = leakyIntegratorState + (dcBlockerState - leakyIntegratorState) * 0.995f;
            
            // Adjust triangle wave smoothness (CS-01 characteristic)
            value = output * 0.8f + std::sin(output * juce::MathConstants<float>::pi * 0.5f) * 0.2f;
            break;
        }
        
        case Waveform::Square:
            // CS-01 square wave characteristics (slightly rounded rise/fall)
            value = pulseWave;
            break;
            
        case Waveform::Pulse:
        case Waveform::Pwm:
            // CS-01 pulse wave characteristics
            value = pulseWave;
            break;
            
        default: 
            break;
    }

    // Phase update
    phase += phaseIncrement;
    if (phase >= 1.0f) phase -= 1.0f;
    
    // Emulate analog circuit output stage (slight distortion and volume increase)
    return std::tanh(value * 1.5f);
}

void ToneGenerator::setLfoValue(float newLfoValue)
{
    lfoValue = newLfoValue;
}

void ToneGenerator::setVcoFrequency(float frequency)
{
    phaseIncrement = frequency / sampleRate;
}

void ToneGenerator::setPitchBend(float bendInSemitones)
{
    midiPitchBendValue = bendInSemitones;
}
