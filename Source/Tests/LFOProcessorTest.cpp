#include "JuceHeader.h"
#include "CS01Synth/LFOProcessor.h"
#include "TestHelpers.h"

class LFOProcessorTest : public juce::UnitTest
{
public:
    LFOProcessorTest() : juce::UnitTest("LFOProcessorTest") {}

    void runTest() override
    {
        auto layout = createTestParameterLayout();
        DummyProcessor processor;
        juce::AudioProcessorValueTreeState apvts(processor, nullptr, "params", std::move(layout));
        LFOProcessor lfo(apvts);
        lfo.prepareToPlay(44100.0, 64);

        juce::AudioBuffer<float> buffer(1, 64);
        juce::MidiBuffer midi;
        lfo.processBlock(buffer, midi);

        beginTest("Buffer not silent after processing");
        bool allZero = true;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (buffer.getSample(0, i) != 0.0f) { allZero = false; break; }
        expect(!allZero);

        beginTest("Zero speed produces constant output");
        *apvts.getRawParameterValue(ParameterIds::lfoSpeed) = 0.0f;
        lfo.prepareToPlay(44100.0, 8);
        buffer.setSize(1, 8);
        buffer.clear();
        lfo.processBlock(buffer, midi);
        float first = buffer.getSample(0, 0);
        bool constant = true;
        for (int i = 1; i < buffer.getNumSamples(); ++i)
            if (buffer.getSample(0, i) != first) { constant = false; break; }
        expect(constant);
    }
};

static LFOProcessorTest lfoProcessorTest;
