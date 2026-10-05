#include <JuceHeader.h>

#include "CS01Synth/LFOProcessor.h"
#include "Parameters.h"

#include <gtest/gtest.h>

// Test fixture for LFOProcessor tests
class LFOProcessorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Create a dummy processor for APVTS
        dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();

        auto parameterLayout = createParameterLayout();
        apvts = std::make_unique<juce::AudioProcessorValueTreeState>(
            *dummyProcessor, nullptr, "PARAMETERS", std::move(parameterLayout));

        // Create LFOProcessor
        processor = std::make_unique<LFOProcessor>(*apvts);
    }

    void TearDown() override {
        processor.reset();
        apvts.reset();
        dummyProcessor.reset();
    }

    // Create a parameter layout for testing
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // Add parameters needed for LFOProcessor
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::lfoSpeed, "LFO Speed",
            juce::NormalisableRange<float>(0.1f, 21.0f, 0.01f, 0.5f), 1.0f));

        return layout;
    }

    std::unique_ptr<juce::AudioProcessorGraph> dummyProcessor;
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
    std::unique_ptr<LFOProcessor> processor;
};

TEST_F(LFOProcessorTest, Initialization) {
    // Check that processor was created successfully
    EXPECT_NE(processor.get(), nullptr);

    // Check that processor has expected properties
    EXPECT_EQ(processor->getName(), juce::String("LFO"));
    EXPECT_FALSE(processor->acceptsMidi());
    EXPECT_FALSE(processor->producesMidi());
    EXPECT_FALSE(processor->isMidiEffect());

    // Check bus configuration
    EXPECT_EQ(processor->getBusCount(true), 0);   // No input buses
    EXPECT_EQ(processor->getBusCount(false), 1);  // 1 output bus

    EXPECT_EQ(processor->getBus(false, 0)->getName(), juce::String("Output"));
}

TEST_F(LFOProcessorTest, ParameterSettings) {
    // Test LFO speed parameter
    auto* lfoSpeedParam = apvts->getParameter(ParameterIds::lfoSpeed);
    EXPECT_NE(lfoSpeedParam, nullptr);

    // Set LFO speed to different values
    lfoSpeedParam->setValueNotifyingHost(lfoSpeedParam->convertTo0to1(0.5f));  // 0.5 Hz
    EXPECT_NEAR(apvts->getRawParameterValue(ParameterIds::lfoSpeed)->load(), 0.5f, 0.001f);

    lfoSpeedParam->setValueNotifyingHost(lfoSpeedParam->convertTo0to1(5.0f));  // 5.0 Hz
    EXPECT_NEAR(apvts->getRawParameterValue(ParameterIds::lfoSpeed)->load(), 5.0f, 0.001f);
}

TEST_F(LFOProcessorTest, BusesLayout) {
    // Test supported buses layout
    juce::AudioProcessor::BusesLayout supportedLayout;
    supportedLayout.inputBuses.clear();                              // No input buses
    supportedLayout.outputBuses.add(juce::AudioChannelSet::mono());  // Mono output

    EXPECT_TRUE(processor->isBusesLayoutSupported(supportedLayout));

    // Test supported buses layout with input (LFOProcessor only checks output)
    juce::AudioProcessor::BusesLayout supportedLayout2;
    supportedLayout2.inputBuses.add(juce::AudioChannelSet::mono());   // Input bus (supported)
    supportedLayout2.outputBuses.add(juce::AudioChannelSet::mono());  // Mono output

    EXPECT_TRUE(processor->isBusesLayoutSupported(supportedLayout2));

    // Test unsupported buses layout (stereo output)
    juce::AudioProcessor::BusesLayout unsupportedLayout;
    unsupportedLayout.inputBuses.clear();  // No input buses
    unsupportedLayout.outputBuses.add(
        juce::AudioChannelSet::stereo());  // Stereo output (not supported)

    EXPECT_FALSE(processor->isBusesLayoutSupported(unsupportedLayout));
}

TEST_F(LFOProcessorTest, WaveformGeneration) {
    // Prepare processor
    processor->prepareToPlay(44100.0, 512);

    // Create audio buffer
    juce::AudioBuffer<float> buffer(1, 512);  // 1 channel for output
    juce::MidiBuffer midiBuffer;

    // Set LFO speed to 1 Hz
    apvts->getParameter(ParameterIds::lfoSpeed)
        ->setValueNotifyingHost(
            apvts->getParameter(ParameterIds::lfoSpeed)->convertTo0to1(1.0f));  // 1.0 Hz

    // Process block with 1 Hz LFO
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);

    // Check that output buffer has non-zero values
    float sum = 0.0f;
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        sum += std::abs(buffer.getSample(0, i));
    }
    EXPECT_GT(sum, 0.0001f);

    // Store the output for comparison
    juce::AudioBuffer<float> slowLfoBuffer;
    slowLfoBuffer.makeCopyOf(buffer);

    // Set LFO speed to 5 Hz
    apvts->getParameter(ParameterIds::lfoSpeed)
        ->setValueNotifyingHost(
            apvts->getParameter(ParameterIds::lfoSpeed)->convertTo0to1(5.0f));  // 5.0 Hz

    // Process block with 5 Hz LFO
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);

    // Compare the outputs - they should be different
    bool isDifferent = false;
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        if (std::abs(buffer.getSample(0, i) - slowLfoBuffer.getSample(0, i)) > 0.0001f) {
            isDifferent = true;
            break;
        }
    }

    EXPECT_TRUE(isDifferent) << "Different LFO speeds should produce different outputs";

    // Period and triangle continuity are checked after warm-up in the test below.

    // Also verify that there is significant variation in the output
    float minVal = slowLfoBuffer.getSample(0, 0);
    float maxVal = slowLfoBuffer.getSample(0, 0);

    for (int i = 0; i < slowLfoBuffer.getNumSamples(); ++i) {
        float sample = slowLfoBuffer.getSample(0, i);
        minVal = std::min(minVal, sample);
        maxVal = std::max(maxVal, sample);
    }

    float range = maxVal - minVal;
    EXPECT_GT(range, 0.01f) << "LFO should vary even over a short observation window";
}

TEST_F(LFOProcessorTest, PeriodAmplitudeAndContinuity) {
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        for (float frequency : {0.8f, 10.9f, 21.0f}) {
            SCOPED_TRACE(sampleRate);
            SCOPED_TRACE(frequency);
            // Physical Hz probes; the public parameter endpoints are tested
            // separately in ModulationRangeTest.
            processor = std::make_unique<LFOProcessor>(*apvts);
            apvts->getParameter(ParameterIds::lfoSpeed)
                ->setValueNotifyingHost(
                    apvts->getParameter(ParameterIds::lfoSpeed)->convertTo0to1(frequency));
            processor->prepareToPlay(sampleRate, 256);
            juce::AudioBuffer<float> buffer(1, 256);
            juce::MidiBuffer midi;
            // Discard the oscillator's frequency smoothing interval.
            for (int i = 0; i < static_cast<int>(sampleRate / 256); ++i)
                processor->processBlock(buffer, midi);
            float previous = 0.0f;
            float minimum = 1.0f, maximum = -1.0f;
            int lastCrossing = -1, periods = 0, index = 0;
            const int count = static_cast<int>(sampleRate * 3.0 / frequency);
            while (index < count) {
                processor->processBlock(buffer, midi);
                for (int i = 0; i < 256 && index < count; ++i, ++index) {
                    const float value = buffer.getSample(0, i);
                    ASSERT_TRUE(std::isfinite(value));
                    minimum = std::min(minimum, value);
                    maximum = std::max(maximum, value);
                    if (index > 0) {
                        ASSERT_LE(std::abs(value - previous),
                                  4.0 * frequency / sampleRate + 0.00001);
                        if (previous < 0.0f && value >= 0.0f) {
                            if (lastCrossing >= 0) {
                                EXPECT_NEAR(index - lastCrossing, sampleRate / frequency,
                                            sampleRate / frequency * 0.001 + 2.0);
                                ++periods;
                            }
                            lastCrossing = index;
                        }
                    }
                    previous = value;
                }
            }
            EXPECT_GE(periods, 2);
            EXPECT_NEAR(minimum, -1.0f, 0.001f);
            EXPECT_NEAR(maximum, 1.0f, 0.001f);
        }
    }
}
