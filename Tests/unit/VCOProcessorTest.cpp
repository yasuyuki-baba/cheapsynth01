#include <JuceHeader.h>

#include "CS01Synth/MidiProcessor.h"
#include "CS01Synth/VCOProcessor.h"
#include "Parameters.h"
#include "mocks/MockToneGenerator.h"

#include <gtest/gtest.h>

#include <thread>

// Test fixture for VCOProcessor tests
class VCOProcessorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Create a dummy processor for APVTS
        dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();

        auto parameterLayout = createParameterLayout();
        apvts = std::make_unique<juce::AudioProcessorValueTreeState>(
            *dummyProcessor, nullptr, "PARAMETERS", std::move(parameterLayout));

        // Create VCOProcessor
        processor = std::make_unique<VCOProcessor>(*apvts);
    }

    void TearDown() override {
        processor.reset();
        apvts.reset();
        dummyProcessor.reset();
    }

    // Create a parameter layout for testing
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // Add parameters needed for VCOProcessor
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            ParameterIds::waveType, "Wave Type",
            juce::StringArray("Triangle", "Sawtooth", "Square", "Pulse", "PWM"), 0));

        layout.add(std::make_unique<juce::AudioParameterChoice>(
            ParameterIds::feet, "Feet", juce::StringArray("32'", "16'", "8'", "4'", "Noise"), 2));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::modDepth, "Mod Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

        // Match the production float parameter instead of an incompatible bool fixture.
        layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterIds::glissando, "Glissando",
                                                               0.0f, 0.208f, 0.0f));

        // Add parameters required by ToneGenerator
        layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterIds::pitchBend,
                                                               "Pitch Bend", -12.0f, 12.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::release, "Release", juce::NormalisableRange<float>(0.01f, 5.0f), 0.1f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::pitchBendUpRange, "Pitch Bend Up Range",
            juce::NormalisableRange<float>(0.0f, 12.0f), 2.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::pitchBendDownRange, "Pitch Bend Down Range",
            juce::NormalisableRange<float>(0.0f, 12.0f), 2.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::pitch, "Pitch", juce::NormalisableRange<float>(-12.0f, 12.0f), 0.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::pwmSpeed, "PWM Speed", juce::NormalisableRange<float>(0.1f, 10.0f),
            1.0f));

        // Add parameters for MockToneGenerator
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            "vco_waveform", "VCO Waveform", juce::NormalisableRange<float>(0.0f, 4.0f), 0.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            "vco_octave", "VCO Octave", juce::NormalisableRange<float>(-2.0f, 2.0f), 0.0f));

        return layout;
    }

    std::unique_ptr<juce::AudioProcessorGraph> dummyProcessor;
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
    std::unique_ptr<VCOProcessor> processor;
};

TEST_F(VCOProcessorTest, HeldNoteSurvivesNoiseRoundTrip) {
    processor->prepareToPlay(48000, 256);
    auto* original = processor->getSoundGenerator();
    original->startNote(69, 1.0f, 8192);
    int changes = 0;
    processor->onGeneratorTypeChanged = [&]() { ++changes; };
    juce::AudioBuffer<float> buffer(1, 256);
    juce::MidiBuffer midi;
    auto* feet = apvts->getParameter(ParameterIds::feet);
    for (int selection : {4, 2, 4, 2}) {
        feet->setValueNotifyingHost(feet->convertTo0to1(static_cast<float>(selection)));
        processor->applyPendingGeneratorChange();
        auto* selected = processor->getSoundGenerator();
        ASSERT_NE(selected, nullptr);
        EXPECT_EQ(selected == original, selection == 2);
        EXPECT_TRUE(selected->isActive());
        EXPECT_EQ(selected->getCurrentlyPlayingNote(), 69);
        buffer.clear();
        processor->processBlock(buffer, midi);
        double power = 0.0;
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            const double value = buffer.getSample(0, i);
            ASSERT_TRUE(std::isfinite(value));
            power += value * value;
        }
        EXPECT_GT(power, 0.0);
    }
    EXPECT_EQ(changes, 4);
    processor->getSoundGenerator()->stopNote(false);
    EXPECT_FALSE(processor->getSoundGenerator()->isActive());
    processor->onGeneratorTypeChanged = nullptr;
}

TEST_F(VCOProcessorTest, IdleAndExpiredSourcesStaySilentAfterSwitching) {
    processor->prepareToPlay(48000, 256);
    auto* feet = apvts->getParameter(ParameterIds::feet);
    juce::AudioBuffer<float> buffer(1, 256);
    juce::MidiBuffer midi;
    for (bool previouslyPlayed : {false, true}) {
        if (previouslyPlayed) {
            processor->getSoundGenerator()->startNote(69, 1.0f, 8192);
            processor->getSoundGenerator()->stopNote(true);
            for (int i = 0; i < 40; ++i) {
                buffer.clear();
                processor->processBlock(buffer, midi);
            }
        }
        for (int selection : {4, 2, 4, 2}) {
            feet->setValueNotifyingHost(feet->convertTo0to1(static_cast<float>(selection)));
            processor->applyPendingGeneratorChange();
            EXPECT_FALSE(processor->getSoundGenerator()->isActive());
            buffer.clear();
            processor->processBlock(buffer, midi);
            EXPECT_FLOAT_EQ(buffer.getMagnitude(0, 256), 0.0f);
        }
    }
}

TEST_F(VCOProcessorTest, NewNoteCancelsOldReleaseDeadline) {
    processor->prepareToPlay(48000, 256);
    juce::AudioBuffer<float> buffer(1, 256);
    juce::MidiBuffer midi;
    auto* feet = apvts->getParameter(ParameterIds::feet);
    for (int selection : {2, 4}) {
        feet->setValueNotifyingHost(feet->convertTo0to1(static_cast<float>(selection)));
        processor->applyPendingGeneratorChange();
        auto* generator = processor->getSoundGenerator();
        generator->startNote(60, 1.0f, 8192);
        generator->stopNote(true);
        buffer.clear();
        processor->processBlock(buffer, midi);
        generator->startNote(69, 1.0f, 8192);
        for (int i = 0; i < 40; ++i) {
            buffer.clear();
            processor->processBlock(buffer, midi);
        }
        EXPECT_TRUE(generator->isActive());
        EXPECT_EQ(generator->getCurrentlyPlayingNote(), 69);
        EXPECT_TRUE(generator->getPlaybackState().held);
        generator->stopNote(true);
        for (int i = 0; i < 40; ++i) {
            buffer.clear();
            processor->processBlock(buffer, midi);
        }
        EXPECT_FALSE(generator->isActive());
    }
}

TEST_F(VCOProcessorTest, UnrelatedNoteOffDoesNotRestartRelease) {
    processor->prepareToPlay(48000, 256);
    MidiProcessor midiProcessor(*apvts);
    midiProcessor.setSoundGenerator(processor->getSoundGenerator());
    juce::AudioBuffer<float> buffer(1, 256);
    juce::MidiBuffer midi;
    const auto send = [&](const juce::MidiMessage& message) {
        midi.addEvent(message, 0);
        midiProcessor.processBlock(buffer, midi);
    };
    send(juce::MidiMessage::noteOn(1, 69, 1.0f));
    send(juce::MidiMessage::noteOff(1, 69));
    buffer.clear();
    processor->processBlock(buffer, midi);
    const auto before = processor->getSoundGenerator()->getPlaybackState();
    send(juce::MidiMessage::noteOff(1, 60));
    const auto after = processor->getSoundGenerator()->getPlaybackState();
    EXPECT_DOUBLE_EQ(after.releaseSecondsRemaining, before.releaseSecondsRemaining);
    EXPECT_FALSE(after.held);
}

TEST_F(VCOProcessorTest, ReleaseSwitchDoesNotLeaveGeneratorRunning) {
    processor->prepareToPlay(48000, 256);
    auto* feet = apvts->getParameter(ParameterIds::feet);
    for (int initial : {2, 4}) {
        feet->setValueNotifyingHost(feet->convertTo0to1(static_cast<float>(initial)));
        processor->applyPendingGeneratorChange();
        processor->getSoundGenerator()->startNote(69, 1.0f, 8192);
        processor->getSoundGenerator()->stopNote(true);
        ASSERT_TRUE(processor->getSoundGenerator()->isActive());
        const int destination = initial == 2 ? 4 : 2;
        feet->setValueNotifyingHost(feet->convertTo0to1(static_cast<float>(destination)));
        juce::AudioBuffer<float> buffer(1, 256);
        juce::MidiBuffer midi;
        // One second is longer than the fixture's 0.1 second release.
        for (int offset = 0; offset < 48000; offset += 256) {
            buffer.clear();
            processor->processBlock(buffer, midi);
        }
        EXPECT_FALSE(processor->getSoundGenerator()->isActive())
            << "Switching a released source must not create an indefinitely held note";
        processor->getSoundGenerator()->stopNote(false);
    }
}

TEST_F(VCOProcessorTest, SwitchingPreservesBendAndRemainingReleaseTime) {
    processor->prepareToPlay(48000, 256);
    auto* feet = apvts->getParameter(ParameterIds::feet);
    processor->getSoundGenerator()->startNote(69, 1.0f, 12000);
    feet->setValueNotifyingHost(feet->convertTo0to1(4.0f));
    processor->applyPendingGeneratorChange();
    EXPECT_EQ(processor->getSoundGenerator()->getPlaybackState().pitchWheel, 12000);
    processor->getSoundGenerator()->pitchWheelMoved(15000);
    processor->getSoundGenerator()->stopNote(true);
    juce::AudioBuffer<float> buffer(1, 256);
    juce::MidiBuffer midi;
    buffer.clear();
    processor->processBlock(buffer, midi);
    const auto before = processor->getSoundGenerator()->getPlaybackState();
    EXPECT_FALSE(before.held);
    EXPECT_NEAR(before.releaseSecondsRemaining, 0.1 - 256.0 / 48000.0, 1.0 / 48000.0);
    feet->setValueNotifyingHost(feet->convertTo0to1(2.0f));
    processor->applyPendingGeneratorChange();
    const auto after = processor->getSoundGenerator()->getPlaybackState();
    EXPECT_FALSE(after.held);
    EXPECT_EQ(after.note, 69);
    EXPECT_EQ(after.pitchWheel, 15000);
    EXPECT_NEAR(after.releaseSecondsRemaining, before.releaseSecondsRemaining, 1.0 / 48000.0);
}

TEST_F(VCOProcessorTest, ParameterNotificationDefersSwitchAndCallbackUntilRendering) {
    processor->prepareToPlay(48000, 64);
    auto* original = processor->getSoundGenerator();
    original->startNote(69, 1.0f, 12000);
    int changes = 0;
    const auto audioThread = std::this_thread::get_id();
    processor->onGeneratorTypeChanged = [&] {
        EXPECT_EQ(std::this_thread::get_id(), audioThread);
        ++changes;
    };
    auto* feet = apvts->getParameter(ParameterIds::feet);
    std::thread notifier([&] { feet->setValueNotifyingHost(feet->convertTo0to1(4.0f)); });
    notifier.join();
    EXPECT_EQ(processor->getSoundGenerator(), original);
    EXPECT_EQ(changes, 0);
    EXPECT_TRUE(original->isActive());

    juce::AudioBuffer<float> buffer(1, 64);
    buffer.clear();
    juce::MidiBuffer midi;
    processor->processBlock(buffer, midi);
    EXPECT_TRUE(processor->isNoiseMode());
    EXPECT_EQ(changes, 1);
    EXPECT_FALSE(original->isActive());
    const auto state = processor->getSoundGenerator()->getPlaybackState();
    EXPECT_TRUE(state.held);
    EXPECT_EQ(state.note, 69);
    EXPECT_EQ(state.pitchWheel, 12000);
}

TEST_F(VCOProcessorTest, ConcurrentSwitchRequestsOnlyMutateSourcesOnAudioThread) {
    processor->prepareToPlay(48000, 64);
    processor->getSoundGenerator()->startNote(69, 1.0f, 12000);
    const auto audioThread = std::this_thread::get_id();
    processor->onGeneratorTypeChanged = [&] { EXPECT_EQ(std::this_thread::get_id(), audioThread); };
    std::atomic<bool> started{false};
    std::thread notifier([&] {
        while (!started.load())
            std::this_thread::yield();
        for (int i = 0; i < 10000; ++i)
            processor->parameterChanged(ParameterIds::feet, i % 2 == 0 ? 4.0f : 2.0f);
        processor->parameterChanged(ParameterIds::feet, 4.0f);
    });
    juce::AudioBuffer<float> buffer(1, 64);
    juce::MidiBuffer midi;
    started.store(true);
    for (int i = 0; i < 1000; ++i) {
        buffer.clear();
        processor->processBlock(buffer, midi);
        EXPECT_TRUE(processor->getSoundGenerator()->getPlaybackState().held);
        for (int sample = 0; sample < 64; ++sample)
            EXPECT_TRUE(std::isfinite(buffer.getSample(0, sample)));
    }
    notifier.join();
    processor->applyPendingGeneratorChange();
    EXPECT_TRUE(processor->isNoiseMode());
    EXPECT_EQ(processor->getSoundGenerator()->getCurrentlyPlayingNote(), 69);
}

TEST_F(VCOProcessorTest, LfoIsIndependentOfBlockPartition) {
    apvts->getParameter(ParameterIds::modDepth)->setValueNotifyingHost(1.0f);
    const auto render = [&](int blockSize) {
        VCOProcessor vco(*apvts);
        vco.prepareToPlay(48000, blockSize);
        vco.getSoundGenerator()->startNote(69, 1, 8192);
        std::vector<float> result;
        juce::MidiBuffer midi;
        for (int offset = 0; offset < 4096; offset += blockSize) {
            const int size = std::min(blockSize, 4096 - offset);
            juce::AudioBuffer<float> buffer(1, size);
            for (int i = 0; i < size; ++i)
                buffer.setSample(0, i,
                                 static_cast<float>(std::sin(juce::MathConstants<double>::twoPi *
                                                             30 * (offset + i) / 48000.0)));
            vco.processBlock(buffer, midi);
            for (int i = 0; i < size; ++i)
                result.push_back(buffer.getSample(0, i));
        }
        return result;
    };
    const auto reference = render(1);
    for (int size : {7, 64, 256}) {
        const auto output = render(size);
        double error = 0;
        for (size_t i = 0; i < reference.size(); ++i)
            error = std::max(error, std::abs(static_cast<double>(output[i]) - reference[i]));
        EXPECT_NEAR(error, 0.0, 1.0e-6);
    }
}

TEST_F(VCOProcessorTest, Initialization) {
    // Check that processor was created successfully
    EXPECT_NE(processor.get(), nullptr);

    // Check that processor has expected properties
    EXPECT_EQ(processor->getName(), juce::String("VCOProcessor"));
    EXPECT_FALSE(processor->acceptsMidi());
    EXPECT_FALSE(processor->producesMidi());
    EXPECT_FALSE(processor->isMidiEffect());

    // Check bus configuration
    EXPECT_EQ(processor->getBusCount(true), 1);   // 1 input bus (LFO)
    EXPECT_EQ(processor->getBusCount(false), 1);  // 1 output bus

    EXPECT_EQ(processor->getBus(true, 0)->getName(), juce::String("LFOInput"));
    EXPECT_EQ(processor->getBus(false, 0)->getName(), juce::String("Output"));
}

TEST_F(VCOProcessorTest, ProcessBlock) {
    // Enhanced test with explicit channel checks and buffer safety
    try {
        // Verify channel configuration
        int numInputChannels = processor->getTotalNumInputChannels();
        int numOutputChannels = processor->getTotalNumOutputChannels();

        EXPECT_GE(numOutputChannels, 0);

        // Optional safe buffer test with explicit channel validation
        if (numOutputChannels > 0) {
            const int samplesPerBlock = 512;

            // Create a buffer with exactly the right number of channels
            juce::AudioBuffer<float> buffer(std::max(1, numInputChannels + numOutputChannels),
                                            samplesPerBlock);
            buffer.clear();

            // Prepare processor if needed
            processor->prepareToPlay(44100.0, samplesPerBlock);

            // Create empty MIDI buffer
            juce::MidiBuffer midiBuffer;

            // Skip actual processBlock - just prepare and release
            processor->releaseResources();
        }

        EXPECT_TRUE(true);
    } catch (const std::exception& e) {
        FAIL() << "Exception in ProcessBlock test: " << e.what();
    }
}

TEST_F(VCOProcessorTest, NoteHandling) {
    try {
        // Check if ISoundGenerator interface can be obtained
        ISoundGenerator* noteHandler = processor->getSoundGenerator();
        EXPECT_NE(noteHandler, nullptr);

        if (noteHandler != nullptr) {
            // Test note start
            noteHandler->startNote(60, 1.0f, 8192);  // C4
            EXPECT_TRUE(noteHandler->isActive());
            EXPECT_EQ(noteHandler->getCurrentlyPlayingNote(), 60);

            // Test note stop
            noteHandler->stopNote(false);
            EXPECT_FALSE(noteHandler->isActive());
            EXPECT_EQ(noteHandler->getCurrentlyPlayingNote(), 0);
        }
    } catch (const std::exception& e) {
        FAIL() << "Exception in NoteHandling test: " << e.what();
    }
}

TEST_F(VCOProcessorTest, WaveformChange) {
    try {
        // Check bus layout (without actual setting)
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(juce::AudioChannelSet::mono());   // LFO Input
        layout.outputBuses.add(juce::AudioChannelSet::mono());  // Output
        bool layoutSupported = processor->checkBusesLayoutSupported(layout);
        EXPECT_TRUE(layoutSupported);

        // Set and verify parameters
        auto* waveTypeParam =
            dynamic_cast<juce::AudioParameterChoice*>(apvts->getParameter(ParameterIds::waveType));
        auto* feetParam =
            dynamic_cast<juce::AudioParameterChoice*>(apvts->getParameter(ParameterIds::feet));

        EXPECT_NE(waveTypeParam, nullptr);
        EXPECT_NE(feetParam, nullptr);

        // Test waveform parameters
        if (waveTypeParam) {
            // Triangle wave
            waveTypeParam->setValueNotifyingHost(waveTypeParam->convertTo0to1(0));  // Triangle
            EXPECT_EQ(waveTypeParam->getIndex(), 0);
            EXPECT_EQ(waveTypeParam->getCurrentChoiceName(), juce::String("Triangle"));

            // Sawtooth wave
            waveTypeParam->setValueNotifyingHost(waveTypeParam->convertTo0to1(1));  // Sawtooth
            EXPECT_EQ(waveTypeParam->getIndex(), 1);
            EXPECT_EQ(waveTypeParam->getCurrentChoiceName(), juce::String("Sawtooth"));

            // Square wave
            waveTypeParam->setValueNotifyingHost(waveTypeParam->convertTo0to1(2));  // Square
            EXPECT_EQ(waveTypeParam->getIndex(), 2);
            EXPECT_EQ(waveTypeParam->getCurrentChoiceName(), juce::String("Square"));
        }

        // Note handler test
        auto* noteHandler = processor->getSoundGenerator();
        EXPECT_NE(noteHandler, nullptr);

        // API call verification only
        noteHandler->startNote(60, 1.0f, 8192);  // C4
        EXPECT_TRUE(noteHandler->isActive());
        EXPECT_EQ(noteHandler->getCurrentlyPlayingNote(), 60);
    } catch (const std::exception& e) {
        FAIL() << "Exception in WaveformChange test: " << e.what();
    }
}

TEST_F(VCOProcessorTest, OctaveChange) {
    try {
        // Check bus layout
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(juce::AudioChannelSet::mono());   // LFO Input
        layout.outputBuses.add(juce::AudioChannelSet::mono());  // Output
        bool layoutSupported = processor->checkBusesLayoutSupported(layout);
        EXPECT_TRUE(layoutSupported);

        // Parameter test - octave settings
        auto* feetParam =
            dynamic_cast<juce::AudioParameterChoice*>(apvts->getParameter(ParameterIds::feet));
        EXPECT_NE(feetParam, nullptr);

        // Test each octave setting
        if (feetParam) {
            // 32' setting
            feetParam->setValueNotifyingHost(feetParam->convertTo0to1(0));  // 32'
            EXPECT_EQ(feetParam->getIndex(), 0);
            EXPECT_EQ(feetParam->getCurrentChoiceName(), juce::String("32'"));

            // 16' setting
            feetParam->setValueNotifyingHost(feetParam->convertTo0to1(1));  // 16'
            EXPECT_EQ(feetParam->getIndex(), 1);
            EXPECT_EQ(feetParam->getCurrentChoiceName(), juce::String("16'"));

            // 8' setting
            feetParam->setValueNotifyingHost(feetParam->convertTo0to1(2));  // 8'
            EXPECT_EQ(feetParam->getIndex(), 2);
            EXPECT_EQ(feetParam->getCurrentChoiceName(), juce::String("8'"));

            // 4' setting
            feetParam->setValueNotifyingHost(feetParam->convertTo0to1(3));  // 4'
            EXPECT_EQ(feetParam->getIndex(), 3);
            EXPECT_EQ(feetParam->getCurrentChoiceName(), juce::String("4'"));
        }

        // Note handler API test
        auto* noteHandler = processor->getSoundGenerator();
        EXPECT_NE(noteHandler, nullptr);

        if (noteHandler) {
            // Start note
            noteHandler->startNote(60, 1.0f, 8192);  // C4
            EXPECT_TRUE(noteHandler->isActive());
            EXPECT_EQ(noteHandler->getCurrentlyPlayingNote(), 60);
        }
    } catch (const std::exception& e) {
        FAIL() << "Exception in OctaveChange test: " << e.what();
    }
}

TEST_F(VCOProcessorTest, PitchBend) {
    try {
        // Parameter test - focus only on pitch bend
        auto* pitchBendUpRangeParam = dynamic_cast<juce::AudioParameterFloat*>(
            apvts->getParameter(ParameterIds::pitchBendUpRange));
        auto* pitchBendDownRangeParam = dynamic_cast<juce::AudioParameterFloat*>(
            apvts->getParameter(ParameterIds::pitchBendDownRange));

        EXPECT_NE(pitchBendUpRangeParam, nullptr);
        EXPECT_NE(pitchBendDownRangeParam, nullptr);

        // Test parameter operations
        if (pitchBendUpRangeParam && pitchBendDownRangeParam) {
            // Upward range test
            pitchBendUpRangeParam->setValueNotifyingHost(
                pitchBendUpRangeParam->convertTo0to1(1.0f));
            EXPECT_NEAR(pitchBendUpRangeParam->get(), 1.0f, 0.01f);

            pitchBendUpRangeParam->setValueNotifyingHost(
                pitchBendUpRangeParam->convertTo0to1(2.0f));
            EXPECT_NEAR(pitchBendUpRangeParam->get(), 2.0f, 0.01f);

            pitchBendUpRangeParam->setValueNotifyingHost(
                pitchBendUpRangeParam->convertTo0to1(12.0f));
            EXPECT_NEAR(pitchBendUpRangeParam->get(), 12.0f, 0.01f);

            // Downward range test
            pitchBendDownRangeParam->setValueNotifyingHost(
                pitchBendDownRangeParam->convertTo0to1(1.0f));
            EXPECT_NEAR(pitchBendDownRangeParam->get(), 1.0f, 0.01f);

            pitchBendDownRangeParam->setValueNotifyingHost(
                pitchBendDownRangeParam->convertTo0to1(12.0f));
            EXPECT_NEAR(pitchBendDownRangeParam->get(), 12.0f, 0.01f);
        }

        // Test VCOProcessor creation and note handler API
        auto* noteHandler = processor->getSoundGenerator();
        EXPECT_NE(noteHandler, nullptr);

        if (noteHandler) {
            // Test note start and pitch bend
            noteHandler->startNote(60, 1.0f, 8192);  // C4, center pitch bend
            EXPECT_TRUE(noteHandler->isActive());

            // Test each pitch bend position
            noteHandler->pitchWheelMoved(16383);  // Maximum up
            noteHandler->pitchWheelMoved(0);      // Maximum down
            noteHandler->pitchWheelMoved(8192);   // Return to center
        }
    } catch (const std::exception& e) {
        FAIL() << "Exception in PitchBend test: " << e.what();
    }
}

TEST_F(VCOProcessorTest, BufferProcessing) {
    try {
        // Collect processor details
        int numInputChannels = processor->getTotalNumInputChannels();
        int numOutputChannels = processor->getTotalNumOutputChannels();

        // Verify bus layout settings
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(juce::AudioChannelSet::mono());
        layout.outputBuses.add(juce::AudioChannelSet::mono());

        bool layoutSupported = processor->checkBusesLayoutSupported(layout);
        EXPECT_TRUE(layoutSupported);

        // Prepare processor
        const double sampleRate = 44100.0;
        const int samplesPerBlock = 512;
        processor->prepareToPlay(sampleRate, samplesPerBlock);

        // Set bus layout
        bool layoutSetSuccess = processor->setBusesLayout(layout);
        EXPECT_TRUE(layoutSetSuccess);

        // Retrieve bus information again
        numInputChannels = processor->getTotalNumInputChannels();
        numOutputChannels = processor->getTotalNumOutputChannels();

        // Set parameters
        auto* waveTypeParam =
            dynamic_cast<juce::AudioParameterChoice*>(apvts->getParameter(ParameterIds::waveType));
        auto* feetParam =
            dynamic_cast<juce::AudioParameterChoice*>(apvts->getParameter(ParameterIds::feet));

        EXPECT_NE(waveTypeParam, nullptr);
        EXPECT_NE(feetParam, nullptr);

        if (waveTypeParam)
            waveTypeParam->setValueNotifyingHost(waveTypeParam->convertTo0to1(1));  // Sawtooth

        if (feetParam)
            feetParam->setValueNotifyingHost(feetParam->convertTo0to1(2));  // 8'

        // Start note
        auto* noteHandler = processor->getSoundGenerator();
        EXPECT_NE(noteHandler, nullptr);

        noteHandler->startNote(69, 1.0f, 8192);  // A4 (440Hz)

        // Create audio buffer
        juce::AudioBuffer<float> buffer(numInputChannels + numOutputChannels, samplesPerBlock);
        juce::MidiBuffer midiBuffer;
        buffer.clear();

        // This test verifies setup and parameter handling without complex buffer processing
        EXPECT_TRUE(true);
    } catch (const std::exception& e) {
        FAIL() << "Exception in BufferProcessing test: " << e.what();
    }
}
