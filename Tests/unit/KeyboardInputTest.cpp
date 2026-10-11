#include "UI/KeyboardInput.h"
#include "DSP/NoteOwnership.h"
#include "SynthEngine.h"
#include <gtest/gtest.h>
#include <vector>

TEST(KeyboardInputTest, MappingRepeatModifiersAndUnrecognizedKeys) {
    cs01::QwertyKeyboard keys;
    std::vector<std::pair<int, int>> events;
    auto send = [&](int note, int velocity) { events.emplace_back(note, velocity); };
    EXPECT_FALSE(keys.handle('A', false, true, send));
    EXPECT_TRUE(events.empty());
    EXPECT_TRUE(keys.handle('a', false, false, send));
    keys.handle('A', false, false, send);
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events.front(), (std::pair{48, 127}));
    keys.handle('A', true, true, send);
    EXPECT_EQ(events.back(), (std::pair{48, 0}));
    keys.handle('L', false, false, send);
    EXPECT_EQ(events.back(), (std::pair{62, 127}));
    EXPECT_FALSE(keys.handle(256 + 'A', false, false, send));
    EXPECT_FALSE(keys.handle('Q', false, false, send));
}
TEST(KeyboardInputTest, OctaveSwitchReleasesCapturedPitchesAndIgnoresRepeat) {
    cs01::QwertyKeyboard keys;
    std::vector<std::pair<int, int>> events;
    auto send = [&](int note, int velocity) { events.emplace_back(note, velocity); };
    keys.handle('A', false, false, send);
    keys.handle('X', false, false, send);
    EXPECT_EQ(events.back(), (std::pair{48, 0}));
    EXPECT_EQ(keys.octaveBase(), 60);
    keys.handle('X', false, false, send);
    EXPECT_EQ(keys.octaveBase(), 60);
    const auto released = events.size();
    keys.handle('A', true, false, send);
    EXPECT_EQ(events.size(), released);
    keys.handle('A', false, false, send);
    EXPECT_EQ(events.back(), (std::pair{60, 127}));
    keys.release(send);
    EXPECT_EQ(events.back(), (std::pair{60, 0}));
    keys.handle('A', true, false, send);
    EXPECT_EQ(events.back(), (std::pair{60, 0}));
}
TEST(KeyboardInputTest, InstancesAndOctaveLimitsAreIndependent) {
    cs01::QwertyKeyboard first, second;
    auto send = [](int, int) {};
    for (int i = 0; i < 20; ++i) {
        first.handle('X', false, false, send);
        first.handle('X', true, false, send);
    }
    EXPECT_EQ(first.octaveBase(), 96);
    EXPECT_EQ(second.octaveBase(), 48);
    for (int i = 0; i < 20; ++i) {
        first.handle('Z', false, false, send);
        first.handle('Z', true, false, send);
    }
    EXPECT_EQ(first.octaveBase(), 24);
}
TEST(KeyboardInputTest, MouseAndPcOwnershipSharePitchWithoutPrematureRelease) {
    cs01::UiNoteBridge bridge;
    std::vector<cs01::MidiMessage> events;
    auto send = [&](auto event) { events.push_back(event); };
    bridge.set(60, 80, cs01::UiNoteBridge::Mouse);
    bridge.consume(send);
    ASSERT_EQ(events.size(), 1);
    EXPECT_TRUE(events.back().isNoteOn());
    EXPECT_EQ(events.back().getVelocity(), 80);
    bridge.set(60, 127, cs01::UiNoteBridge::PC);
    bridge.set(60, 0, cs01::UiNoteBridge::Mouse);
    bridge.consume(send);
    EXPECT_EQ(events.size(), 1);
    bridge.set(60, 0, cs01::UiNoteBridge::PC);
    bridge.consume(send);
    ASSERT_EQ(events.size(), 2);
    EXPECT_TRUE(events.back().isNoteOff());
    bridge.consume(send);
    EXPECT_EQ(events.size(), 2);
}
TEST(KeyboardInputTest, HideCloseFocusCleanupCannotSilenceExternalSamePitch) {
    cs01::ParameterState parameters;
    SynthEngine engine(parameters);
    engine.prepare(48000);
    cs01::UiNoteBridge bridge;
    auto send = [&](auto event) { engine.handleUiMidi(event); };
    engine.handleMidi({0x90, 60, 100});
    bridge.set(60, 127, cs01::UiNoteBridge::PC);
    bridge.consume(send);
    bridge.releaseAll();
    bridge.consume(send);
    EXPECT_EQ(engine.currentNote(), 60);
    bridge.publishExternal(engine.externalNotes());
    EXPECT_TRUE(bridge.displayed(60));
    engine.handleMidi({0x80, 60, 0});
    EXPECT_EQ(engine.currentNote(), 0);
    bridge.publishExternal(engine.externalNotes());
    EXPECT_FALSE(bridge.displayed(60));
}
TEST(KeyboardInputTest, ExternalReleasePreservesUiAndUiCleanupReturnsToLowerHostNote) {
    cs01::ParameterState parameters;
    SynthEngine engine(parameters);
    engine.prepare(48000);
    engine.handleMidi({0x90, 48, 100});
    engine.handleUiMidi({0x90, 60, 127});
    engine.handleMidi({0x90, 60, 100});
    engine.handleMidi({0x80, 60, 0});
    EXPECT_EQ(engine.currentNote(), 60);
    engine.handleUiMidi({0x80, 60, 0});
    EXPECT_EQ(engine.currentNote(), 48);
    engine.handleUiMidi({0x80, 48, 0});
    EXPECT_EQ(engine.currentNote(), 48);
    engine.handleMidi({0x80, 48, 0});
    EXPECT_EQ(engine.currentNote(), 0);
}
TEST(KeyboardInputTest, DesiredStateCoalescesAndCannotOverflowLostNoteOffs) {
    cs01::UiNoteBridge bridge;
    unsigned events = 0;
    auto send = [&](auto) { ++events; };
    for (int i = 0; i < 10000; ++i) {
        bridge.set(i % 128, 127, cs01::UiNoteBridge::PC);
        bridge.set(i % 128, 0, cs01::UiNoteBridge::PC);
    }
    bridge.consume(send);
    EXPECT_EQ(events, 0u);
    for (int note = 0; note < 128; ++note)
        bridge.set(note, 127, cs01::UiNoteBridge::Mouse);
    bridge.consume(send);
    EXPECT_EQ(events, 128u);
    bridge.releaseAll();
    bridge.consume(send);
    EXPECT_EQ(events, 256u);
}
TEST(KeyboardInputTest, GlobalHostPanicClearsAllSourceOwnership) {
    cs01::NoteOwnership ownership;
    unsigned events = 0;
    auto send = [&](auto) { ++events; };
    ownership.process({0x90, 60, 100}, false, send);
    ownership.process({0x90, 60, 127}, true, send);
    ownership.process({0xb0, 120, 0}, false, send);
    EXPECT_TRUE(ownership.externalNotes().none());
    ownership.process({0x80, 60, 0}, false, send);
    ownership.process({0x80, 60, 0}, true, send);
    EXPECT_EQ(events, 3u);
}
