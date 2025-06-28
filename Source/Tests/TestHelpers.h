#include "JuceHeader.h"
#include "CS01Synth/INoteHandler.h"
#include "Parameters.h"

inline juce::AudioProcessorValueTreeState::ParameterLayout createTestParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto vcoGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "vco", "VCO", "|",
        std::make_unique<juce::AudioParameterChoice>(ParameterIds::waveType, "Wave Type", juce::StringArray{"Triangle", "Sawtooth", "Square", "Pulse", "PWM"}, 1),
        std::make_unique<juce::AudioParameterChoice>(ParameterIds::feet, "Feet", juce::StringArray{"32'", "16'", "8'", "4'", "WN"}, 2),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::pwmSpeed, "PWM Speed", juce::NormalisableRange<float>(0.6f, 12.0f, 0.01f, 0.5f), 2.0f),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::pitch, "Pitch", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::glissando, "Glissando", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.5f), 0.0f)
    );
    layout.add(std::move(vcoGroup));

    auto vcfGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "vcf", "VCF", "|",
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::cutoff, "Cutoff", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 20000.0f),
        std::make_unique<juce::AudioParameterBool>(ParameterIds::resonance, "Resonance", false),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::vcfEgDepth, "VCF EG Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f)
    );
    layout.add(std::move(vcfGroup));

    auto vcaGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "vca", "VCA", "|",
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::vcaEgDepth, "VCA EG Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f)
    );
    layout.add(std::move(vcaGroup));

    auto egGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "eg", "EG", "|",
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::attack, "Attack", juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.3f), 0.1f),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::decay, "Decay", juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.3f), 0.1f),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::sustain, "Sustain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::release, "Release", juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.3f), 0.1f)
    );
    layout.add(std::move(egGroup));

    auto lfoGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "lfo", "LFO", "|",
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::lfoSpeed, "LFO Speed", juce::NormalisableRange<float>(0.0f, 21.0f, 0.01f, 0.3f), 5.0f),
        std::make_unique<juce::AudioParameterChoice>(ParameterIds::lfoTarget, "LFO Target", juce::StringArray{"VCO", "VCF"}, 0),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::modDepth, "Mod Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f)
    );
    layout.add(std::move(lfoGroup));

    auto modGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "mod", "Modulation", "|",
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::pitchBend, "Pitch Bend", juce::NormalisableRange<float>(0.0f, 12.0f), 0.0f),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::breathVcf, "Breath VCF", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::breathVca, "Breath VCA", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f),
        std::make_unique<juce::AudioParameterInt>(ParameterIds::pitchBendUpRange, "Pitch Bend Up", 0, 12, 12),
        std::make_unique<juce::AudioParameterInt>(ParameterIds::pitchBendDownRange, "Pitch Bend Down", 0, 12, 12)
    );
    layout.add(std::move(modGroup));

    auto globalGroup = std::make_unique<juce::AudioProcessorParameterGroup>(
        "global", "Global", "|",
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::volume, "Volume", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f),
        std::make_unique<juce::AudioParameterFloat>(ParameterIds::breathInput, "Breath Input", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f),
        std::make_unique<juce::AudioParameterChoice>(ParameterIds::filterType, "Filter Type", juce::StringArray{"CS01", "MODERN"}, 0)
    );
    layout.add(std::move(globalGroup));

    return layout;
}

class DummyNoteHandler : public INoteHandler
{
public:
    int lastNote = 0;
    int startCount = 0;
    bool stopCalled = false;

    void startNote(int midiNoteNumber, float, int) override { lastNote = midiNoteNumber; ++startCount; }
    void stopNote(bool) override { stopCalled = true; }
    void changeNote(int midiNoteNumber) override { lastNote = midiNoteNumber; }
    void pitchWheelMoved(int) override {}
    bool isActive() const override { return true; }
    int getCurrentlyPlayingNote() const override { return lastNote; }
};

class DummyProcessor : public juce::AudioProcessor
{
public:
    DummyProcessor() : juce::AudioProcessor(BusesProperties{}) {}

    const juce::String getName() const override { return "Dummy"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};
