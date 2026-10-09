#include <JuceHeader.h>

#include "CS01AudioProcessor.h"
#include "CS01Synth/EGProcessor.h"
#include "Parameters.h"

#include <gtest/gtest.h>

namespace {
// Closed-form midpoint for the stateful exponential stage (k = 2).
float provisionalHalfLevel(float initial, float) {
    return initial * (1.0 - (1.0 - std::exp(-1.0)) / (1.0 - std::exp(-2.0)));
}
}  // namespace

TEST(EGTimingTest, ProductionRangeStageDurations) {
    // Use production parameter ranges, not the different ranges in the unit fixture.
    CS01AudioProcessor owner;
    auto& state = owner.apvts;
    for (double rate : {44100.0, 48000.0, 96000.0, 192000.0, 384000.0}) {
        for (float position : {0.0f, 0.5f, 1.0f}) {
            SCOPED_TRACE(rate);
            SCOPED_TRACE(position);
            for (const auto& id :
                 {ParameterIds::attack, ParameterIds::decay, ParameterIds::release})
                state.getParameter(id)->setValueNotifyingHost(position);
            state.getParameter(ParameterIds::sustain)->setValueNotifyingHost(0.5f);
            EGProcessor eg(state);
            eg.prepareToPlay(rate, 256);
            juce::AudioBuffer<float> buffer(1, 256);
            juce::MidiBuffer midi;
            int index = 256;
            auto next = [&]() {
                if (index == 256) {
                    eg.processBlock(buffer, midi);
                    index = 0;
                }
                return buffer.getSample(0, index++);
            };
            auto duration = [&](auto done) {
                const int limit = static_cast<int>(rate * 3.0);
                for (int n = 1; n <= limit; ++n)
                    if (done(next()))
                        return n;
                return limit + 1;
            };
            auto check = [&](const auto& id, int samples) {
                const double expected = state.getRawParameterValue(id)->load();
                // Explicit numerical timing target, not a CS-01 hardware tolerance.
                EXPECT_NEAR(samples / rate, expected, expected * 0.01 + 2.0 / rate);
            };
            eg.startEnvelope();
            check(ParameterIds::attack, duration([](float v) { return v >= 1.0f; }));
            check(ParameterIds::decay, duration([](float v) { return v <= 0.5f; }));
            // Drain the pre-rendered block before applying the note-off event.
            while (index < 256)
                next();
            eg.releaseEnvelope();
            check(ParameterIds::release, duration([](float v) { return v == 0.0f; }));
        }
    }
}

// Test fixture for EGProcessor tests
class EGProcessorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Create a dummy processor for APVTS
        dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();

        auto parameterLayout = createParameterLayout();
        apvts = std::make_unique<juce::AudioProcessorValueTreeState>(
            *dummyProcessor, nullptr, "PARAMETERS", std::move(parameterLayout));

        // Create EGProcessor
        processor = std::make_unique<EGProcessor>(*apvts);
    }

    void TearDown() override {
        processor.reset();
        apvts.reset();
        dummyProcessor.reset();
    }

    // Create a parameter layout for testing
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // Add parameters needed for EGProcessor
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::attack, "Attack",
            juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.5f), 0.1f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::decay, "Decay",
            juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.5f), 0.3f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::sustain, "Sustain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::release, "Release",
            juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.5f), 0.5f));

        return layout;
    }

    std::unique_ptr<juce::AudioProcessorGraph> dummyProcessor;
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
    std::unique_ptr<EGProcessor> processor;
};

TEST_F(EGProcessorTest, SustainAutomationRemainsBoundedAndReachesTarget) {
    // Generic ADSR behavior, not a claim about CS-01 analog switching thresholds.
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        SCOPED_TRACE(rate);
        auto set = [&](const juce::String& id, float value) {
            auto* parameter = apvts->getParameter(id);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        };
        set(ParameterIds::attack, 0.01f);
        set(ParameterIds::decay, 0.05f);
        set(ParameterIds::sustain, 0.5f);
        processor->prepareToPlay(rate, 64);
        processor->startEnvelope();
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> sample(1, 1);
        auto advance = [&](int count) {
            for (int i = 0; i < count; ++i) {
                processor->processBlock(sample, midi);
                const float value = sample.getSample(0, 0);
                EXPECT_TRUE(std::isfinite(value));
                EXPECT_GE(value, 0.0f);
                EXPECT_LE(value, 1.0f);
            }
        };
        advance(static_cast<int>(rate * 0.1));
        ASSERT_NEAR(sample.getSample(0, 0), 0.5f, 1.0e-5f);
        for (float target : {0.8f, 0.2f, 1.0f, 0.0f, 0.5f}) {
            set(ParameterIds::sustain, target);
            advance(static_cast<int>(rate * 0.1));
            EXPECT_NEAR(sample.getSample(0, 0), target, 1.0e-5f);
        }
        processor->releaseEnvelope();
        advance(static_cast<int>(rate * 0.6));
        EXPECT_FALSE(processor->isActive());
        EXPECT_FLOAT_EQ(sample.getSample(0, 0), 0.0f);
    }
}

TEST_F(EGProcessorTest, StatefulEditsAndRetriggerDoNotJump) {
    processor->prepareToPlay(48000.0, 1);
    processor->startEnvelope();
    juce::AudioBuffer<float> buffer(1, 1);
    juce::MidiBuffer midi;
    for (int i = 0; i < 24000; ++i)
        processor->processBlock(buffer, midi);
    ASSERT_NEAR(processor->getLastOutputForTesting(), 0.5f, 1.0e-6f);
    apvts->getParameter(ParameterIds::sustain)->setValueNotifyingHost(0.8f);
    const float before = processor->getLastOutputForTesting();
    processor->processBlock(buffer, midi);
    EXPECT_GT(buffer.getSample(0, 0), before);
    EXPECT_LT(buffer.getSample(0, 0) - before, 0.001f);
    processor->releaseEnvelope();
    for (int i = 0; i < 1000; ++i)
        processor->processBlock(buffer, midi);
    const float releasingLevel = processor->getLastOutputForTesting();
    processor->startEnvelope();
    EXPECT_FLOAT_EQ(processor->getLastOutputForTesting(), releasingLevel);
    processor->processBlock(buffer, midi);
    EXPECT_GT(buffer.getSample(0, 0), releasingLevel);
    EXPECT_LT(buffer.getSample(0, 0) - releasingLevel, 0.001f);
}

TEST_F(EGProcessorTest, AttackAutomationPreservesLevelAndProgress) {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        processor->prepareToPlay(rate, static_cast<int>(rate * 0.02));
        processor->startEnvelope();
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> initial(1, static_cast<int>(rate * 0.02));
        processor->processBlock(initial, midi);
        const float before = processor->getLastOutputForTesting();
        ASSERT_GT(before, 0.0f);
        auto* attack = apvts->getParameter(ParameterIds::attack);
        attack->setValueNotifyingHost(attack->convertTo0to1(0.5f));
        juce::AudioBuffer<float> next(1, 256);
        processor->processBlock(next, midi);
        float previous = before;
        for (int i = 0; i < next.getNumSamples(); ++i) {
            const float value = next.getSample(0, i);
            ASSERT_TRUE(std::isfinite(value));
            EXPECT_GE(value, previous);
            EXPECT_LE(value, 1.0f);
            EXPECT_LT(value - previous, 0.001f);
            previous = value;
        }
        EXPECT_TRUE(processor->isActive());
        attack->setValueNotifyingHost(attack->convertTo0to1(0.1f));
    }
}

TEST_F(EGProcessorTest, EarlyReleaseIsIndependentOfBlockPartition) {
    // Implementation invariant, not a hardware-calibrated envelope curve.
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        const int noteOff = static_cast<int>(sampleRate * 0.037);
        const int count = static_cast<int>(sampleRate * 0.7);
        std::vector<float> reference;
        for (int blockSize : {1, 7, 64, 256}) {
            SCOPED_TRACE(sampleRate);
            SCOPED_TRACE(blockSize);
            EGProcessor envelope(*apvts);
            envelope.prepareToPlay(sampleRate, blockSize);
            envelope.startEnvelope();
            juce::MidiBuffer midi;
            std::vector<float> output;
            for (int offset = 0; offset < count;) {
                if (offset == noteOff)
                    envelope.releaseEnvelope();
                const int boundary = offset < noteOff ? noteOff : count;
                const int length = std::min(blockSize, boundary - offset);
                juce::AudioBuffer<float> buffer(1, length);
                envelope.processBlock(buffer, midi);
                for (int i = 0; i < length; ++i) {
                    const float value = buffer.getSample(0, i);
                    ASSERT_TRUE(std::isfinite(value));
                    ASSERT_GE(value, 0.0f);
                    ASSERT_LE(value, 1.0f);
                    output.push_back(value);
                }
                offset += length;
            }
            EXPECT_FALSE(envelope.isActive());
            EXPECT_FLOAT_EQ(output.back(), 0.0f);
            ASSERT_GT(output[noteOff - 1], 0.0f);
            EXPECT_LE(output[noteOff], output[noteOff - 1]);
            if (reference.empty())
                reference = output;
            else {
                ASSERT_EQ(output.size(), reference.size());
                for (size_t i = 0; i < output.size(); ++i)
                    ASSERT_NEAR(output[i], reference[i], 1.0e-6f) << "sample=" << i;
            }
        }
    }
}

TEST_F(EGProcessorTest, LifecycleClearsEnvelopeAndObservation) {
    for (bool duringRelease : {false, true}) {
        processor->prepareToPlay(48000.0, 64);
        processor->startEnvelope();
        juce::AudioBuffer<float> buffer(1, 64);
        juce::MidiBuffer midi;
        processor->processBlock(buffer, midi);
        if (duringRelease) {
            processor->releaseEnvelope();
            processor->processBlock(buffer, midi);
        }
        ASSERT_TRUE(processor->isActive());
        ASSERT_GT(processor->getLastOutputForTesting(), 0.0f);
        processor->releaseResources();
        EXPECT_FALSE(processor->isActive());
        EXPECT_FLOAT_EQ(processor->getLastOutputForTesting(), 0.0f);
        processor->prepareToPlay(96000.0, 64);
        processor->processBlock(buffer, midi);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            EXPECT_FLOAT_EQ(buffer.getSample(0, i), 0.0f);

        processor->startEnvelope();
        processor->processBlock(buffer, midi);
        processor->prepareToPlay(44100.0, 64);
        EXPECT_FALSE(processor->isActive());
        EXPECT_FLOAT_EQ(processor->getLastOutputForTesting(), 0.0f);
    }
}

TEST_F(EGProcessorTest, EarlyReleaseKeepsNoteOffRateWithZeroSustain) {
    apvts->getParameter(ParameterIds::sustain)->setValueNotifyingHost(0.0f);
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        SCOPED_TRACE(sampleRate);
        EGProcessor envelope(*apvts);
        envelope.prepareToPlay(sampleRate, 1);
        envelope.startEnvelope();
        juce::AudioBuffer<float> buffer(1, 1);
        juce::MidiBuffer midi;
        for (int i = 0; i < static_cast<int>(sampleRate * 0.025); ++i)
            envelope.processBlock(buffer, midi);
        const float initial = buffer.getSample(0, 0);
        ASSERT_GT(initial, 0.0f);
        envelope.releaseEnvelope();
        // The configured release is 0.5 s. A halfway observation must remain
        // audible, even when the key was released before reaching sustain.
        for (int i = 0; i < static_cast<int>(sampleRate * 0.25); ++i)
            envelope.processBlock(buffer, midi);
        EXPECT_TRUE(envelope.isActive());
        EXPECT_NEAR(buffer.getSample(0, 0), provisionalHalfLevel(initial, 0.0f), initial * 0.002f);
        for (int i = 0; i < static_cast<int>(sampleRate * 0.26); ++i)
            envelope.processBlock(buffer, midi);
        EXPECT_FALSE(envelope.isActive());
        EXPECT_FLOAT_EQ(buffer.getSample(0, 0), 0.0f);
    }
}

TEST_F(EGProcessorTest, AttackAndDecayChangesDoNotAlterRunningRelease) {
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        const auto render = [&](bool change) {
            for (const auto& id : {ParameterIds::attack, ParameterIds::decay}) {
                auto* parameter = apvts->getParameter(id);
                parameter->setValueNotifyingHost(parameter->convertTo0to1(0.1f));
            }
            EGProcessor envelope(*apvts);
            envelope.prepareToPlay(sampleRate, 1);
            envelope.startEnvelope();
            juce::AudioBuffer<float> buffer(1, 1);
            juce::MidiBuffer midi;
            for (int i = 0; i < static_cast<int>(sampleRate * 0.025); ++i)
                envelope.processBlock(buffer, midi);
            envelope.releaseEnvelope();
            std::vector<float> result;
            for (int i = 0; i < static_cast<int>(sampleRate * 0.6); ++i) {
                if (change && i == static_cast<int>(sampleRate * 0.05)) {
                    for (const auto& id : {ParameterIds::attack, ParameterIds::decay}) {
                        auto* parameter = apvts->getParameter(id);
                        parameter->setValueNotifyingHost(parameter->convertTo0to1(0.2f));
                    }
                }
                envelope.processBlock(buffer, midi);
                result.push_back(buffer.getSample(0, 0));
            }
            return result;
        };
        const auto reference = render(false);
        const auto changed = render(true);
        ASSERT_EQ(reference.size(), changed.size());
        for (size_t i = 0; i < reference.size(); ++i)
            ASSERT_FLOAT_EQ(reference[i], changed[i]) << "fs=" << sampleRate << ", sample=" << i;
    }
}

TEST_F(EGProcessorTest, ReleaseTimeChangeUsesCurrentLevelNotSustain) {
    // Model policy: a changed release duration starts at the current level.
    // This is not a claim about the hardware's RC decay curve.
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        for (float duration : {0.05f, 0.2f}) {
            SCOPED_TRACE(sampleRate);
            SCOPED_TRACE(duration);
            apvts->getParameter(ParameterIds::sustain)->setValueNotifyingHost(0.0f);
            auto* release = apvts->getParameter(ParameterIds::release);
            release->setValueNotifyingHost(release->convertTo0to1(0.5f));
            EGProcessor envelope(*apvts);
            envelope.prepareToPlay(sampleRate, 1);
            envelope.startEnvelope();
            juce::AudioBuffer<float> buffer(1, 1);
            juce::MidiBuffer midi;
            for (int i = 0; i < static_cast<int>(sampleRate * 0.025); ++i)
                envelope.processBlock(buffer, midi);
            envelope.releaseEnvelope();
            for (int i = 0; i < static_cast<int>(sampleRate * 0.05); ++i)
                envelope.processBlock(buffer, midi);
            const float initial = buffer.getSample(0, 0);
            ASSERT_GT(initial, 0.0f);
            release->setValueNotifyingHost(release->convertTo0to1(duration));
            envelope.processBlock(buffer, midi);
            EXPECT_TRUE(envelope.isActive());
            EXPECT_NEAR(buffer.getSample(0, 0),
                        initial * (1.0 - (1.0 - std::exp(-2.0 / (duration * sampleRate))) /
                                             (1.0 - std::exp(-2.0))),
                        1.0e-5);
            const int halfway = static_cast<int>(duration * sampleRate * 0.5);
            for (int i = 1; i < halfway; ++i)
                envelope.processBlock(buffer, midi);
            EXPECT_NEAR(buffer.getSample(0, 0), provisionalHalfLevel(initial, 0.0f),
                        initial * 0.003f);
            for (int i = 0; i < static_cast<int>(duration * sampleRate * 0.6); ++i)
                envelope.processBlock(buffer, midi);
            EXPECT_FALSE(envelope.isActive());
            EXPECT_FLOAT_EQ(buffer.getSample(0, 0), 0.0f);
        }
    }
}

TEST_F(EGProcessorTest, RetriggerRestoresSustainAfterReleaseTimeChange) {
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        for (bool finishRelease : {false, true}) {
            SCOPED_TRACE(sampleRate);
            SCOPED_TRACE(finishRelease);
            apvts->getParameter(ParameterIds::sustain)->setValueNotifyingHost(0.6f);
            auto* release = apvts->getParameter(ParameterIds::release);
            release->setValueNotifyingHost(release->convertTo0to1(0.5f));
            EGProcessor envelope(*apvts);
            envelope.prepareToPlay(sampleRate, 1);
            juce::AudioBuffer<float> buffer(1, 1);
            juce::MidiBuffer midi;
            const auto advance = [&](double seconds) {
                for (int i = 0; i < static_cast<int>(sampleRate * seconds); ++i) {
                    envelope.processBlock(buffer, midi);
                    ASSERT_TRUE(std::isfinite(buffer.getSample(0, 0)));
                }
            };
            envelope.startEnvelope();
            advance(0.025);
            envelope.releaseEnvelope();
            advance(0.05);
            release->setValueNotifyingHost(release->convertTo0to1(0.2f));
            advance(0.001);
            ASSERT_TRUE(envelope.isActive());
            if (finishRelease) {
                advance(0.25);
                ASSERT_FALSE(envelope.isActive());
            }
            // Also cover a sustain edit deferred while releasing.
            apvts->getParameter(ParameterIds::sustain)->setValueNotifyingHost(0.8f);
            envelope.startEnvelope();
            advance(0.5);
            EXPECT_TRUE(envelope.isActive());
            EXPECT_NEAR(buffer.getSample(0, 0), 0.8f, 1.0e-6f);
            envelope.releaseEnvelope();
            advance(0.1);
            EXPECT_NEAR(buffer.getSample(0, 0), provisionalHalfLevel(0.8f, 0.8f), 0.002f);
            advance(0.12);
            EXPECT_FALSE(envelope.isActive());
            EXPECT_FLOAT_EQ(buffer.getSample(0, 0), 0.0f);
        }
    }
}

TEST_F(EGProcessorTest, Initialization) {
    // Check that processor was created successfully
    EXPECT_NE(processor.get(), nullptr);

    // Check that processor has expected properties
    EXPECT_EQ(processor->getName(), juce::String("EG"));
    EXPECT_FALSE(processor->acceptsMidi());
    EXPECT_FALSE(processor->producesMidi());
    EXPECT_FALSE(processor->isMidiEffect());

    // Check bus configuration
    EXPECT_EQ(processor->getBusCount(true), 0);   // No input buses
    EXPECT_EQ(processor->getBusCount(false), 1);  // 1 output bus

    EXPECT_EQ(processor->getBus(false, 0)->getName(), juce::String("Output"));

    // Check initial state
    EXPECT_FALSE(processor->isActive());
}

TEST_F(EGProcessorTest, ParameterSettings) {
    // Test attack parameter
    auto* attackParam = apvts->getParameter(ParameterIds::attack);
    EXPECT_NE(attackParam, nullptr);

    // Set attack to different values
    attackParam->setValueNotifyingHost(attackParam->convertTo0to1(0.05f));  // 50ms
    EXPECT_NEAR(apvts->getRawParameterValue(ParameterIds::attack)->load(), 0.05f, 0.001f);

    attackParam->setValueNotifyingHost(attackParam->convertTo0to1(0.5f));  // 500ms
    EXPECT_NEAR(apvts->getRawParameterValue(ParameterIds::attack)->load(), 0.5f, 0.001f);

    // Test decay parameter
    auto* decayParam = apvts->getParameter(ParameterIds::decay);
    EXPECT_NE(decayParam, nullptr);

    // Set decay to different values
    decayParam->setValueNotifyingHost(decayParam->convertTo0to1(0.1f));  // 100ms
    EXPECT_NEAR(apvts->getRawParameterValue(ParameterIds::decay)->load(), 0.1f, 0.001f);

    decayParam->setValueNotifyingHost(decayParam->convertTo0to1(1.0f));  // 1s
    EXPECT_NEAR(apvts->getRawParameterValue(ParameterIds::decay)->load(), 1.0f, 0.001f);

    // Test sustain parameter
    auto* sustainParam = apvts->getParameter(ParameterIds::sustain);
    EXPECT_NE(sustainParam, nullptr);

    // Set sustain to different values
    sustainParam->setValueNotifyingHost(0.25f);  // 25%
    EXPECT_NEAR(apvts->getRawParameterValue(ParameterIds::sustain)->load(), 0.25f, 0.001f);

    sustainParam->setValueNotifyingHost(0.75f);  // 75%
    EXPECT_NEAR(apvts->getRawParameterValue(ParameterIds::sustain)->load(), 0.75f, 0.001f);

    // Test release parameter
    auto* releaseParam = apvts->getParameter(ParameterIds::release);
    EXPECT_NE(releaseParam, nullptr);

    // Set release to different values
    releaseParam->setValueNotifyingHost(releaseParam->convertTo0to1(0.2f));  // 200ms
    EXPECT_NEAR(apvts->getRawParameterValue(ParameterIds::release)->load(), 0.2f, 0.001f);

    releaseParam->setValueNotifyingHost(releaseParam->convertTo0to1(2.0f));  // 2s
    EXPECT_NEAR(apvts->getRawParameterValue(ParameterIds::release)->load(), 2.0f, 0.001f);
}

TEST_F(EGProcessorTest, BusesLayout) {
    // Test supported buses layout
    juce::AudioProcessor::BusesLayout supportedLayout;
    supportedLayout.inputBuses.clear();                              // No input buses
    supportedLayout.outputBuses.add(juce::AudioChannelSet::mono());  // Mono output

    EXPECT_TRUE(processor->isBusesLayoutSupported(supportedLayout));

    // Test unsupported buses layout (with input)
    juce::AudioProcessor::BusesLayout unsupportedLayout1;
    unsupportedLayout1.inputBuses.add(juce::AudioChannelSet::mono());   // Input bus (not supported)
    unsupportedLayout1.outputBuses.add(juce::AudioChannelSet::mono());  // Mono output

    EXPECT_FALSE(processor->isBusesLayoutSupported(unsupportedLayout1));

    // Test unsupported buses layout (stereo output)
    juce::AudioProcessor::BusesLayout unsupportedLayout2;
    unsupportedLayout2.inputBuses.clear();  // No input buses
    unsupportedLayout2.outputBuses.add(
        juce::AudioChannelSet::stereo());  // Stereo output (not supported)

    EXPECT_FALSE(processor->isBusesLayoutSupported(unsupportedLayout2));
}

TEST_F(EGProcessorTest, EnvelopeGeneration) {
    // Prepare processor
    processor->prepareToPlay(44100.0, 512);

    // Create audio buffer
    juce::AudioBuffer<float> buffer(1, 512);  // 1 channel for output
    juce::MidiBuffer midiBuffer;

    // Set ADSR parameters for testing
    apvts->getParameter(ParameterIds::attack)
        ->setValueNotifyingHost(
            apvts->getParameter(ParameterIds::attack)->convertTo0to1(0.01f));  // 10ms attack

    apvts->getParameter(ParameterIds::decay)
        ->setValueNotifyingHost(
            apvts->getParameter(ParameterIds::decay)->convertTo0to1(0.1f));  // 100ms decay

    apvts->getParameter(ParameterIds::sustain)->setValueNotifyingHost(0.5f);  // 50% sustain

    apvts->getParameter(ParameterIds::release)
        ->setValueNotifyingHost(
            apvts->getParameter(ParameterIds::release)->convertTo0to1(0.2f));  // 200ms release

    // Trigger note on
    processor->startEnvelope();

    // Process block (should generate attack phase)
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);

    // Check that envelope is active
    EXPECT_TRUE(processor->isActive());

    // Check that output buffer has non-zero values
    float sum = 0.0f;
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        sum += std::abs(buffer.getSample(0, i));
    }
    EXPECT_GT(sum, 0.0001f);

    // Process more blocks to reach sustain phase
    for (int i = 0; i < 10; ++i) {
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
    }

    // Check that envelope is still active
    EXPECT_TRUE(processor->isActive());

    // Trigger note off
    processor->releaseEnvelope();

    // Process block (should generate release phase)
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);

    // Check that envelope is still active during release phase
    EXPECT_TRUE(processor->isActive());

    // Process more blocks to complete release phase
    for (int i = 0; i < 20; ++i) {
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
    }

    // Check that envelope is no longer active
    EXPECT_FALSE(processor->isActive());
}

TEST_F(EGProcessorTest, ContinuousAttackDecayAndRelease) {
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        SCOPED_TRACE(sampleRate);
        processor = std::make_unique<EGProcessor>(*apvts);
        processor->prepareToPlay(sampleRate, 1);
        juce::AudioBuffer<float> buffer(1, 1);
        juce::MidiBuffer midi;
        processor->startEnvelope();
        float previous = 0.0f;
        bool reachedPeak = false;
        for (int i = 0; i < static_cast<int>(sampleRate * 0.5); ++i) {
            processor->processBlock(buffer, midi);
            const float value = buffer.getSample(0, 0);
            ASSERT_TRUE(std::isfinite(value));
            ASSERT_GE(value, 0.0f);
            ASSERT_LE(value, 1.0f);
            ASSERT_LE(std::abs(value - previous), 2.32 / (0.1 * sampleRate) + 0.00001);
            if (!reachedPeak) {
                ASSERT_GE(value + 0.000001f, previous);
                reachedPeak = value == 1.0f;
            } else {
                ASSERT_LE(value, previous + 0.000001f);
            }
            previous = value;
        }
        EXPECT_TRUE(reachedPeak);
        EXPECT_NEAR(previous, 0.5f, 0.00001f);
        processor->releaseEnvelope();
        for (int i = 0; i < static_cast<int>(sampleRate * 0.6); ++i) {
            processor->processBlock(buffer, midi);
            const float value = buffer.getSample(0, 0);
            ASSERT_GE(value, 0.0f);
            ASSERT_LE(value, previous + 0.000001f);
            ASSERT_LE(std::abs(value - previous), 1.16 / (0.5 * sampleRate) + 0.00001);
            previous = value;
        }
        EXPECT_FLOAT_EQ(previous, 0.0f);
        EXPECT_FALSE(processor->isActive());
    }
}

TEST_F(EGProcessorTest, NoteOnOff) {
    // Prepare processor
    processor->prepareToPlay(44100.0, 512);

    // Check initial state
    EXPECT_FALSE(processor->isActive());

    // Trigger note on
    processor->startEnvelope();

    // Check that envelope is active
    EXPECT_TRUE(processor->isActive());

    // Trigger note off
    processor->releaseEnvelope();

    // Note: We can't check that envelope is inactive immediately after noteOff
    // because the release phase takes time. In a real test, we would process
    // audio blocks until the release phase is complete.
}
