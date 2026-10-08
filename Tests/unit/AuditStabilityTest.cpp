#include <JuceHeader.h>

#include "CS01AudioProcessor.h"
#include "CS01AudioProcessorEditor.h"
#include "CS01Synth/CS01VCFCircuit.h"
#include "CS01Synth/EGProcessor.h"
#include "CS01Synth/NoiseGenerator.h"
#include "CS01Synth/VCOProcessor.h"
#include "CS01Synth/VCAProcessor.h"
#include "UI/ProgramPanel.h"

#include <gtest/gtest.h>

#include <limits>
#include <thread>

namespace {
void setControl(CS01AudioProcessor& processor, const juce::String& id, float value) {
    auto* parameter = processor.apvts.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
VCOProcessor* source(CS01AudioProcessor& p) {
    return dynamic_cast<VCOProcessor*>(
        p.getAudioGraphForTesting().getNodeForId(p.getVcoNodeIdForTesting())->getProcessor());
}
void render(CS01AudioProcessor& p, int samples, const juce::MidiMessage* event = nullptr,
            int offset = 0) {
    juce::AudioBuffer<float> buffer(2, samples);
    buffer.clear();
    juce::MidiBuffer midi;
    if (event)
        midi.addEvent(*event, offset);
    p.processBlock(buffer, midi);
    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < samples; ++i)
            ASSERT_TRUE(std::isfinite(buffer.getSample(channel, i)));
}
void pump() {
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
}
juce::TextButton* button(CS01AudioProcessorEditor& editor, const juce::String& name) {
    for (auto* child : editor.getChildren())
        if (auto* panel = dynamic_cast<ProgramPanel*>(child))
            for (auto* control : panel->getChildren())
                if (auto* b = dynamic_cast<juce::TextButton*>(control))
                    if (b->getButtonText() == name)
                        return b;
    return nullptr;
}
}  // namespace

TEST(AuditMidiTest, BoundaryEventsAreAppliedIncludingEmptyBlocks) {
    for (int size : {0, 64}) {
        for (int position : {-1, size > 0 ? size - 1 : 0, size, size + 1}) {
            SCOPED_TRACE(size);
            SCOPED_TRACE(position);
            CS01AudioProcessor p;
            p.prepareToPlay(48000, 64);
            auto on = juce::MidiMessage::noteOn(1, 60, 1.0f);
            render(p, size, &on, position);
            EXPECT_TRUE(source(p)->getSoundGenerator()->isActive());
            auto panic = juce::MidiMessage::controllerEvent(1, 120, 0);
            render(p, size, &panic, position);
            EXPECT_FALSE(source(p)->getSoundGenerator()->isActive());
        }
    }
}

TEST(AuditMidiTest, SamePositionOrderSurvivesBoundaryAndUnsupportedLongMessage) {
    for (int size : {0, 64}) {
        for (bool panicLast : {false, true}) {
            CS01AudioProcessor p;
            p.prepareToPlay(48000, 64);
            juce::AudioBuffer<float> audio(2, size);
            audio.clear();
            juce::MidiBuffer midi;
            auto on = juce::MidiMessage::noteOn(1, 60, 1.0f);
            auto off = juce::MidiMessage::noteOff(1, 60);
            auto panic = juce::MidiMessage::controllerEvent(1, 120, 0);
            std::array<juce::uint8, 4096> bytes{};
            auto sysex = juce::MidiMessage::createSysExMessage(bytes.data(), bytes.size());
            midi.addEvent(panicLast ? on : panic, size);
            midi.addEvent(sysex, size);
            midi.addEvent(off, size);
            midi.addEvent(panicLast ? panic : on, size);
            p.processBlock(audio, midi);
            EXPECT_EQ(source(p)->getSoundGenerator()->isActive(), !panicLast);
        }
    }
}

TEST(AuditReleaseTest, WholeGraphExtensionShorteningAndSwitchUseEnvelopeLifetime) {
    for (int feet : {2, 4}) {
        for (float depth : {0.0f, 1.0f}) {
            for (int partition : {1, 7, 64}) {
                CS01AudioProcessor p;
                setControl(p, ParameterIds::feet, static_cast<float>(feet));
                setControl(p, ParameterIds::attack, 0.001f);
                setControl(p, ParameterIds::decay, 0.001f);
                setControl(p, ParameterIds::sustain, 1.0f);
                setControl(p, ParameterIds::vcaEgDepth, depth);
                setControl(p, ParameterIds::release, 0.05f);
                p.prepareToPlay(48000, 64);
                EGProcessor* envelope = nullptr;
                for (auto* node : p.getAudioGraphForTesting().getNodes())
                    if (auto* eg = dynamic_cast<EGProcessor*>(node->getProcessor()))
                        envelope = eg;
                ASSERT_NE(envelope, nullptr);
                auto run = [&](int count) {
                    while (count > 0) {
                        int length = std::min(partition, count);
                        render(p, length);
                        EXPECT_EQ(source(p)->getSoundGenerator()->isActive(), envelope->isActive());
                        count -= length;
                    }
                };
                auto on = juce::MidiMessage::noteOn(1, 60, 1.0f);
                render(p, 64, &on);
                run(480);
                auto off = juce::MidiMessage::noteOff(1, 60);
                render(p, 0, &off);
                run(960);
                setControl(p, ParameterIds::release, 1.0f);
                run(2400);
                EXPECT_TRUE(source(p)->getSoundGenerator()->isActive());
                setControl(p, ParameterIds::feet, feet == 2 ? 4.0f : 2.0f);
                run(64);
                EXPECT_TRUE(source(p)->getSoundGenerator()->isActive());
                for (float release : {0.2f, 0.7f, 0.01f}) {
                    setControl(p, ParameterIds::release, release);
                    run(64);
                    EXPECT_TRUE(source(p)->getSoundGenerator()->isActive());
                }
                run(480);
                EXPECT_FALSE(source(p)->getSoundGenerator()->isActive());
            }
        }
    }
}

TEST(AuditNoiseTest, StopsInsideBlockAtRemainingSampleCount) {
    for (int partition : {1, 7, 64, 256}) {
        CS01AudioProcessor p;
        setControl(p, ParameterIds::release, 0.001f);
        NoiseGenerator noise(p.apvts);
        noise.prepare({48000, 256, 1});
        noise.startNote(60, 1, 8192);
        noise.stopNote(true);
        juce::AudioBuffer<float> audio(1, 256);
        audio.clear();
        for (int i = 0; i < 256; i += partition)
            noise.renderNextBlock(audio, i, std::min(partition, 256 - i));
        EXPECT_FALSE(noise.isActive());
        // ceil(float(.001) * 48000) can be 49; all later samples must stay clear.
        for (int i = 49; i < 256; ++i)
            EXPECT_FLOAT_EQ(audio.getSample(0, i), 0.0f);
    }
}

TEST(AuditNumericalTest, OriginalCouplingRecoversAfterNonfiniteInput) {
    for (float bad :
         {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        for (bool block : {false, true}) {
            CS01VCFCircuit circuit(48000), reference(48000);
            circuit.prepare(48000);
            reference.prepare(48000);
            float sample = bad, cutoff = 1000;
            if (block)
                circuit.processBlock(&sample, 1, &cutoff, 0.1f);
            else
                sample = circuit.processSample(0, bad);
            EXPECT_FLOAT_EQ(sample, 0);
            double energy = 0;
            for (int i = 0; i < 1024; ++i) {
                float input = std::sin(i * 0.13f);
                float output = circuit.processSample(0, input);
                ASSERT_TRUE(std::isfinite(output));
                EXPECT_FLOAT_EQ(output, reference.processSample(0, input));
                energy += output * output;
            }
            EXPECT_GT(energy, 0.01);
        }
    }
}

TEST(AuditNumericalTest, VcaRecoversInternalCouplingAfterBadAudioOrEnvelope) {
    for (int channel : {0, 1}) {
        CS01AudioProcessor p;
        VCAProcessor vca(p.apvts), reference(p.apvts);
        vca.prepareToPlay(48000, 64);
        reference.prepareToPlay(48000, 64);
        juce::AudioBuffer<float> audio(2, 64), expected(2, 64);
        juce::MidiBuffer midi;
        audio.clear();
        audio.setSample(channel, 63, std::numeric_limits<float>::quiet_NaN());
        vca.processBlock(audio, midi);
        for (int i = 0; i < 64; ++i) {
            audio.setSample(0, i, std::sin(i * .13f));
            audio.setSample(1, i, 1.0f);
        }
        expected.makeCopyOf(audio);
        vca.processBlock(audio, midi);
        reference.processBlock(expected, midi);
        for (int i = 0; i < 64; ++i) {
            ASSERT_TRUE(std::isfinite(audio.getSample(0, i)));
            EXPECT_FLOAT_EQ(audio.getSample(0, i), expected.getSample(0, i));
        }
    }
}

TEST(AuditUiTest, WorkerChoiceNotificationsWaitForMessageThreadAndLatestValueWins) {
    CS01AudioProcessor p;
    auto editor = std::make_unique<CS01AudioProcessorEditor>(p);
    VCFComponent* filter = nullptr;
    juce::ToggleButton* modern = nullptr;
    for (auto* child : editor->getChildren())
        if (auto* component = dynamic_cast<VCFComponent*>(child))
            filter = component;
    ASSERT_NE(filter, nullptr);
    for (auto* child : filter->getChildren())
        if (auto* b = dynamic_cast<juce::ToggleButton*>(child))
            if (b->getButtonText() == "II")
                modern = b;
    ASSERT_NE(modern, nullptr);
    std::thread worker([&] {
        for (int i = 0; i < 100; ++i) {
            setControl(p, ParameterIds::filterType, i % 2);
            setControl(p, ParameterIds::lfoTarget, i % 2);
            setControl(p, ParameterIds::waveType, i % 5);
            setControl(p, ParameterIds::feet, i % 5);
        }
    });
    worker.join();
    EXPECT_FALSE(modern->getToggleState());
    pump();
    EXPECT_TRUE(modern->getToggleState());
    setControl(p, ParameterIds::filterType, 0);
    editor.reset();
    pump();
}

TEST(AuditPresetTest, PortableNameValidation) {
    for (const auto* name : {"", "../escape", "a/b", "a\\b", ".", "..", " a", "a ", "a.", "CON",
                             "nul.txt", "COM1", "LPT9", "a:b", "a?b", "a\nb"})
        EXPECT_FALSE(ProgramManager::isValidPresetName(name)) << name;
    for (const auto* name : {"My Preset", "音色", "lead.v2", "COM10"})
        EXPECT_TRUE(ProgramManager::isValidPresetName(name)) << name;
}

TEST(AuditPresetTest, FailedLoadsAndWritesPreserveSelectionAndExistingFile) {
    CS01AudioProcessor p;
    auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("cheapsynth-audit-" + juce::Uuid().toString());
    ASSERT_TRUE(directory.createDirectory());
    auto& manager = p.getPresetManager();
    manager.setUserPresetsDirectoryForTesting(directory);
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("good"));
    auto file = directory.getChildFile("good.xml");
    int index = manager.findProgram("good.xml", PresetType::User);
    ASSERT_GE(index, 0);
    manager.setCurrentProgram(1);
    const float cutoff = p.apvts.getRawParameterValue(ParameterIds::cutoff)->load();
    ASSERT_TRUE(file.replaceWithText("broken XML"));
    manager.setCurrentProgram(index);
    EXPECT_EQ(manager.getCurrentProgram(), 1);
    EXPECT_FLOAT_EQ(p.apvts.getRawParameterValue(ParameterIds::cutoff)->load(), cutoff);
    ASSERT_TRUE(file.deleteFile());
    manager.setCurrentProgram(index);
    EXPECT_EQ(manager.getCurrentProgram(), 1);
    ASSERT_TRUE(file.createDirectory());
    ASSERT_TRUE(file.getChildFile("keep").replaceWithText("preserve"));
    EXPECT_FALSE(manager.saveCurrentStateAsPreset("good"));
    EXPECT_TRUE(file.getChildFile("keep").existsAsFile());
    EXPECT_EQ(manager.getCurrentProgram(), 1);
    EXPECT_FALSE(manager.saveCurrentStateAsPreset("../escape"));
    directory.deleteRecursively();
}

TEST(AuditDialogTest, ClosingEditorAtEachSaveStageCancelsAccess) {
    for (int stage : {0, 1, 2, 3}) {
        CS01AudioProcessor p;
        auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("cheapsynth-dialog-" + juce::Uuid().toString());
        ASSERT_TRUE(directory.createDirectory());
        auto& manager = p.getPresetManager();
        manager.setUserPresetsDirectoryForTesting(directory);
        if (stage >= 2) {
            ASSERT_TRUE(manager.saveCurrentStateAsPreset("Existing"));
        }
        auto editor = std::make_unique<CS01AudioProcessorEditor>(p);
        auto* save = button(*editor, "Save");
        ASSERT_NE(save, nullptr);
        save->onClick();
        auto* dialog =
            dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        ASSERT_NE(dialog, nullptr);
        juce::Component::SafePointer<juce::AlertWindow> safe(dialog);
        dialog->getTextEditor("presetName")->setText(stage >= 2 ? "Existing" : "New");
        if (stage >= 1)
            dialog->exitModalState(1);
        if (stage >= 2) {
            pump();
            dialog =
                dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
            ASSERT_NE(dialog, nullptr);
            safe = dialog;
            if (stage == 3)
                dialog->exitModalState(1);
        }
        editor.reset();
        EXPECT_EQ(safe.getComponent(), nullptr);
        pump();
        EXPECT_FALSE(directory.getChildFile("New.xml").exists());
        directory.deleteRecursively();
    }
}

TEST(AuditQueueTest, OverflowPanicsAndSubsequentInputCanRecover) {
    CS01AudioProcessor p;
    p.prepareToPlay(48000, 64);
    auto on = juce::MidiMessage::noteOn(1, 60, 1.0f);
    render(p, 64, &on);
    for (size_t i = 0; i < RealtimeMidiQueue::capacity + 1; ++i)
        p.getMidiMessageCollector().addMessageToQueue(on);
    render(p, 64);
    EXPECT_FALSE(source(p)->getSoundGenerator()->isActive());
    p.getMidiMessageCollector().addMessageToQueue(on);
    render(p, 64);
    EXPECT_TRUE(source(p)->getSoundGenerator()->isActive());
    p.getMidiMessageCollector().addMessageToQueue(juce::MidiMessage::noteOff(1, 60));
    render(p, 64);
    render(p, 9600);
    EXPECT_FALSE(source(p)->getSoundGenerator()->isActive());
}

TEST(AuditProgramTest, HostThreadRequestsApplyAtNextBlockAndMatchUiLoads) {
    for (int index = 0; index < 7; ++index) {
        CS01AudioProcessor p, expected;
        p.prepareToPlay(48000, 64);
        for (auto* parameter : p.getParameters())
            parameter->setValueNotifyingHost(0.23f);
        for (auto* parameter : expected.getParameters())
            parameter->setValueNotifyingHost(0.23f);
        expected.setCurrentProgram(index);
        std::thread host([&] { p.setCurrentProgram(index); });
        host.join();
        render(p, 0);
        EXPECT_EQ(p.getCurrentProgram(), index);
        for (int i = 0; i < p.getParameters().size(); ++i)
            EXPECT_NEAR(p.getParameters()[i]->getValue(), expected.getParameters()[i]->getValue(),
                        1e-6f);
        juce::MemoryBlock saved;
        p.getStateInformation(saved);
        auto xml = juce::AudioProcessor::getXmlFromBinary(saved.getData(), saved.getSize());
        ASSERT_NE(xml, nullptr);
        EXPECT_EQ(xml->getIntAttribute("program"), index);
    }
}

TEST(AuditProgramTest, CatalogueRefreshAndHostRequestsCanRunConcurrently) {
    CS01AudioProcessor p;
    p.prepareToPlay(48000, 64);
    std::atomic<bool> done{false};
    std::thread host([&] {
        juce::AudioBuffer<float> audio(2, 16);
        juce::MidiBuffer midi;
        for (int i = 0; i < 500; ++i) {
            p.setCurrentProgram(i % 7);
            p.processBlock(audio, midi);
            EXPECT_EQ(p.getNumPrograms(), 7);
            EXPECT_FALSE(p.getProgramName(i % 7).isEmpty());
        }
        done = true;
    });
    while (!done.load()) {
        p.getPresetManager().refreshUserPresets();
        pump();
    }
    host.join();
    EXPECT_GE(p.getCurrentProgram(), 0);
    EXPECT_LT(p.getCurrentProgram(), 7);
}

#if defined(CHEAPSYNTH_RT_AUDIT)
#include "RealtimeAudit.h"
TEST(AuditRealtimeTest, EntireCallbackHasNoHeapOperationsForDenseMidiAndGuiQueue) {
    CS01AudioProcessor p;
    p.prepareToPlay(48000, 64);
    juce::AudioBuffer<float> audio(2, 64);
    juce::MidiBuffer midi;
    midi.ensureSize(65536);
    render(p, 64);
    size_t allocations = 0, frees = 0, locks = 0;
    std::thread audioThread([&] {
        for (int block = 0; block < 100; ++block) {
            midi.clear();
            for (int position = 0; position < 64; ++position) {
                midi.addEvent(juce::MidiMessage::noteOn(1, 60 + position % 12, 1.0f), position);
                midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, position), position);
                midi.addEvent(juce::MidiMessage::noteOff(1, 60 + position % 12), position);
            }
            p.getMidiMessageCollector().addMessageToQueue(juce::MidiMessage::noteOff(1, 60));
            realtimeAudit::begin();
            p.setCurrentProgram(block % 7);
            p.processBlock(audio, midi);
            realtimeAudit::end();
            allocations += realtimeAudit::allocations;
            frees += realtimeAudit::deallocations;
            locks += realtimeAudit::locks;
        }
    });
    audioThread.join();
    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(frees, 0u);
    std::cout << "RT callback probe: allocation=" << allocations << " free=" << frees
              << " pthread_mutex_lock=" << locks
              << " (including JUCE graph nodes and MessageManager thread checks)\n";
}
#endif

TEST(AuditNumericalTest, VcaEnvelopeDepthSmootherRecoversAfterNonfiniteControl) {
    CS01AudioProcessor p;
    VCAProcessor vca(p.apvts);
    vca.prepareToPlay(48000, 64);
    juce::AudioBuffer<float> audio(2, 64);
    juce::MidiBuffer midi;
    auto fill = [&] {
        for (int i = 0; i < 64; ++i) {
            audio.setSample(0, i, std::sin(i * 0.1f));
            audio.setSample(1, i, 1.0f);
        }
    };
    p.apvts.getParameter(ParameterIds::vcaEgDepth)
        ->setValue(std::numeric_limits<float>::quiet_NaN());
    fill();
    vca.processBlock(audio, midi);
    p.apvts.getParameter(ParameterIds::vcaEgDepth)->setValue(1.0f);
    double energy = 0.0;
    for (int block = 0; block < 20; ++block) {
        fill();
        vca.processBlock(audio, midi);
        for (int i = 0; i < 64; ++i) {
            ASSERT_TRUE(std::isfinite(audio.getSample(0, i)));
            energy += std::pow(audio.getSample(0, i), 2.0);
        }
    }
    EXPECT_GT(energy, 0.01);
}

#if defined(CHEAPSYNTH_RT_AUDIT)
TEST(AuditRealtimeTest, FirstAndVariableLengthCallbacksReusePreparedGraphStorage) {
    CS01AudioProcessor p;
    p.prepareToPlay(48000, 64);
    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(p.createEditor());
    size_t allocations = 0, frees = 0;
    std::thread audioThread([&] {
        for (int size : {0, 1, 7, 16, 64, 127, 64, 7, 1}) {
            juce::AudioBuffer<float> audio(2, size);
            juce::MidiBuffer midi;
            midi.ensureSize(32768);
            std::array<juce::uint8, 4096> bytes{};
            midi.addEvent(juce::MidiMessage::createSysExMessage(bytes.data(), bytes.size()),
                          size / 2);
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
            midi.addEvent(juce::MidiMessage::noteOff(1, 60), size > 0 ? size - 1 : 0);
            realtimeAudit::begin();
            p.processBlock(audio, midi);
            realtimeAudit::end();
            allocations += realtimeAudit::allocations;
            frees += realtimeAudit::deallocations;
        }
    });
    audioThread.join();
    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(frees, 0u);
}
#endif

TEST(AuditQueueTest, PreparationResetCanOverlapProducersAndConsumer) {
    RealtimeMidiQueue queue;
    std::atomic<int> completed{0};
    const auto produce = [&] {
        for (int i = 0; i < 10000; ++i)
            queue.addMessageToQueue(i % 2 == 0 ? juce::MidiMessage::noteOn(1, 60, 1.0f)
                                               : juce::MidiMessage::noteOff(1, 60));
        ++completed;
    };
    std::thread first(produce), second(produce);
    std::thread prepare([&] {
        for (int i = 0; i < 1000; ++i)
            queue.reset(i % 2 == 0 ? 48000.0 : 44100.0);
        ++completed;
    });
    juce::MidiBuffer midi;
    midi.ensureSize(32768);
    while (completed.load() != 3) {
        midi.clear();
        queue.removeNextBlockOfMessages(midi, 64);
        for (const auto event : midi)
            EXPECT_GE(event.samplePosition, 0);
    }
    first.join();
    second.join();
    prepare.join();
    queue.reset(48000);
    for (int i = 0; i < 3; ++i) {
        midi.clear();
        queue.removeNextBlockOfMessages(midi, 64);
    }
    queue.addMessageToQueue(juce::MidiMessage::noteOff(1, 60));
    midi.clear();
    EXPECT_FALSE(queue.removeNextBlockOfMessages(midi, 64));
    ASSERT_EQ(midi.getNumEvents(), 1);
    EXPECT_TRUE((*midi.begin()).getMessage().isNoteOff());
}

TEST(AuditProgramTest, UserHostRequestsUsePreparedValuesAndRejectInvalidRefreshedFiles) {
    CS01AudioProcessor processor;
    processor.prepareToPlay(48000, 64);
    auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("cheapsynth-host-user-" + juce::Uuid().toString());
    ASSERT_TRUE(directory.createDirectory());
    auto& manager = processor.getPresetManager();
    manager.setUserPresetsDirectoryForTesting(directory);
    setControl(processor, ParameterIds::cutoff, 0.8f);
    setControl(processor, ParameterIds::feet, 4);
    const auto savedCutoff = processor.apvts.getParameter(ParameterIds::cutoff)->getValue();
    ASSERT_TRUE(manager.saveCurrentStateAsPreset("Prepared"));
    const int index = manager.findProgram("Prepared.xml", PresetType::User);
    ASSERT_GE(index, 0);
    manager.setCurrentProgram(0);
    render(processor, 64);
    EXPECT_FALSE(source(processor)->isNoiseMode());
    juce::MemoryBlock defaultSession;
    processor.getStateInformation(defaultSession);
    // External edits do not introduce file I/O on a host/audio program request.
    ASSERT_TRUE(directory.getChildFile("Prepared.xml").replaceWithText("broken XML"));
    std::thread host([&] { processor.setCurrentProgram(index); });
    host.join();
    EXPECT_EQ(processor.getCurrentProgram(), 0);
    render(processor, 64);
    EXPECT_EQ(processor.getCurrentProgram(), index);
    EXPECT_FLOAT_EQ(processor.apvts.getParameter(ParameterIds::cutoff)->getValue(), savedCutoff);
    EXPECT_TRUE(source(processor)->isNoiseMode());
    processor.setStateInformation(defaultSession.getData(),
                                  static_cast<int>(defaultSession.getSize()));
    render(processor, 64);
    EXPECT_FALSE(source(processor)->isNoiseMode());
    std::thread again([&] { processor.setCurrentProgram(index); });
    again.join();
    render(processor, 64);
    EXPECT_TRUE(source(processor)->isNoiseMode());
    manager.refreshUserPresets();
    manager.setCurrentProgram(0);
    std::thread invalid([&] { processor.setCurrentProgram(index); });
    invalid.join();
    render(processor, 64);
    EXPECT_EQ(processor.getCurrentProgram(), 0);
    EXPECT_FALSE(source(processor)->isNoiseMode());
    directory.deleteRecursively();
}
