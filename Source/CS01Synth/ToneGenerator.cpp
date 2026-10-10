#include "CS01Synth/ToneGenerator.h"

#include "CS01Synth/WaveformStrategies.h"

#include <cmath>

ToneGenerator::ToneGenerator(cs01::ParameterState& parameters) : parameters(parameters) {}

void ToneGenerator::prepare(double rate) {
    sampleRate = internalSampleRate = static_cast<float>(rate);
    pwmLfo.prepare(rate);
    pwmLfo.initialise([](double x) { return std::asin(std::sin(x)) * (2.0 / std::numbers::pi); });
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
    noteOn = false;
    currentlyPlayingNote = 0;

    if (allowTailOff) {
        tailOff = true;
        // Get release time from parameter (convert to samples)
        float releaseSecs = parameters.get(ParameterIds::release);
        tailOffDuration = static_cast<int>(releaseSecs * sampleRate);
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
    auto upRange = parameters.get(ParameterIds::pitchBendUpRange);
    auto downRange = parameters.get(ParameterIds::pitchBendDownRange);
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
float ToneGenerator::renderSample() {
    if (!isActive())
        return 0;
    const float output = getNextSample();
    if (tailOff && ++tailOffCounter >= tailOffDuration)
        tailOff = false;
    return output;
}

// Existing methods from ToneGenerator
void ToneGenerator::updateBlockRateParameters() {
    if (appliedBendUpRange >= 0.0f &&
        (parameters.get(ParameterIds::pitchBendUpRange) != appliedBendUpRange ||
         parameters.get(ParameterIds::pitchBendDownRange) != appliedBendDownRange))
        pitchWheelMoved(lastPitchWheel);
    currentFeet = static_cast<Feet>(static_cast<int>(parameters.get(ParameterIds::feet)));
    currentWaveform =
        static_cast<Waveform>(static_cast<int>(parameters.get(ParameterIds::waveType)));

    // PWM LFO frequency setting with hardware-accurate range (0-60Hz)
    float pwmSpeed = parameters.get(ParameterIds::pwmSpeed);
    pwmLfo.setFrequency(pwmSpeed);

    currentModDepth = parameters.get(ParameterIds::modDepth);

    // Cache pitch-related parameters to avoid per-sample parameter access
    // MIDI (including queued panel gestures) applies bend once via pitchWheelMoved.
    pitchBendOffset = 0.0f;
    pitchOffset = parameters.get(ParameterIds::pitch);

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
    pwmLfo.reset();
    pwmLfo.setFrequency(parameters.get(ParameterIds::pwmSpeed), true);

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
    auto timePerSemitone = parameters.get(ParameterIds::glissando);

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
        const float duration = parameters.get(ParameterIds::glissando);
        if (duration < 0.001f) {
            currentPitch = targetPitch;
            isSliding = false;
        } else {
            const int updatedSamples = std::max(1, static_cast<int>(duration * sampleRate));
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

    return generateVcoSampleFromMaster(generateMasterSquareWave(finalPitch));
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
