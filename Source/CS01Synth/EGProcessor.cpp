#include "CS01Synth/EGProcessor.h"

#include <cmath>

//==============================================================================
EGProcessor::EGProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::mono(), true)),
      apvts(apvts) {}

EGProcessor::~EGProcessor() {}

//==============================================================================
void EGProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    stopEnvelopeImmediately();
    envelopeSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    updateADSR();
}

void EGProcessor::releaseResources() {
    stopEnvelopeImmediately();
}

bool EGProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    return true;
}

void EGProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    updateADSR();

    // CS01 is a mono synth, so only generate mono output
    buffer.clear();

    // Skip MIDI message processing (already processed in MidiProcessor)
    // MIDI messages are processed by startEnvelope/releaseEnvelope methods

    // Process mono output (channel 0) only
    auto* channelData = buffer.getWritePointer(0);

    // Provisional exponential shaping; endpoints and stage durations are retained.
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        channelData[sample] = nextEnvelopeSample();
    }
    if (buffer.getNumSamples() > 0)
        lastOutput = channelData[buffer.getNumSamples() - 1];
}

// Each stage approaches an overshoot target exponentially and crosses its
// endpoint at the configured duration. k=2 is provisional, not calibrated.
void EGProcessor::beginStage(Stage next, double endpoint, double seconds) {
    stage = next;
    stageEndpoint = endpoint;
    const double distance = endpoint - level;
    stageTarget = level + distance / (-std::expm1(-2.0));
    stageCoefficient = -std::expm1(-2.0 / (std::max(seconds, 1.0e-6) * envelopeSampleRate));
    remainingSamples = std::max<int64_t>(1, static_cast<int64_t>(
        std::ceil(std::max(seconds, 0.0) * envelopeSampleRate)));
}

float EGProcessor::nextEnvelopeSample() {
    if (stage == Stage::idle)
        return 0.0f;
    if (stage != Stage::sustain) {
        level += (stageTarget - level) * stageCoefficient;
        if (--remainingSamples <= 0) {
            level = stageEndpoint;
            if (stage == Stage::attack)
                beginStage(Stage::decay, settings.sustain, settings.decay);
            else if (stage == Stage::release)
                stage = Stage::idle;
            else
                stage = Stage::sustain;
        }
    }
    return static_cast<float>(juce::jlimit(0.0, 1.0, level));
}

void EGProcessor::startEnvelope() {
    updateADSR();
    beginStage(Stage::attack, 1.0, settings.attack);
}

void EGProcessor::releaseEnvelope() {
    updateADSR();
    if (stage != Stage::idle)
        beginStage(Stage::release, 0.0, settings.release);
}

void EGProcessor::stopEnvelopeImmediately() {
    stage = Stage::idle;
    level = 0.0;
    lastOutput = 0.0f;
    remainingSamples = 0;
}

void EGProcessor::updateADSR() {
    juce::ADSR::Parameters next;
    next.attack = apvts.getRawParameterValue(ParameterIds::attack)->load();
    next.decay = apvts.getRawParameterValue(ParameterIds::decay)->load();
    next.sustain = apvts.getRawParameterValue(ParameterIds::sustain)->load();
    next.release = apvts.getRawParameterValue(ParameterIds::release)->load();
    const auto previous = settings;
    settings = next;
    // Edits restart only the affected stage from its current level. Unrelated
    // edits never overwrite a running release; sustain changes slew via decay.
    if (stage == Stage::attack && next.attack != previous.attack)
        beginStage(Stage::attack, 1.0, next.attack);
    else if (stage == Stage::release && next.release != previous.release)
        beginStage(Stage::release, 0.0, next.release);
    else if ((stage == Stage::decay || stage == Stage::sustain) &&
             (next.sustain != previous.sustain ||
              (stage == Stage::decay && next.decay != previous.decay)))
        beginStage(Stage::decay, next.sustain, next.decay);
}
