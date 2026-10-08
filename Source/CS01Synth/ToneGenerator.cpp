#include "CS01Synth/ToneGenerator.h"

#include "MidiParameterValue.h"

#include "CS01Synth/WaveformStrategies.h"

#include <cmath>

ToneGenerator::ToneGenerator(juce::AudioProcessorValueTreeState& apvts) : apvts(apvts) {}

void ToneGenerator::prepare(const juce::dsp::ProcessSpec& spec) {
    sampleRate = spec.sampleRate;
    internalSampleRate = static_cast<float>(
        spec.sampleRate * (externalOversampling ? 1 : Constants::oversamplingFactor));
    auto internalSpec = spec;
    internalSpec.sampleRate = internalSampleRate;
    internalSpec.maximumBlockSize *= externalOversampling ? 1 : Constants::oversamplingFactor;
    pwmLfo.prepare(internalSpec);
    oversampling.initProcessing(1);
    pwmLfo.initialise(
        [](double x) { return std::asin(std::sin(x)) * (2.0 / juce::MathConstants<double>::pi); });

    reset();
}

// INoteHandler interface implementation
void ToneGenerator::startNote(int midiNoteNumber, float velocity, int currentPitchWheelPosition) {
    lastNote = midiNoteNumber;
    tailOff = false;
    currentlyPlayingNote = midiNoteNumber;
    setNote(midiNoteNumber, false);  // isLegato = false
    pitchWheelMoved(currentPitchWheelPosition);
    noteOn = true;
}

void ToneGenerator::stopNote(bool allowTailOff) {
    graphOwnsRelease = false;
    noteOn = false;
    currentlyPlayingNote = 0;

    if (allowTailOff) {
        tailOff = true;
        // Get release time from parameter (convert to samples)
        releaseSeconds = getMidiParameterValue(apvts, ParameterIds::release);
        tailOffDuration =
            static_cast<int>(std::ceil(static_cast<double>(releaseSeconds) * sampleRate));
        tailOffCounter = 0;
    } else {
        tailOff = false;
    }
}

void ToneGenerator::changeNote(int midiNoteNumber) {
    currentlyPlayingNote = midiNoteNumber;
    setNote(midiNoteNumber, true);  // isLegato = true
}

void ToneGenerator::pitchWheelMoved(int newPitchWheelValue) {
    lastPitchWheel = newPitchWheelValue;
    auto upRange = getCurrentParameterValue(apvts, ParameterIds::pitchBendUpRange);
    auto downRange = getCurrentParameterValue(apvts, ParameterIds::pitchBendDownRange);
    appliedBendUpRange = upRange;
    appliedBendDownRange = downRange;

    const float displacement = static_cast<float>(newPitchWheelValue - 8192);
    const float bendValue = displacement / (displacement >= 0.0f ? 8191.0f : 8192.0f);

    float bendOffset = 0.0f;
    if (bendValue > 0)
        bendOffset = bendValue * upRange;
    else
        bendOffset = bendValue * downRange;

    pitchBend = bendOffset;
}

bool ToneGenerator::isActive() const {
    return noteOn || (tailOff && tailOffCounter < tailOffDuration);
}

ISoundGenerator::PlaybackState ToneGenerator::getPlaybackState() const {
    return {noteOn, lastNote, lastPitchWheel,
            tailOff
                ? std::max(0, tailOffDuration - tailOffCounter) / static_cast<double>(sampleRate)
                : 0.0};
}

void ToneGenerator::restorePlaybackState(const PlaybackState& state) {
    stopNote(false);
    if (!state.held && state.releaseSecondsRemaining <= 0.0)
        return;
    startNote(state.note, 1.0f, state.pitchWheel);
    if (!state.held) {
        noteOn = false;
        tailOff = true;
        releaseSeconds = getMidiParameterValue(apvts, ParameterIds::release);
        tailOffCounter = 0;
        tailOffDuration =
            static_cast<int>(std::llround(state.releaseSecondsRemaining * sampleRate));
        currentlyPlayingNote = 0;
    }
}

int ToneGenerator::getCurrentlyPlayingNote() const {
    return currentlyPlayingNote;
}

// Audio processing methods
void ToneGenerator::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample,
                                    int numSamples) {
    updateReleaseDuration();
    if (!isActive())
        return;

    if (tailOff)
        numSamples = juce::jmin(numSamples, juce::jmax(0, tailOffDuration - tailOffCounter));
    updateBlockRateParameters();

    const int numChannels = outputBuffer.getNumChannels();

    // Fill channel 0 (mono) directly to avoid per-sample per-channel inner loop.
    float* ch0 = outputBuffer.getWritePointer(0, startSample);
    for (int i = 0; i < numSamples; ++i) {
        float currentSample = getNextSample();
        ch0[i] += currentSample;  // preserve additive behavior
    }

    // Duplicate channel 0 into other channels efficiently
    for (int channel = 1; channel < numChannels; ++channel) {
        outputBuffer.addFrom(channel, startSample, outputBuffer, 0, startSample, numSamples);
    }

    // Advance counter if in tail-off
    if (tailOff) {
        tailOffCounter += numSamples;

        if (tailOffCounter >= tailOffDuration) {
            tailOff = false;
        }
    }
}

void ToneGenerator::process(const juce::dsp::ProcessContextReplacing<float>& context) {
    auto& outputBlock = context.getOutputBlock();
    const auto numSamples = static_cast<int>(outputBlock.getNumSamples());
    const auto numChannels = static_cast<int>(outputBlock.getNumChannels());

    updateBlockRateParameters();

    // Fill channel 0 (mono) first
    for (int sample = 0; sample < numSamples; ++sample) {
        float currentSample = getNextSample();
        outputBlock.setSample(0, sample, currentSample);
    }

    // For additional channels, copy channel 0 contents to avoid regenerating per channel
    for (int ch = 1; ch < numChannels; ++ch) {
        for (int sample = 0; sample < numSamples; ++sample) {
            outputBlock.setSample(ch, sample, outputBlock.getSample(0, sample));
        }
    }
}

// Existing methods from ToneGenerator
void ToneGenerator::updateBlockRateParameters() {
    if (appliedBendUpRange >= 0.0f &&
        (getCurrentParameterValue(apvts, ParameterIds::pitchBendUpRange) != appliedBendUpRange ||
         getCurrentParameterValue(apvts, ParameterIds::pitchBendDownRange) != appliedBendDownRange))
        pitchWheelMoved(lastPitchWheel);
    currentFeet =
        static_cast<Feet>(static_cast<int>(getCurrentParameterValue(apvts, ParameterIds::feet)));
    currentWaveform = static_cast<Waveform>(
        static_cast<int>(getCurrentParameterValue(apvts, ParameterIds::waveType)));

    // PWM LFO frequency setting with hardware-accurate range (0-60Hz)
    float pwmSpeed = getCurrentParameterValue(apvts, ParameterIds::pwmSpeed);
    pwmLfo.setFrequency(pwmSpeed);

    currentModDepth = getMidiParameterValue(apvts, ParameterIds::modDepth);

    // Cache pitch-related parameters to avoid per-sample parameter access
    // MIDI (including queued panel gestures) applies bend once via pitchWheelMoved.
    pitchBendOffset = 0.0f;
    pitchOffset = getCurrentParameterValue(apvts, ParameterIds::pitch);

    // Update waveform strategy based on current waveform
    waveformModel.selectWaveform(currentWaveform);
}

void ToneGenerator::reset() {
    currentPitch = 60.0f;
    targetPitch = 60.0f;
    isSliding = false;
    samplesPerStep = 0;
    stepCounter = 0;
    phase = 0.0f;
    leakyIntegratorState = 0.0f;
    dcBlockerState = 0.0f;

    // Reset base square wave state
    waveformModel.reset();
    oversampling.reset();
    oversamplingBuffer.clear();
    pwmLfo.reset();
    pwmLfo.setFrequency(getCurrentParameterValue(apvts, ParameterIds::pwmSpeed), true);

    // Reset note state
    noteOn = false;
    tailOff = false;
    tailOffCounter = 0;
    tailOffDuration = 0;
    currentlyPlayingNote = 0;
}

void ToneGenerator::setNote(int midiNoteNumber, bool isLegato) {
    lastNote = midiNoteNumber;
    if (isLegato) {
        if (std::abs(midiNoteNumber - currentPitch) > 0.1f) {
            calculateSlideParameters(midiNoteNumber);
        }
    } else {
        isSliding = false;
        currentPitch = static_cast<float>(midiNoteNumber);
        targetPitch = currentPitch;
    }
}

void ToneGenerator::calculateSlideParameters(int targetNote) {
    targetPitch = static_cast<float>(targetNote);
    auto timePerSemitone = getMidiParameterValue(apvts, ParameterIds::glissando);

    if (timePerSemitone < 0.001f)  // No slide
    {
        isSliding = false;
        currentPitch = targetPitch;
        return;
    }

    float pitchDifference = std::abs(targetPitch - currentPitch);
    if (pitchDifference == 0) {
        isSliding = false;
        return;
    }

    // Use time per semitone directly
    samplesPerStep = static_cast<int>(timePerSemitone * sampleRate);
    if (samplesPerStep < 1)
        samplesPerStep = 1;

    stepCounter = 0;
    isSliding = true;
}

float ToneGenerator::getNextSample() {
    // Handle glissando (discrete semitone steps - remains unchanged)
    if (isSliding) {
        // Interim live-control model: preserve fractional progress through the
        // current semitone. YM10150's oscillator phase behavior is not established.
        const float duration = getMidiParameterValue(apvts, ParameterIds::glissando);
        if (duration < 0.001f) {
            currentPitch = targetPitch;
            isSliding = false;
        } else {
            const int updatedSamples = juce::jmax(1, static_cast<int>(duration * sampleRate));
            if (updatedSamples != samplesPerStep) {
                stepCounter = static_cast<int>(static_cast<double>(stepCounter) * updatedSamples /
                                               samplesPerStep);
                samplesPerStep = updatedSamples;
            }
        }
    }
    if (isSliding) {
        stepCounter++;
        if (stepCounter >= samplesPerStep) {
            stepCounter = 0;
            if (targetPitch > currentPitch)
                currentPitch += 1.0f;  // Half-tone steps for glissando
            else
                currentPitch -= 1.0f;  // Half-tone steps for glissando

            if (std::abs(targetPitch - currentPitch) < 0.1f) {
                currentPitch = targetPitch;
                isSliding = false;
            }
        }
    }

    // Calculate final pitch with continuous modulations
    float finalPitch = currentPitch;  // Base pitch (discrete for glissando)

    // Add continuous pitch modulations (smooth, continuous changes)
    finalPitch += pitchBend;        // Pitch bend wheel (continuous)
    finalPitch += pitchBendOffset;  // Pitch bend offset (continuous)
    finalPitch += pitchOffset;      // Fine pitch adjustment (continuous)
    finalPitch += lfoValue;         // LFO pitch modulation (continuous)

    // Add octave offset (discrete, but doesn't affect continuity)
    int octaveOffset = 0;
    switch (currentFeet) {
        case Feet::Feet32:
            octaveOffset = -24;
            break;
        case Feet::Feet16:
            octaveOffset = -12;
            break;
        case Feet::Feet8:
            octaveOffset = 0;
            break;
        case Feet::Feet4:
            octaveOffset = 12;
            break;
        default:
            break;
    }
    finalPitch += octaveOffset;

    // Generate directly at the shared internal rate; only the final signal is
    // downsampled. Glide and external modulation still advance at the host rate.
    if (externalOversampling)
        return generateVcoSampleFromMaster(generateMasterSquareWave(finalPitch));
    juce::dsp::AudioBlock<float> block(oversamplingBuffer);
    auto internalBlock = oversampling.processSamplesUp(block);
    auto* data = internalBlock.getChannelPointer(0);
    for (size_t i = 0; i < internalBlock.getNumSamples(); ++i) {
        const float masterSquare = generateMasterSquareWave(finalPitch);
        data[i] = generateVcoSampleFromMaster(masterSquare);
    }
    oversampling.processSamplesDown(block);
    return oversamplingBuffer.getSample(0, 0);
}

void ToneGenerator::setLfoValue(float newLfoValue) {
    lfoValue = newLfoValue;
}

void ToneGenerator::setPitchBend(float bendInSemitones) {
    pitchBend = bendInSemitones;
}

float ToneGenerator::generateMasterSquareWave(float finalPitch) {
    return waveformModel.generateMasterSquareWave(finalPitch, internalSampleRate, phase,
                                                  phaseIncrement);
}

float ToneGenerator::generateVcoSampleFromMaster(float masterSquare) {
    return waveformModel.generateWaveform(masterSquare, phase, phaseIncrement, internalSampleRate,
                                          pwmLfo);
}

void ToneGenerator::setReleaseSamplesRemaining(int samples) {
    graphOwnsRelease = true;
    if (!tailOff)
        return;
    releaseSeconds = getMidiParameterValue(apvts, ParameterIds::release);
    tailOffCounter = 0;
    tailOffDuration = juce::jmax(0, samples);
    if (samples <= 0) {
        tailOff = false;
        noteOn = false;
    }
}

void ToneGenerator::updateReleaseDuration() {
    if (graphOwnsRelease)
        return;
    if (tailOff) {
        const float requested = getMidiParameterValue(apvts, ParameterIds::release);
        if (requested != releaseSeconds) {
            releaseSeconds = requested;
            tailOffCounter = 0;
            tailOffDuration =
                static_cast<int>(std::ceil(static_cast<double>(requested) * sampleRate));
        }
    }
}
