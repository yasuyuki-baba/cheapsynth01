#include "ToneGenerator.h"
#include "WaveformStrategies.h"

ToneGenerator::ToneGenerator(juce::AudioProcessorValueTreeState& apvts) : apvts(apvts) 
{
    initializeWaveformStrategies();
}

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
    
    float bendOffset = 0.0f;
    if (bendValue > 0)
        bendOffset = bendValue * upRange;
    else
        bendOffset = bendValue * downRange;

    pitchBend = bendOffset;
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
    
    for (size_t sample = 0; sample < static_cast<size_t>(numSamples); ++sample)
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

    updateBlockRateParameters();

    for (size_t sample = 0; sample < static_cast<size_t>(numSamples); ++sample)
    {
        float currentSample = getNextSample();

        for (size_t channel = 0; channel < static_cast<size_t>(numChannels); ++channel)
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
    
    // Cache pitch-related parameters to avoid per-sample parameter access
    pitchBendOffset = apvts.getRawParameterValue(ParameterIds::pitchBend)->load();
    pitchOffset = apvts.getRawParameterValue(ParameterIds::pitch)->load();
    
    // Update waveform strategy based on current waveform
    updateWaveformStrategy();
}

void ToneGenerator::reset()
{
    currentPitch = 60.0f;
    targetPitch = 60.0f;
    isSliding = false;
    samplesPerStep = 0;
    stepCounter = 0;
    phase = 0.0f;
    leakyIntegratorState = 0.0f;
    dcBlockerState = 0.0f;
    
    // Reset base square wave state
    previousBaseSquare = 0.0f;
    
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
    // Handle glissando (discrete semitone steps - remains unchanged)
    if (isSliding)
    {
        stepCounter++;
        if (stepCounter >= samplesPerStep)
        {
            stepCounter = 0;
            if (targetPitch > currentPitch)
                currentPitch += 1.0f;  // Half-tone steps for glissando
            else
                currentPitch -= 1.0f;  // Half-tone steps for glissando

            if (std::abs(targetPitch - currentPitch) < 0.1f)
            {
                currentPitch = targetPitch;
                isSliding = false;
            }
        }
    }

    // Calculate final pitch with continuous modulations
    float finalPitch = currentPitch;  // Base pitch (discrete for glissando)
    
    // Add continuous pitch modulations (smooth, continuous changes)
    finalPitch += pitchBend;                 // Pitch bend wheel (continuous)
    finalPitch += pitchBendOffset;           // Pitch bend offset (continuous)
    finalPitch += pitchOffset;               // Fine pitch adjustment (continuous)
    finalPitch += lfoValue;                  // LFO pitch modulation (continuous)

    // Add octave offset (discrete, but doesn't affect continuity)
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

    // Generate master square wave with continuous pitch calculation
    float masterSquare = generateMasterSquareWave(finalPitch);
    
    // Convert to desired waveform
    return generateVcoSampleFromMaster(masterSquare);
}


void ToneGenerator::initializeWaveformStrategies()
{
    // Initialize waveform strategy mapping directly
    waveformStrategies[Waveform::Triangle] = std::make_unique<TriangleWaveformStrategy>();
    waveformStrategies[Waveform::Sawtooth] = std::make_unique<SawtoothWaveformStrategy>();
    waveformStrategies[Waveform::Square] = std::make_unique<SquareWaveformStrategy>();
    waveformStrategies[Waveform::Pulse] = std::make_unique<PulseWaveformStrategy>();
    waveformStrategies[Waveform::Pwm] = std::make_unique<PWMWaveformStrategy>();
    
    // Set default strategy
    currentWaveformStrategy = waveformStrategies[Waveform::Sawtooth].get();
}

void ToneGenerator::updateWaveformStrategy()
{
    // Reset strategy-specific states when waveform changes
    if (previousWaveform != currentWaveform)
    {
        currentWaveformStrategy->reset();
    }
    
    // Direct access to strategy (all waveforms are guaranteed to be initialized)
    currentWaveformStrategy = waveformStrategies[currentWaveform].get();
    
    // Update for next comparison
    previousWaveform = currentWaveform;
}

void ToneGenerator::setLfoValue(float newLfoValue)
{
    lfoValue = newLfoValue;
}

void ToneGenerator::setPitchBend(float bendInSemitones)
{
    pitchBend = bendInSemitones;
}

float ToneGenerator::generateMasterSquareWave(float finalPitch)
{
    // Calculate frequency directly from finalPitch using continuous calculation
    // This ensures smooth pitch bend and pitch slider operation
    float frequency = 440.0f * std::pow(2.0f, (finalPitch - 69.0f) / 12.0f);
    phaseIncrement = frequency / sampleRate;  // Update global phaseIncrement!
    
    // Generate master clock square wave (50% duty cycle)
    float t = phase;
    float baseSquare = (t < 0.5f) ? 1.0f : -1.0f;
    
    // Apply poly_blep anti-aliasing
    baseSquare += poly_blep(t, phaseIncrement);
    baseSquare -= poly_blep(fmod(t + 0.5f, 1.0f), phaseIncrement);
    
    // Update phase for next sample
    phase += phaseIncrement;
    if (phase >= 1.0f) phase -= 1.0f;
    
    // Emulate analog circuit characteristics
    return std::tanh(baseSquare * 1.2f);
}


float ToneGenerator::generateVcoSampleFromMaster(float masterSquare)
{
    if (currentWaveformStrategy == nullptr)
        return masterSquare;
    
    // Use Strategy pattern to generate waveform
    float value = currentWaveformStrategy->generate(
        masterSquare,
        phase,
        phaseIncrement,
        sampleRate,
        previousBaseSquare,
        pwmLfo
    );
    
    // Standard analog circuit output stage for all waveforms
    return std::tanh(value * 1.2f);
}
