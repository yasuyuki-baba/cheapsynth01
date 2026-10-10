#include <JuceHeader.h>

#include "CS01Synth/VCAProcessor.h"
#include "Parameters.h"

#include <gtest/gtest.h>

#include <complex>

namespace {
// Match the characterization window and sample generation exactly.
template <typename ProcessSample>
double measureReferenceGain(double sampleRate, double frequency, ProcessSample process) {
    double power = 0.0;
    int measured = 0;
    const int count = static_cast<int>(sampleRate * 2.0);
    for (int offset = 0; offset < count; offset += 256) {
        for (int i = 0; i < 256; ++i) {
            const float input =
                static_cast<float>(0.01 * std::sin(juce::MathConstants<double>::twoPi * frequency *
                                                   (offset + i) / sampleRate));
            const float output = process(input);
            if (!std::isfinite(output))
                return std::numeric_limits<double>::quiet_NaN();
            if (offset + i >= sampleRate && offset + i < count) {
                power += output * output;
                ++measured;
            }
        }
    }
    return std::sqrt(power / measured) / (0.01 / std::sqrt(2.0));
}
}  // namespace

TEST(ResponseMeasurementTest, KnownGainAndAnalyticalFilter) {
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        for (double frequency : {20.0, 40.0, 80.0, 200.0, 1000.0}) {
            SCOPED_TRACE(sampleRate);
            SCOPED_TRACE(frequency);
            EXPECT_NEAR(
                measureReferenceGain(sampleRate, frequency, [](float input) { return input; }), 1.0,
                1.0e-6);
            const double halfGain = measureReferenceGain(sampleRate, frequency,
                                                         [](float input) { return input * 0.5f; });
            EXPECT_NEAR(20.0 * std::log10(halfGain), -6.020599913, 1.0e-5);

            // A separately specified first-order high-pass recurrence:
            // y[n] = a * (y[n-1] + x[n] - x[n-1]).
            const double a = std::exp(-juce::MathConstants<double>::twoPi * 40.0 / sampleRate);
            double previousInput = 0.0, previousOutput = 0.0;
            const double measured = measureReferenceGain(sampleRate, frequency, [&](float input) {
                previousOutput = a * (previousOutput + input - previousInput);
                previousInput = input;
                return static_cast<float>(previousOutput);
            });
            // Independent steady-state result from H(z)=a(1-z^-1)/(1-a*z^-1).
            const auto delay =
                std::polar(1.0, -juce::MathConstants<double>::twoPi * frequency / sampleRate);
            const double expected = std::abs(a * (1.0 - delay) / (1.0 - a * delay));
            EXPECT_NEAR(measured, expected, 1.0e-6);
        }
    }
}

TEST(ResponseMeasurementTest, HighPassCoefficientPrecision) {
    // Separate coefficient quantization from time-domain float arithmetic.
    const double sampleRate = 96000.0, frequency = 40.0;
    const auto z = std::polar(1.0, -juce::MathConstants<double>::twoPi * frequency / sampleRate);
    const auto response = [&](const auto& coefficients) {
        const auto* c = coefficients->getRawCoefficients();
        return std::abs((static_cast<double>(c[0]) + static_cast<double>(c[1]) * z +
                         static_cast<double>(c[2]) * z * z) /
                        (1.0 + static_cast<double>(c[3]) * z + static_cast<double>(c[4]) * z * z));
    };
    double cascadeError = 0.0;
    for (double cutoff : {20.0, 40.0}) {
        const double ratio = std::tan(juce::MathConstants<double>::pi * frequency / sampleRate) /
                             std::tan(juce::MathConstants<double>::pi * cutoff / sampleRate);
        const double ideal = ratio * ratio / std::sqrt(1.0 + std::pow(ratio, 4.0));
        const auto floatCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(
            sampleRate, static_cast<float>(cutoff));
        const auto doubleCoefficients =
            juce::dsp::IIR::Coefficients<double>::makeHighPass(sampleRate, cutoff);
        EXPECT_NEAR(response(doubleCoefficients), ideal, 1.0e-9);
        juce::dsp::IIR::Filter<float> filter;
        filter.coefficients = floatCoefficients;
        const double measured = measureReferenceGain(
            sampleRate, frequency, [&](float input) { return filter.processSample(input); });
        const double coefficientError = 20.0 * std::log10(response(floatCoefficients) / ideal);
        cascadeError += coefficientError;
        std::cout << "HP precision: cutoff=" << cutoff
                  << " Hz, coefficient error=" << coefficientError << " dB, arithmetic residual="
                  << 20.0 * std::log10(measured / response(floatCoefficients)) << " dB\n";
        EXPECT_NEAR(20.0 * std::log10(measured / response(floatCoefficients)), 0.0, 0.01);
    }
    std::cout << "HP cascade coefficient error: " << cascadeError << " dB\n";
}

// Test fixture for VCAProcessor tests
class VCAProcessorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Create a dummy processor for APVTS
        dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();

        auto parameterLayout = createParameterLayout();
        apvts = std::make_unique<juce::AudioProcessorValueTreeState>(
            *dummyProcessor, nullptr, "PARAMETERS", std::move(parameterLayout));

        // Create VCAProcessor
        processor = std::make_unique<VCAProcessor>(*apvts);
    }

    void TearDown() override {
        processor.reset();
        apvts.reset();
        dummyProcessor.reset();
    }

    // Create a parameter layout for testing
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // Add parameters needed for VCAProcessor
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::vcaEgDepth, "VCA EG Depth", juce::NormalisableRange<float>(0.0f, 1.0f),
            0.5f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::breathInput, "Breath Input", juce::NormalisableRange<float>(0.0f, 1.0f),
            0.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::breathVca, "Breath VCA", juce::NormalisableRange<float>(0.0f, 1.0f),
            0.0f));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParameterIds::volume, "Volume", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));

        return layout;
    }

    std::unique_ptr<juce::AudioProcessorGraph> dummyProcessor;
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
    std::unique_ptr<VCAProcessor> processor;
};

TEST_F(VCAProcessorTest, LiveEgDepthIsPartitionIndependent) {
    std::vector<float> reference;
    for (int blockSize : {1, 7, 64}) {
        apvts->getParameter(ParameterIds::vcaEgDepth)->setValueNotifyingHost(0.0f);
        processor->prepareToPlay(48000.0, 64);
        apvts->getParameter(ParameterIds::vcaEgDepth)->setValueNotifyingHost(1.0f);
        std::vector<float> output;
        juce::MidiBuffer midi;
        for (int offset = 0; offset < 960;) {
            const int count = std::min(blockSize, 960 - offset);
            juce::AudioBuffer<float> buffer(2, count);
            for (int i = 0; i < count; ++i) {
                buffer.setSample(0, i, 0.1f);
                buffer.setSample(1, i, 0.0f);
            }
            processor->processBlock(buffer, midi);
            for (int i = 0; i < count; ++i) {
                ASSERT_TRUE(std::isfinite(buffer.getSample(0, i)));
                output.push_back(buffer.getSample(0, i));
            }
            offset += count;
        }
        if (reference.empty())
            reference = output;
        else
            for (size_t i = 0; i < output.size(); ++i)
                ASSERT_NEAR(output[i], reference[i], 1.0e-7f) << i;
    }
}

TEST_F(VCAProcessorTest, FrequencyResponseCharacterization) {
    std::array<double, 5> referenceGains{};
    // Characterize the current implementation, not a calibrated hardware target.
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        for (double frequency : {20.0, 40.0, 80.0, 200.0, 1000.0}) {
            processor = std::make_unique<VCAProcessor>(*apvts);
            apvts->getParameter(ParameterIds::volume)->setValueNotifyingHost(1.0f);
            apvts->getParameter(ParameterIds::vcaEgDepth)->setValueNotifyingHost(1.0f);
            processor->prepareToPlay(sampleRate, 256);
            juce::AudioBuffer<float> buffer(2, 256);
            ASSERT_EQ(processor->getTotalNumInputChannels(), 2);
            ASSERT_EQ(processor->getTotalNumOutputChannels(), 1);
            ASSERT_EQ(processor->getBusBuffer(buffer, true, 0).getWritePointer(0),
                      buffer.getWritePointer(0));
            ASSERT_EQ(processor->getBusBuffer(buffer, true, 1).getWritePointer(0),
                      buffer.getWritePointer(1));
            juce::MidiBuffer midi;
            double power = 0.0, latePower = 0.0;
            double sineProjection = 0.0, cosineProjection = 0.0;
            int measured = 0, lateMeasured = 0;
            const int count = static_cast<int>(sampleRate * 5.0);
            for (int offset = 0; offset < count; offset += 256) {
                for (int i = 0; i < 256; ++i) {
                    buffer.setSample(
                        0, i,
                        static_cast<float>(0.01 * std::sin(juce::MathConstants<double>::twoPi *
                                                           frequency * (offset + i) / sampleRate)));
                    buffer.setSample(1, i, 1.0f);
                }
                processor->processBlock(buffer, midi);
                for (int i = 0; i < 256; ++i) {
                    const float value = buffer.getSample(0, i);
                    ASSERT_TRUE(std::isfinite(value));
                    if (offset + i >= sampleRate && offset + i < sampleRate * 2.0) {
                        power += value * value;
                        ++measured;
                    }
                    if (offset + i >= sampleRate * 4.0 && offset + i < count) {
                        latePower += static_cast<double>(value) * value;
                        const double phase = juce::MathConstants<double>::twoPi * frequency *
                                             (offset + i) / sampleRate;
                        sineProjection += value * std::sin(phase);
                        cosineProjection += value * std::cos(phase);
                        ++lateMeasured;
                    }
                }
            }
            ASSERT_GT(measured, 0);
            const double gain = std::sqrt(power / measured) / (0.01 / std::sqrt(2.0));
            ASSERT_GT(gain, 0.0);
            ASSERT_EQ(measured, static_cast<int>(sampleRate));
            ASSERT_EQ(lateMeasured, measured);
            const double lateGain = std::sqrt(latePower / lateMeasured) / (0.01 / std::sqrt(2.0));
            const int frequencyIndex = frequency == 20.0    ? 0
                                       : frequency == 40.0  ? 1
                                       : frequency == 80.0  ? 2
                                       : frequency == 200.0 ? 3
                                                            : 4;
            if (sampleRate == 44100.0)
                referenceGains[frequencyIndex] = lateGain;
            else
                EXPECT_NEAR(20.0 * std::log10(lateGain / referenceGains[frequencyIndex]), 0.0, 0.1);
            // A convergence check, not a hardware response target.
            EXPECT_NEAR(20.0 * std::log10(gain / lateGain), 0.0, 0.01);

            // Independently derive the fundamental response: bilinear-transform
            // Butterworth stages, two fixed-pole HP stages and the difference boost.
            const auto z =
                std::polar(1.0, -juce::MathConstants<double>::twoPi * frequency / sampleRate);
            const auto s = 2.0 * sampleRate * (1.0 - z) / (1.0 + z);
            const auto butterworth = [&](double cutoff, bool highPass) {
                const double w = 2.0 * sampleRate *
                                 std::tan(juce::MathConstants<double>::pi * cutoff / sampleRate);
                const auto denominator = s * s + std::sqrt(2.0) * w * s + w * w;
                return (highPass ? s * s : std::complex<double>(w * w, 0.0)) / denominator;
            };
            const auto fixedHighPass = [&](double pole) {
                return pole * (1.0 - z) / (1.0 - pole * z);
            };
            const double expectedFundamental = std::abs(
                butterworth(40.0, true) * butterworth(20.0, true) *
                fixedHighPass(std::pow(static_cast<double>(0.997f), 44100.0 / sampleRate)) * 0.935 *
                (1.0 + 0.004 * (1.0 - z)) *
                fixedHighPass(std::pow(static_cast<double>(0.9995f), 44100.0 / sampleRate)) *
                butterworth(15000.0, false));
            // The 0.95/0.92 piecewise gain has fundamental slope 0.935;
            // its even harmonics are deliberately excluded by projection.
            const double fundamental =
                2.0 * std::hypot(sineProjection, cosineProjection) / lateMeasured / 0.01;
            EXPECT_NEAR(20.0 * std::log10(fundamental / expectedFundamental), 0.0, 0.05);
            std::cout << "VCA response: fs=" << sampleRate << " Hz, f=" << frequency
                      << " Hz, gain=" << 20.0 * std::log10(gain)
                      << " dB, convergence=" << 20.0 * std::log10(gain / lateGain)
                      << " dB, analytical error="
                      << 20.0 * std::log10(fundamental / expectedFundamental) << " dB\n";
        }
    }
}

TEST_F(VCAProcessorTest, Initialization) {
    // Check that processor was created successfully
    EXPECT_NE(processor.get(), nullptr);

    // Check that processor has expected properties
    EXPECT_EQ(processor->getName(), juce::String("VCA"));
    EXPECT_FALSE(processor->acceptsMidi());
    EXPECT_FALSE(processor->producesMidi());
}

TEST_F(VCAProcessorTest, BusesLayout) {
    // Test valid layout
    juce::AudioProcessor::BusesLayout validLayout;
    validLayout.inputBuses.add(juce::AudioChannelSet::mono());   // Audio input
    validLayout.inputBuses.add(juce::AudioChannelSet::mono());   // EG input
    validLayout.outputBuses.add(juce::AudioChannelSet::mono());  // Output

    EXPECT_TRUE(processor->isBusesLayoutSupported(validLayout));

    // Test invalid layout with stereo inputs
    juce::AudioProcessor::BusesLayout invalidLayout1;
    invalidLayout1.inputBuses.add(juce::AudioChannelSet::stereo());  // Audio input
    invalidLayout1.inputBuses.add(juce::AudioChannelSet::mono());    // EG input
    invalidLayout1.outputBuses.add(juce::AudioChannelSet::mono());   // Output

    EXPECT_FALSE(processor->isBusesLayoutSupported(invalidLayout1));

    // Test invalid layout with stereo output
    juce::AudioProcessor::BusesLayout invalidLayout2;
    invalidLayout2.inputBuses.add(juce::AudioChannelSet::mono());     // Audio input
    invalidLayout2.inputBuses.add(juce::AudioChannelSet::mono());     // EG input
    invalidLayout2.outputBuses.add(juce::AudioChannelSet::stereo());  // Output

    EXPECT_FALSE(processor->isBusesLayoutSupported(invalidLayout2));
}

TEST_F(VCAProcessorTest, PrepareToPlay) {
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

TEST_F(VCAProcessorTest, BreathControlEndpoints) {
    // Endpoint and monotonicity checks, not calibration of the hardware gain law.
    const auto measure = [&](float eg, float depth, float breath, float egDepth = 1.0f) {
        VCAProcessor vca(*apvts);
        apvts->getParameter(ParameterIds::volume)->setValueNotifyingHost(1.0f);
        apvts->getParameter(ParameterIds::vcaEgDepth)->setValueNotifyingHost(egDepth);
        apvts->getParameter(ParameterIds::breathVca)->setValueNotifyingHost(depth);
        apvts->getParameter(ParameterIds::breathInput)->setValueNotifyingHost(breath);
        vca.prepareToPlay(48000.0, 256);
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;
        double power = 0.0;
        for (int offset = 0; offset < 96000; offset += 256) {
            for (int i = 0; i < 256; ++i) {
                buffer.setSample(
                    0, i,
                    static_cast<float>(0.001 * std::sin(juce::MathConstants<double>::twoPi *
                                                        1000.0 * (offset + i) / 48000.0)));
                buffer.setSample(1, i, eg);
            }
            vca.processBlock(buffer, midi);
            for (int i = 0; i < 256; ++i)
                if (offset + i >= 48000 && offset + i < 96000) {
                    const double value = buffer.getSample(0, i);
                    power += value * value;
                }
        }
        return std::sqrt(power / 48000.0);
    };
    const double bypass = measure(1.0f, 0.0f, 0.0f);
    ASSERT_GT(bypass, 0.0);
    EXPECT_NEAR(measure(1.0f, 0.0f, 1.0f), bypass, 1.0e-10);
    EXPECT_EQ(measure(1.0f, 1.0f, 0.0f), 0.0);
    EXPECT_NEAR(measure(1.0f, 1.0f, 1.0f), bypass, 1.0e-10);
    EXPECT_EQ(measure(0.0f, 1.0f, 1.0f), 0.0);
    double previous = 0.0;
    for (float breath : {0.25f, 0.5f, 0.75f, 1.0f}) {
        const double current = measure(1.0f, 1.0f, breath);
        EXPECT_GT(current, previous);
        previous = current;
    }
    // Independent controls: disabling EG depth must remove dependence on EG.
    // These are model invariants, not proof of the chip's gain law.
    for (float breath : {0.25f, 0.5f, 1.0f}) {
        SCOPED_TRACE(breath);
        const double egBypass = measure(0.0f, 1.0f, breath, 0.0f);
        ASSERT_GT(egBypass, 0.0);
        EXPECT_NEAR(measure(1.0f, 1.0f, breath, 0.0f), egBypass, 1.0e-10);
        double previousEg = 0.0;
        for (float eg : {0.25f, 0.5f, 0.75f, 1.0f}) {
            const double current = measure(eg, 1.0f, breath);
            EXPECT_GT(current, previousEg);
            previousEg = current;
        }
    }
}

TEST_F(VCAProcessorTest, ProcessBlock) {
    // Simple test due to buffer handling complexity

    // Check that parameters are correctly set
    auto vcaEgDepthParam =
        static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::vcaEgDepth));
    auto volumeParam =
        static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::volume));

    EXPECT_NE(vcaEgDepthParam, nullptr);
    EXPECT_NE(volumeParam, nullptr);

    if (vcaEgDepthParam != nullptr) {
        vcaEgDepthParam->setValueNotifyingHost(0.5f);  // 50% EG depth
        EXPECT_GT(vcaEgDepthParam->get(), 0.0f);
    }

    if (volumeParam != nullptr) {
        volumeParam->setValueNotifyingHost(0.7f);  // 70% volume
        EXPECT_GT(volumeParam->get(), 0.0f);
    }

    // Prepare processor
    const double sampleRate = 44100.0;
    const int samplesPerBlock = 512;
    processor->prepareToPlay(sampleRate, samplesPerBlock);

    // Check that bus configuration can be set correctly
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::mono());   // Audio input
    layout.inputBuses.add(juce::AudioChannelSet::mono());   // EG input
    layout.outputBuses.add(juce::AudioChannelSet::mono());  // Output

    EXPECT_TRUE(processor->setBusesLayout(layout));

    // For this test, we only perform preparation and parameter tests, not actual audio processing
    EXPECT_TRUE(true);
}

TEST_F(VCAProcessorTest, VCAFunctionality) {
    // Prepare processor
    const double sampleRate = 44100.0;
    const int samplesPerBlock = 512;
    processor->prepareToPlay(sampleRate, samplesPerBlock);

    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::mono());
    layout.inputBuses.add(juce::AudioChannelSet::mono());
    layout.outputBuses.add(juce::AudioChannelSet::mono());

    // Create audio buffer
    juce::AudioBuffer<float> buffer(3, samplesPerBlock);  // Audio in, EG in, Output
    juce::MidiBuffer midiBuffer;

    // Fill buffers
    auto* audioInputData = buffer.getWritePointer(0);
    auto* egInputData = buffer.getWritePointer(1);

    // Generate constant audio input (makes comparison easier)
    for (int i = 0; i < samplesPerBlock; ++i) {
        audioInputData[i] = 0.5f;  // Constant value
    }

    // Test 1: Zero EG value should result in much lower output
    // Set EG depth to 1.0 (full EG control)
    auto vcaEgDepthParam =
        static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::vcaEgDepth));
    auto volumeParam =
        static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::volume));

    if (vcaEgDepthParam != nullptr)
        vcaEgDepthParam->setValueNotifyingHost(1.0f);

    if (volumeParam != nullptr)
        volumeParam->setValueNotifyingHost(1.0f);

    // Set EG to 0
    // Endpoint comparison, not a live-depth transition: initialize controls
    // and filter state identically for both measurements.
    processor->prepareToPlay(sampleRate, samplesPerBlock);
    for (int i = 0; i < samplesPerBlock; ++i) {
        egInputData[i] = 0.0f;
    }

    // Process
    juce::AudioBuffer<float> processBuffer1(3, samplesPerBlock);
    processBuffer1.copyFrom(0, 0, audioInputData, samplesPerBlock);
    processBuffer1.copyFrom(1, 0, egInputData, samplesPerBlock);

    processor->processBlock(processBuffer1, midiBuffer);

    // Record zero EG output
    float zeroEGSum = 0.0f;
    auto* outputData1 = processBuffer1.getReadPointer(0);
    for (int i = 0; i < processBuffer1.getNumSamples(); ++i) {
        zeroEGSum += std::abs(outputData1[i]);
    }

    // Test 2: Full EG value should result in higher output
    processor->prepareToPlay(sampleRate, samplesPerBlock);
    // Set EG to 1.0
    for (int i = 0; i < samplesPerBlock; ++i) {
        egInputData[i] = 1.0f;
    }

    // Process
    juce::AudioBuffer<float> processBuffer2(3, samplesPerBlock);
    processBuffer2.copyFrom(0, 0, audioInputData, samplesPerBlock);
    processBuffer2.copyFrom(1, 0, egInputData, samplesPerBlock);

    processor->processBlock(processBuffer2, midiBuffer);

    // Record full EG output
    float fullEGSum = 0.0f;
    auto* outputData2 = processBuffer2.getReadPointer(0);
    for (int i = 0; i < processBuffer2.getNumSamples(); ++i) {
        fullEGSum += std::abs(outputData2[i]);
    }

    // Full EG should produce more output than zero EG
    EXPECT_GT(fullEGSum, zeroEGSum);
}

TEST_F(VCAProcessorTest, OutputFiltering) {
    // Prepare processor
    const double sampleRate = 44100.0;
    const int samplesPerBlock = 512;
    processor->prepareToPlay(sampleRate, samplesPerBlock);

    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::mono());
    layout.inputBuses.add(juce::AudioChannelSet::mono());
    layout.outputBuses.add(juce::AudioChannelSet::mono());

    // Create audio buffer
    juce::AudioBuffer<float> buffer(3, samplesPerBlock);
    juce::MidiBuffer midiBuffer;

    // Fill buffers with high-frequency noise to test filtering
    auto* audioInputData = buffer.getWritePointer(0);
    auto* egInputData = buffer.getWritePointer(1);

    // Generate high-frequency noise
    juce::Random random;
    for (int i = 0; i < samplesPerBlock; ++i) {
        audioInputData[i] = random.nextFloat() * 2.0f - 1.0f;
        egInputData[i] = 1.0f;  // Full EG level
    }

    // Set parameters
    auto vcaEgDepthParam =
        static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::vcaEgDepth));
    auto volumeParam =
        static_cast<juce::AudioParameterFloat*>(apvts->getParameter(ParameterIds::volume));

    if (vcaEgDepthParam != nullptr)
        vcaEgDepthParam->setValueNotifyingHost(0.0f);  // No EG modulation

    if (volumeParam != nullptr)
        volumeParam->setValueNotifyingHost(1.0f);  // Full volume

    // Process
    juce::AudioBuffer<float> processBuffer(3, samplesPerBlock);
    processBuffer.copyFrom(0, 0, audioInputData, samplesPerBlock);
    processBuffer.copyFrom(1, 0, egInputData, samplesPerBlock);

    processor->processBlock(processBuffer, midiBuffer);

    // Input and output RMS
    float inputRMS = 0.0f;
    float outputRMS = 0.0f;

    auto* outputData = processBuffer.getReadPointer(0);
    for (int i = 0; i < processBuffer.getNumSamples(); ++i) {
        inputRMS += audioInputData[i] * audioInputData[i];
        outputRMS += outputData[i] * outputData[i];
    }

    inputRMS = std::sqrt(inputRMS / processBuffer.getNumSamples());
    outputRMS = std::sqrt(outputRMS / processBuffer.getNumSamples());

    // Due to filtering, output RMS should be different from input RMS
    // This is a very basic test that just ensures some processing is happening
    EXPECT_GT(std::abs(outputRMS - inputRMS), 0.01f);
}
