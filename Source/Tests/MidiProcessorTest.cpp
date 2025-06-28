#include "JuceHeader.h"
#include "CS01Synth/MidiProcessor.h"
#include "TestHelpers.h"

class MidiProcessorTest : public juce::UnitTest
{
public:
    MidiProcessorTest() : juce::UnitTest("MidiProcessorTest") {}

    void runTest() override
    {
        auto layout = createTestParameterLayout();
        DummyProcessor processor;
        juce::AudioProcessorValueTreeState apvts(processor, nullptr, "params", std::move(layout));
        MidiProcessor midi(apvts);
        DummyNoteHandler noteHandler;
        midi.setNoteHandler(&noteHandler);

        juce::MidiBuffer midiMessages;
        juce::AudioBuffer<float> buffer(1, 32);

        beginTest("Note on triggers handler");
        midiMessages.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
        midi.processBlock(buffer, midiMessages);
        expectEquals(noteHandler.startCount, 1);
        expectEquals(noteHandler.lastNote, 60);

        beginTest("Note off triggers stop");
        midiMessages.clear();
        midiMessages.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
        midi.processBlock(buffer, midiMessages);
        expect(noteHandler.stopCalled);

        beginTest("Pitch wheel updates parameter");
        midiMessages.clear();
        midiMessages.addEvent(juce::MidiMessage::pitchWheel(1, 16383), 0);
        midi.processBlock(buffer, midiMessages);
        auto bend = apvts.getRawParameterValue(ParameterIds::pitchBend)->load();
        expectGreaterThan(bend, 0.0f);

        beginTest("Legato note changes highest note");
        midiMessages.clear();
        noteHandler.startCount = 0; noteHandler.stopCalled = false; noteHandler.lastNote = 0;
        midiMessages.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
        midiMessages.addEvent(juce::MidiMessage::noteOn(1, 62, (juce::uint8)100), 0);
        midi.processBlock(buffer, midiMessages);
        expectEquals(noteHandler.startCount, 1); // only first note should start
        expectEquals(noteHandler.lastNote, 62);

        beginTest("Note off returns to previous note");
        midiMessages.clear();
        midiMessages.addEvent(juce::MidiMessage::noteOff(1, 62), 0);
        midi.processBlock(buffer, midiMessages);
        expect(!noteHandler.stopCalled); // still holding 60
        expectEquals(noteHandler.lastNote, 60);

        beginTest("Controller messages update parameters");
        midiMessages.clear();
        midiMessages.addEvent(juce::MidiMessage::controllerEvent(1, 1, 127), 0);
        midiMessages.addEvent(juce::MidiMessage::controllerEvent(1, 2, 64), 0);
        midi.processBlock(buffer, midiMessages);
        auto mod = apvts.getRawParameterValue(ParameterIds::modDepth)->load();
        auto breath = apvts.getRawParameterValue(ParameterIds::breathInput)->load();
        expectWithinAbsoluteError(mod, 1.0f, 0.0001f);
        expectWithinAbsoluteError(breath, 64.0f/127.0f, 0.0001f);
    }
};

static MidiProcessorTest midiProcessorTest;
