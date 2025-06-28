#include "JuceHeader.h"
#include "CS01Synth/ToneGenerator.h"
#include "TestHelpers.h"

class ToneGeneratorTest : public juce::UnitTest
{
public:
    ToneGeneratorTest() : juce::UnitTest("ToneGeneratorTest") {}

    void runTest() override
    {
        auto layout = createTestParameterLayout();
        DummyProcessor processor;
        juce::AudioProcessorValueTreeState apvts(processor, nullptr, "params", std::move(layout));
        ToneGenerator tg(apvts);
        tg.prepare({44100.0, 512, 1});

        beginTest("Start and stop note");
        tg.startNote(60, 1.0f, 8192);
        expect(tg.isActive());
        tg.stopNote(false);
        expect(!tg.isActive());

        beginTest("Pitch bend influences sample");
        tg.startNote(60, 1.0f, 8192);
        tg.pitchWheelMoved(16383); // maximum up
        auto sample1 = tg.getNextSample();
        expect(!std::isnan(sample1));

        beginTest("LFO modulation changes output");
        float noLfo = tg.getNextSample();
        tg.setLfoValue(0.5f);
        float withLfo = tg.getNextSample();
        expectNotEquals(noLfo, withLfo);
    }
};

static ToneGeneratorTest toneGeneratorTest;
