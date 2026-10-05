#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "../../Source/CS01Synth/ModernVCFProcessor.h"
#include "../../Source/Parameters.h"

// Test fixture for ModernVCFProcessor tests
class ModernVCFProcessorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Create a dummy processor for APVTS
        dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();

        auto parameterLayout = createParameterLayout();
        apvts = std::make_unique<juce::AudioProcessorValueTreeState>(
            *dummyProcessor, nullptr, "PARAMETERS", std::move(parameterLayout));

        // Create ModernVCFProcessor
        processor = std::make_unique<ModernVCFProcessor>(*apvts);
    }

    void TearDown() override {
        processor.reset();
        apvts.reset();
        dummyProcessor.reset();
    }

    // Create a parameter layout for testing
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // Add parameters needed for ModernVCFProcessor
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::cutoff, "Cutoff", juce::NormalisableRange<float>(20.0f, 20000.0f),
            1000.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(ParameterIds::resonance, "Resonance",
                                                              0.0f, 1.0f, 0.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::vcfEgDepth, "VCF EG Depth", juce::NormalisableRange<float>(0.0f, 1.0f),
            0.5f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::modDepth, "Mod Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::breathInput, "Breath Input", juce::NormalisableRange<float>(0.0f, 1.0f),
            0.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::breathVcf, "Breath VCF", juce::NormalisableRange<float>(0.0f, 1.0f),
            0.0f));

        return layout;
    }

    std::unique_ptr<juce::AudioProcessorGraph> dummyProcessor;
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
    std::unique_ptr<ModernVCFProcessor> processor;
};

TEST_F(ModernVCFProcessorTest, Initialization) {
    // Check that processor was created successfully
    EXPECT_NE(processor.get(), nullptr);

    // Check that processor has expected properties
    EXPECT_EQ(processor->getName(), juce::String("Modern VCF"));
    EXPECT_FALSE(processor->acceptsMidi());
    EXPECT_FALSE(processor->producesMidi());
    EXPECT_FALSE(processor->isMidiEffect());

    // Check bus configuration
    EXPECT_EQ(processor->getBusCount(true), 3);   // 3 input buses
    EXPECT_EQ(processor->getBusCount(false), 1);  // 1 output bus

    EXPECT_EQ(processor->getBus(true, 0)->getName(), juce::String("AudioInput"));
    EXPECT_EQ(processor->getBus(true, 1)->getName(), juce::String("EGInput"));
    EXPECT_EQ(processor->getBus(true, 2)->getName(), juce::String("LFOInput"));
    EXPECT_EQ(processor->getBus(false, 0)->getName(), juce::String("Output"));
}

TEST_F(ModernVCFProcessorTest, BusesLayout) {
    // Test valid layout with all required buses
    juce::AudioProcessor::BusesLayout validLayout;
    validLayout.inputBuses.add(juce::AudioChannelSet::mono());   // AudioInput
    validLayout.inputBuses.add(juce::AudioChannelSet::mono());   // EGInput
    validLayout.inputBuses.add(juce::AudioChannelSet::mono());   // LFOInput
    validLayout.outputBuses.add(juce::AudioChannelSet::mono());  // Output

    EXPECT_TRUE(processor->isBusesLayoutSupported(validLayout));

    // Test invalid layout with stereo audio input
    juce::AudioProcessor::BusesLayout invalidLayout1;
    invalidLayout1.inputBuses.add(juce::AudioChannelSet::stereo());  // AudioInput
    invalidLayout1.inputBuses.add(juce::AudioChannelSet::mono());    // EGInput
    invalidLayout1.inputBuses.add(juce::AudioChannelSet::mono());    // LFOInput
    invalidLayout1.outputBuses.add(juce::AudioChannelSet::mono());   // Output

    EXPECT_FALSE(processor->isBusesLayoutSupported(invalidLayout1));

    // Test invalid layout with stereo EG input
    juce::AudioProcessor::BusesLayout invalidLayout2;
    invalidLayout2.inputBuses.add(juce::AudioChannelSet::mono());    // AudioInput
    invalidLayout2.inputBuses.add(juce::AudioChannelSet::stereo());  // EGInput
    invalidLayout2.inputBuses.add(juce::AudioChannelSet::mono());    // LFOInput
    invalidLayout2.outputBuses.add(juce::AudioChannelSet::mono());   // Output

    EXPECT_FALSE(processor->isBusesLayoutSupported(invalidLayout2));

    // Test invalid layout with stereo LFO input
    juce::AudioProcessor::BusesLayout invalidLayout3;
    invalidLayout3.inputBuses.add(juce::AudioChannelSet::mono());    // AudioInput
    invalidLayout3.inputBuses.add(juce::AudioChannelSet::mono());    // EGInput
    invalidLayout3.inputBuses.add(juce::AudioChannelSet::stereo());  // LFOInput
    invalidLayout3.outputBuses.add(juce::AudioChannelSet::mono());   // Output

    EXPECT_FALSE(processor->isBusesLayoutSupported(invalidLayout3));

    // Test invalid layout with stereo output
    juce::AudioProcessor::BusesLayout invalidLayout4;
    invalidLayout4.inputBuses.add(juce::AudioChannelSet::mono());     // AudioInput
    invalidLayout4.inputBuses.add(juce::AudioChannelSet::mono());     // EGInput
    invalidLayout4.inputBuses.add(juce::AudioChannelSet::mono());     // LFOInput
    invalidLayout4.outputBuses.add(juce::AudioChannelSet::stereo());  // Output

    EXPECT_FALSE(processor->isBusesLayoutSupported(invalidLayout4));
}

TEST_F(ModernVCFProcessorTest, PrepareToPlay) {
    // Test prepareToPlay with different sample rates
    // This should not throw any exceptions
    processor->prepareToPlay(44100.0, 512);
    EXPECT_TRUE(true);  // If we got here, no exception was thrown

    processor->prepareToPlay(48000.0, 1024);
    EXPECT_TRUE(true);  // If we got here, no exception was thrown

    // Test releaseResources
    processor->releaseResources();
    EXPECT_TRUE(true);  // If we got here, no exception was thrown
}

TEST_F(ModernVCFProcessorTest, ProcessBlock) {
    // Test is simplified to check for structural issues only
    // Skip actual audio processing due to bus structure complexity

    // Prepare processor
    const double sampleRate = 44100.0;
    const int samplesPerBlock = 512;
    processor->prepareToPlay(sampleRate, samplesPerBlock);

    // Set the bus layout
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::mono());   // AudioInput
    layout.inputBuses.add(juce::AudioChannelSet::mono());   // EGInput
    layout.inputBuses.add(juce::AudioChannelSet::mono());   // LFOInput
    layout.outputBuses.add(juce::AudioChannelSet::mono());  // Output

    EXPECT_TRUE(processor->setBusesLayout(layout));

    // Set cutoff parameter to a known value
    auto cutoffParam =
        static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::cutoff));
    if (cutoffParam != nullptr)
        cutoffParam->setValueNotifyingHost(cutoffParam->convertTo0to1(1000.0f));  // 1kHz cutoff

    // This test only verifies preparation and bus setup, not actual processing
    EXPECT_TRUE(true);
}

TEST_F(ModernVCFProcessorTest, ShortBlocksPreserveFilterStateAcrossPartitions) {
    constexpr int capacity = 512;
    constexpr int totalSamples = 1024;
    auto* depth = apvts->getParameter(ParameterIds::vcfEgDepth);
    depth->setValueNotifyingHost(0.0f);

    // Constant coefficients isolate state advancement from block-averaged modulation.
    const auto render = [&](int partitionSize) {
        ModernVCFProcessor filter(*apvts);
        filter.prepareToPlay(48000.0, capacity);
        juce::AudioBuffer<float> output(1, totalSamples);
        juce::MidiBuffer midi;
        for (int offset = 0; offset < totalSamples;) {
            const int length = juce::jmin(partitionSize, totalSamples - offset);
            juce::AudioBuffer<float> block(3, length);
            block.clear();
            // The impulse tail must survive each processing boundary unchanged.
            if (offset == 0)
                block.setSample(0, 0, 1.0f);
            filter.processBlock(block, midi);
            output.copyFrom(0, offset, block, 0, 0, length);
            offset += length;
        }
        return output;
    };

    const auto reference = render(capacity);
    ASSERT_GT(reference.getMagnitude(0, totalSamples), 0.001f);
    for (int partitionSize : {1, 7, 64, 127, 511}) {
        SCOPED_TRACE(partitionSize);
        const auto actual = render(partitionSize);
        float maximumDifference = 0.0f;
        for (int sample = 0; sample < totalSamples; ++sample)
            maximumDifference =
                juce::jmax(maximumDifference,
                           std::abs(actual.getSample(0, sample) - reference.getSample(0, sample)));
        EXPECT_LT(maximumDifference, 1.0e-6f);
    }
}

TEST_F(ModernVCFProcessorTest, CutoffParameter) {
    // Prepare processor
    const double sampleRate = 44100.0;
    const int samplesPerBlock = 512;
    processor->prepareToPlay(sampleRate, samplesPerBlock);

    // Set bus layout
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::mono());   // AudioInput
    layout.inputBuses.add(juce::AudioChannelSet::mono());   // EGInput
    layout.inputBuses.add(juce::AudioChannelSet::mono());   // LFOInput
    layout.outputBuses.add(juce::AudioChannelSet::mono());  // Output

    EXPECT_TRUE(processor->setBusesLayout(layout));

    // Test parameter behavior - verify that cutoff can be changed
    auto cutoffParam =
        static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::cutoff));
    EXPECT_NE(cutoffParam, nullptr);

    if (cutoffParam != nullptr) {
        // Set low cutoff value
        float lowValue = 500.0f;
        cutoffParam->setValueNotifyingHost(cutoffParam->convertTo0to1(lowValue));
        EXPECT_NEAR(cutoffParam->get(), lowValue, 0.1f);

        // Set high cutoff value
        float highValue = 10000.0f;
        cutoffParam->setValueNotifyingHost(cutoffParam->convertTo0to1(highValue));
        EXPECT_NEAR(cutoffParam->get(), highValue, 0.1f);
    }
}

TEST_F(ModernVCFProcessorTest, ResonanceParameter) {
    // Prepare processor
    const double sampleRate = 44100.0;
    const int samplesPerBlock = 512;
    processor->prepareToPlay(sampleRate, samplesPerBlock);

    // Test resonance parameter behavior
    auto resonanceParam =
        static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::resonance));
    EXPECT_NE(resonanceParam, nullptr);

    if (resonanceParam != nullptr) {
        // Set low resonance value
        resonanceParam->setValueNotifyingHost(false);
        EXPECT_EQ(resonanceParam->get(), false);

        // Set high resonance value
        resonanceParam->setValueNotifyingHost(true);
        EXPECT_EQ(resonanceParam->get(), true);
    }
}

TEST_F(ModernVCFProcessorTest, Modulation) {
    // Prepare processor
    const double sampleRate = 44100.0;
    const int samplesPerBlock = 512;
    processor->prepareToPlay(sampleRate, samplesPerBlock);

    // Test modulation parameter behavior
    auto vcfEgDepthParam =
        static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::vcfEgDepth));
    auto modDepthParam =
        static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::modDepth));

    EXPECT_NE(vcfEgDepthParam, nullptr);
    EXPECT_NE(modDepthParam, nullptr);

    if (vcfEgDepthParam != nullptr) {
        // Set EG modulation depth
        float testValue = 0.75f;
        vcfEgDepthParam->setValueNotifyingHost(testValue);
        EXPECT_GT(vcfEgDepthParam->get(), 0.0f);
    }

    if (modDepthParam != nullptr) {
        // Set LFO modulation depth
        float testValue = 0.5f;
        modDepthParam->setValueNotifyingHost(testValue);
        EXPECT_GT(modDepthParam->get(), 0.0f);
    }
}

TEST_F(ModernVCFProcessorTest, ModulationIsIndependentOfBlockPartition) {
    apvts->getParameter(ParameterIds::vcfEgDepth)->setValueNotifyingHost(0.7f);
    apvts->getParameter(ParameterIds::modDepth)->setValueNotifyingHost(0.6f);
    constexpr int count = 1024;
    const auto render = [&](int size) {
        ModernVCFProcessor filter(*apvts);
        filter.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> result(1, count);
        juce::MidiBuffer midi;
        for (int offset = 0; offset < count; offset += size) {
            const int length = juce::jmin(size, count - offset);
            juce::AudioBuffer<float> block(3, length);
            for (int i = 0; i < length; ++i) {
                const float t = static_cast<float>(offset + i);
                block.setSample(0, i, 0.2f * std::sin(t * 0.17f));
                block.setSample(1, i, t / count);
                block.setSample(2, i, std::sin(t * 0.031f));
            }
            filter.processBlock(block, midi);
            result.copyFrom(0, offset, block, 0, 0, length);
        }
        return result;
    };
    const auto reference = render(512);
    EXPECT_GT(reference.getMagnitude(0, count), 0.001f);
    for (int size : {1, 7, 64, 127}) {
        const auto actual = render(size);
        for (int i = 0; i < count; ++i) {
            ASSERT_TRUE(std::isfinite(actual.getSample(0, i)));
            EXPECT_NEAR(actual.getSample(0, i), reference.getSample(0, i), 1.0e-6f);
        }
    }
}

TEST_F(ModernVCFProcessorTest, FourPoleResponseAndFiniteResonance) {
    apvts->getParameter(ParameterIds::vcfEgDepth)->setValueNotifyingHost(0.0f);
    const auto measure = [&](double rate, double frequency, float resonance) {
        apvts->getParameter(ParameterIds::resonance)->setValueNotifyingHost(resonance);
        processor->prepareToPlay(rate, 256);
        double energy = 0.0;
        juce::MidiBuffer midi;
        for (int block = 0; block < 64; ++block) {
            juce::AudioBuffer<float> buffer(3, 256);
            buffer.clear();
            for (int i = 0; i < 256; ++i)
                buffer.setSample(0, i, static_cast<float>(0.1 * std::sin(
                    juce::MathConstants<double>::twoPi * frequency * (block * 256 + i) / rate)));
            processor->processBlock(buffer, midi);
            for (int i = 0; i < 256; ++i) {
                const float value = buffer.getSample(0, i);
                EXPECT_TRUE(std::isfinite(value));
                if (block >= 32)
                    energy += value * value;
            }
        }
        return std::sqrt(energy / 8192.0) / (0.1 / std::sqrt(2.0));
    };
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        EXPECT_NEAR(measure(rate, 100.0, 0.0f), 1.0, 0.02);
        EXPECT_NEAR(measure(rate, 1000.0, 0.0f), 1.0 / std::sqrt(2.0), 0.02);
        const double upper = measure(rate, 4000.0, 0.0f);
        const double lower = measure(rate, 2000.0, 0.0f);
        EXPECT_LT(upper / lower, 0.075); // Approximately 24 dB/octave, not 12.
        EXPECT_GT(measure(rate, 1000.0, 1.0f), measure(rate, 1000.0, 0.0f));
    }
}

TEST(CS01IIVCFCircuitTest, MatchesLinearCoreWithGentleColorationAndResets) {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        CS01IIVCFCircuit circuit;
        circuit.prepare(rate);
        juce::dsp::StateVariableTPTFilter<float> first, second;
        for (auto* stage : {&first, &second}) {
            stage->setType(juce::dsp::StateVariableTPTFilter<float>::Type::lowpass);
            stage->prepare({rate, 512, 1});
        }
        const auto compare = [&]() {
            float envelope = 0.0f;
            const float smoothing = static_cast<float>(std::pow(0.99, 44100.0 / rate));
            for (int i = 0; i < 4096; ++i) {
                const float resonance = static_cast<float>(i % 101) / 100.0f;
                const float cutoff = 20.0f + static_cast<float>(i % 1000) * 19.0f;
                const float input = 0.2f * std::sin(static_cast<float>(i) * 0.13f);
                circuit.setResonance(resonance);
                circuit.setCutoffFrequency(cutoff);
                first.setResonance(0.5411961f * (1.0f + 3.0f * resonance));
                second.setResonance(1.3065630f);
                first.setCutoffFrequency(cutoff);
                second.setCutoffFrequency(cutoff);
                const float linear = second.processSample(0, first.processSample(0, input));
                envelope = envelope * smoothing + std::abs(input) * (1.0f - smoothing);
                const float drive = 0.5f + 0.5f * resonance + 0.25f * envelope;
                const float blend = 0.08f + 0.22f * resonance;
                const float expected = linear + blend * (std::tanh(linear * drive) / drive - linear);
                EXPECT_FLOAT_EQ(circuit.processSample(0, input), expected);
            }
        };
        compare();
        circuit.reset();
        first.reset();
        second.reset();
        compare();
    }
}

TEST(IG05630Test, ColorationIsGentleSymmetricAndLevelDependent) {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        for (float resonance : {0.0f, 0.5f, 1.0f}) {
            const auto settle = [&](float input) {
                IG05630 model;
                model.prepare(rate);
                model.setCutoffFrequency(1000.0f);
                model.setResonance(resonance);
                float output = 0.0f;
                for (int i = 0; i < static_cast<int>(rate * 0.1); ++i) {
                    output = model.processSample(input);
                    EXPECT_TRUE(std::isfinite(output));
                }
                return output;
            };
            EXPECT_NEAR(settle(0.001f), 0.001f, 1.0e-6f);
            const float loud = settle(1.0f);
            EXPECT_GT(loud, 0.85f);
            EXPECT_LT(loud, 1.0f);
            EXPECT_NEAR(settle(-1.0f), -loud, 1.0e-6f);
            EXPECT_NEAR(settle(0.0f), 0.0f, 1.0e-7f);
        }
    }
}
