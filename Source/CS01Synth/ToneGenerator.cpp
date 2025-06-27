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

void ToneGenerator::updateBlockRateParameters()
{
    currentFeet = static_cast<Feet>(static_cast<int>(*apvts.getRawParameterValue(ParameterIds::feet)));
    currentWaveform = static_cast<Waveform>(static_cast<int>(*apvts.getRawParameterValue(ParameterIds::waveType)));
    pwmLfo.setFrequency(apvts.getRawParameterValue(ParameterIds::pwmSpeed)->load());
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

    // 1半音あたりの時間を直接使用
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
