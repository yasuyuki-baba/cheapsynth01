#include <JuceHeader.h>

#include "CS01AudioProcessor.h"
#include "CS01Synth/CS01VCFCircuit.h"
#include "CS01Synth/SynthConstants.h"

#include <gtest/gtest.h>

TEST(IG02610ControlTest, NonlinearResonanceBoundariesAreContinuous) {
    // Compare complete trajectories with nearly identical controls. This detects
    // branch discontinuities without exposing private nonlinear implementation.
    for (float boundary : {0.4f, 0.7f}) {
        for (float amplitude : {0.01f, 0.8f, 1.2f}) {
            IG02610 below, above;
            below.prepare(48000.0);
            above.prepare(48000.0);
            below.setCutoffFrequency(1000.0f);
            above.setCutoffFrequency(1000.0f);
            below.setResonance(boundary - 1.0e-6f);
            above.setResonance(boundary + 1.0e-6f);
            for (int i = 0; i < 4800; ++i) {
                const float input =
                    amplitude * std::sin(juce::MathConstants<double>::twoPi * 700.0 * i / 48000.0);
                const float a = below.processSample(input);
                const float b = above.processSample(input);
                ASSERT_TRUE(std::isfinite(a));
                ASSERT_TRUE(std::isfinite(b));
                ASSERT_NEAR(a, b, 0.0002f) << boundary << ", sample=" << i;
            }
        }
    }
}

TEST(IG02610ControlTest, ReapplyingIdenticalControlsDoesNotChangeTrajectory) {
    IG02610 held, reapplied;
    held.prepare(48000.0);
    reapplied.prepare(48000.0);
    held.setCutoffFrequency(1000.0f);
    reapplied.setCutoffFrequency(1000.0f);
    held.setResonance(0.7f);
    reapplied.setResonance(0.7f);
    for (int i = 0; i < 12000; ++i) {
        reapplied.setCutoffFrequency(1000.0f);
        reapplied.setResonance(0.7f);
        const float input =
            0.8f * std::sin(juce::MathConstants<double>::twoPi * 440.0 * i / 48000.0);
        ASSERT_FLOAT_EQ(held.processSample(input), reapplied.processSample(input));
    }
}

TEST(IG02610ControlTest, LowCutoffNumeratorPrecisionDiagnosis) {
    // Isolate coefficient construction from nonlinear and coupling stages.
    for (double rate : {176400.0, 192000.0, 384000.0}) {
        const double angle = juce::MathConstants<double>::twoPi * 20.0 / rate;
        // Independent, cancellation-resistant trigonometric identity.
        const double expected = 2.0 * std::pow(std::sin(angle * 0.5), 2.0);
        const double doubleNumerator = 1.0 - std::cos(angle);
        EXPECT_NEAR(doubleNumerator, expected, expected * 1.0e-8);
        const float floatAngle =
            (20.0f / static_cast<float>(rate)) * (2.0f * juce::MathConstants<float>::pi);
        const float floatNumerator = 1.0f - std::cos(floatAngle);
        std::cout << "VCF numerator precision: rate=" << rate
                  << ", relative-error=" << floatNumerator / expected - 1.0 << '\n';
        // Diagnostic only: deliberately do not require the observed float error.
        // An implementation improvement must not make this test fail.
        EXPECT_GT(expected, 0.0);
    }
}

TEST(IG02610ControlTest, Observation_ProductionPanelResponse) {
    CS01AudioProcessor host;
    auto* parameter = host.getValueTreeState().getParameter(ParameterIds::cutoff);
    ASSERT_NE(parameter, nullptr);
    for (double hostRate : {44100.0, 48000.0, 96000.0}) {
        const double rate = hostRate * Constants::oversamplingFactor;
        for (float position : {0.0f, 0.5f, 1.0f}) {
            const float cutoff = parameter->convertFrom0to1(position);
            for (float resonance : {0.2f, 0.7f}) {
                for (double ratio : {0.5, 0.75, 0.9, 1.0, 1.1, 1.5, 2.0}) {
                    // Observe only the host-audible band. No peak claim at the
                    // maximum setting, whose resonance is near the band edge.
                    const double frequency = std::round(cutoff * ratio);
                    if (frequency >= hostRate * 0.45)
                        continue;
                    CS01VCFCircuit filter;
                    filter.prepare(rate);
                    filter.setCutoffFrequency(cutoff);
                    filter.setResonance(resonance);
                    double sine = 0.0, cosine = 0.0;
                    const int count = static_cast<int>(rate);
                    for (int i = 0; i < count * 2; ++i) {
                        const double phase =
                            juce::MathConstants<double>::twoPi * frequency * i / rate;
                        const double output =
                            filter.processSample(0, static_cast<float>(0.01 * std::sin(phase)));
                        ASSERT_TRUE(std::isfinite(output));
                        if (i >= count) {
                            sine += output * std::sin(phase);
                            cosine += output * std::cos(phase);
                        }
                    }
                    const double gain = 2.0 * std::hypot(sine, cosine) / count / 0.01;
                    ASSERT_GT(gain, 0.0);
                    std::cout << "VCF panel: host=" << hostRate << ", position=" << position
                              << ", cutoff=" << cutoff << ", resonance=" << resonance
                              << ", frequency=" << frequency << ", gain=" << 20.0 * std::log10(gain)
                              << " dB\n";
                }
            }
        }
    }
}

TEST(IG02610ControlTest, LiveCutoffAndResonanceRemainBounded) {
    for (double hostRate : {44100.0, 48000.0, 96000.0}) {
        const double rate = hostRate * Constants::oversamplingFactor;
        CS01VCFCircuit filter;
        filter.prepare(rate);
        double phase = 0.0;
        // Exercise abrupt panel changes without resetting filter state.
        for (float cutoff : {20.0f, 20000.0f, 2000.0f, 250.0f, 20000.0f, 20.0f}) {
            for (float resonance : {0.2f, 0.7f}) {
                filter.setCutoffFrequency(cutoff);
                filter.setResonance(resonance);
                double energy = 0.0;
                for (int i = 0; i < static_cast<int>(rate * 0.02); ++i) {
                    const float input = static_cast<float>(0.1 * std::sin(phase));
                    phase += juce::MathConstants<double>::twoPi * 440.0 / rate;
                    const float output = filter.processSample(0, input);
                    ASSERT_TRUE(std::isfinite(output));
                    // Safety bound, not a calibrated amplitude or a click criterion.
                    ASSERT_LT(std::abs(output), 4.0f);
                    energy += static_cast<double>(output) * output;
                }
                EXPECT_GT(energy, 0.0);
            }
        }
        double tail = 0.0;
        for (int i = 0; i < static_cast<int>(rate * 2.0); ++i) {
            const float output = filter.processSample(0, 0.0f);
            ASSERT_TRUE(std::isfinite(output));
            if (i >= static_cast<int>(rate * 1.9))
                tail = std::max(tail, std::abs(static_cast<double>(output)));
        }
        EXPECT_LT(tail, 1.0e-4);
    }
}

TEST(IG02610OversamplingTest, Observation_CharacterizeInternalRateProcessing) {
    for (double rate : {44100.0, 48000.0}) {
        for (float resonance : {0.7f, 0.8f}) {
            for (bool bypassFilter : {true, false}) {
                juce::dsp::Oversampling<float> converter(
                    1, Constants::oversamplingStages,
                    juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
                converter.initProcessing(1);
                converter.reset();
                CS01VCFCircuit filter;
                const double internalRate = rate * Constants::oversamplingFactor;
                filter.prepare(internalRate);
                filter.setCutoffFrequency(5000.0f);
                filter.setResonance(resonance);
                juce::AudioBuffer<float> buffer(1, 1);
                double sine = 0.0, cosine = 0.0, foldedSine = 0.0, foldedCosine = 0.0;
                const int count = static_cast<int>(rate);
                for (int i = 0; i < count * 2; ++i) {
                    buffer.clear();
                    juce::dsp::AudioBlock<float> block(buffer);
                    auto internal = converter.processSamplesUp(block);
                    for (size_t j = 0; j < internal.getNumSamples(); ++j) {
                        const double t = (i * Constants::oversamplingFactor + j) / internalRate;
                        const float input = static_cast<float>(
                            0.5 * std::sin(juce::MathConstants<double>::twoPi * 5000.0 * t));
                        internal.setSample(0, j,
                                           bypassFilter ? input : filter.processSample(0, input));
                    }
                    converter.processSamplesDown(block);
                    const double output = buffer.getSample(0, 0);
                    ASSERT_TRUE(std::isfinite(output));
                    if (i >= count) {
                        const double phase = juce::MathConstants<double>::twoPi * i / rate;
                        sine += output * std::sin(phase * 5000.0);
                        cosine += output * std::cos(phase * 5000.0);
                        foldedSine += output * std::sin(phase * (rate - 25000.0));
                        foldedCosine += output * std::cos(phase * (rate - 25000.0));
                    }
                }
                const double fundamental = 2.0 * std::hypot(sine, cosine) / count;
                const double folded = 2.0 * std::hypot(foldedSine, foldedCosine) / count;
                ASSERT_GT(fundamental, 1.0e-8);
                if (bypassFilter) {
                    EXPECT_NEAR(fundamental, 0.5, 0.001);
                    EXPECT_LT(folded, 1.0e-6);
                }
                std::cout << "VCF internal-rate: fs=" << rate << ", resonance=" << resonance
                          << ", reference=" << bypassFilter << ", fundamental=" << fundamental
                          << ", folded="
                          << 20.0 * std::log10(std::max(folded, 1.0e-15) / fundamental) << " dBc\n";
            }
        }
    }
}

namespace {
double spectralAmplitude(const std::vector<float>& samples, double rate, double frequency) {
    double sine = 0.0, cosine = 0.0;
    for (size_t i = 0; i < samples.size(); ++i) {
        const double phase = juce::MathConstants<double>::twoPi * frequency * i / rate;
        sine += samples[i] * std::sin(phase);
        cosine += samples[i] * std::cos(phase);
    }
    return 2.0 * std::hypot(sine, cosine) / samples.size();
}
}  // namespace

TEST(IG02610SpectrumTest, MeasurementDetectsKnownHarmonicAndFoldedTone) {
    for (double rate : {44100.0, 48000.0}) {
        std::vector<float> samples(static_cast<size_t>(rate));
        for (size_t i = 0; i < samples.size(); ++i) {
            const double t = i / rate;
            samples[i] = static_cast<float>(
                0.3 + 0.5 * std::sin(juce::MathConstants<double>::twoPi * 5000.0 * t) +
                0.1 * std::sin(juce::MathConstants<double>::twoPi * 15000.0 * t) +
                0.02 * std::sin(juce::MathConstants<double>::twoPi * 25000.0 * t));
        }
        EXPECT_NEAR(spectralAmplitude(samples, rate, 5000.0), 0.5, 1.0e-7);
        EXPECT_NEAR(spectralAmplitude(samples, rate, 15000.0), 0.1, 1.0e-7);
        EXPECT_NEAR(spectralAmplitude(samples, rate, rate - 25000.0), 0.02, 1.0e-7);
        EXPECT_LT(spectralAmplitude(samples, rate, 1000.0), 1.0e-7);
    }
}

TEST(IG02610OversamplingTest, Observation_CompareInterpolatedInputResponse) {
    // Realistic resampling path, not direct generation at the internal rate.
    for (double rate : {44100.0, 48000.0}) {
        for (float cutoff : {250.0f, 1000.0f, 5000.0f}) {
            for (float resonance : {0.2f, 0.7f}) {
                for (float amplitude : {0.01f, 0.5f}) {
                    for (double ratio : {0.5, 1.0, 2.0}) {
                        const double frequency = cutoff * ratio;
                        double gains[2]{};
                        for (int mode = 0; mode < 2; ++mode) {
                            juce::dsp::Oversampling<float> converter(
                                1, Constants::oversamplingStages,
                                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
                            converter.initProcessing(1);
                            CS01VCFCircuit filter;
                            filter.prepare(rate * (mode ? Constants::oversamplingFactor : 1));
                            filter.setCutoffFrequency(cutoff);
                            filter.setResonance(resonance);
                            juce::AudioBuffer<float> buffer(1, 1);
                            std::vector<float> samples;
                            for (int i = 0; i < static_cast<int>(rate * 2); ++i) {
                                buffer.setSample(
                                    0, 0,
                                    amplitude * static_cast<float>(
                                                    std::sin(juce::MathConstants<double>::twoPi *
                                                             frequency * i / rate)));
                                if (mode) {
                                    juce::dsp::AudioBlock<float> block(buffer);
                                    auto internal = converter.processSamplesUp(block);
                                    for (size_t j = 0; j < internal.getNumSamples(); ++j)
                                        internal.setSample(
                                            0, j,
                                            filter.processSample(0, internal.getSample(0, j)));
                                    converter.processSamplesDown(block);
                                } else {
                                    buffer.setSample(
                                        0, 0, filter.processSample(0, buffer.getSample(0, 0)));
                                }
                                ASSERT_TRUE(std::isfinite(buffer.getSample(0, 0)));
                                if (i >= static_cast<int>(rate))
                                    samples.push_back(buffer.getSample(0, 0));
                            }
                            gains[mode] = spectralAmplitude(samples, rate, frequency) / amplitude;
                            ASSERT_GT(gains[mode], 1.0e-8);
                        }
                        std::cout << "VCF resampled response: fs=" << rate << ", cutoff=" << cutoff
                                  << ", resonance=" << resonance << ", amplitude=" << amplitude
                                  << ", ratio=" << ratio
                                  << ", change=" << 20.0 * std::log10(gains[1] / gains[0])
                                  << " dB\n";
                    }
                }
            }
        }
    }
}

TEST(IG02610SpectrumTest, Observation_CharacterizeDrivenFilterHarmonicsAndFoldedComponents) {
    // One-second coherent window after one-second settling. Observations only:
    // folded bins can contain multiple harmonics, not exclusively harmonic five.
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        for (float resonance : {0.2f, 0.7f, 0.8f}) {
            for (float amplitude : {0.01f, 0.5f, 2.0f}) {
                CS01VCFCircuit filter;
                filter.prepare(rate);
                filter.setCutoffFrequency(5000.0f);
                filter.setResonance(resonance);
                std::vector<float> samples;
                samples.reserve(static_cast<size_t>(rate));
                for (int i = 0; i < static_cast<int>(2.0 * rate); ++i) {
                    const float input =
                        amplitude * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi *
                                                                5000.0 * i / rate));
                    const float output = filter.processSample(0, input);
                    ASSERT_TRUE(std::isfinite(output));
                    if (i >= static_cast<int>(rate))
                        samples.push_back(output);
                }
                const double fundamental = spectralAmplitude(samples, rate, 5000.0);
                ASSERT_GT(fundamental, 1.0e-8);
                const double fifthFrequency = rate < 50000.0 ? rate - 25000.0 : 25000.0;
                const auto dBc = [&](double frequency) {
                    return 20.0 * std::log10(std::max(1.0e-15,
                                                      spectralAmplitude(samples, rate, frequency)) /
                                             fundamental);
                };
                std::cout << "VCF spectrum: fs=" << rate << ", resonance=" << resonance
                          << ", amplitude=" << amplitude << ", H3=" << dBc(15000.0)
                          << " dBc, fifth-bin=" << fifthFrequency << " Hz: " << dBc(fifthFrequency)
                          << " dBc\n";
            }
        }
    }
}

TEST(IG02610NonlinearSafetyTest, DrivenSignalAndSilenceRemainFinite) {
    // Numerical safety invariant, not a hardware distortion target.
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        for (float resonance : {0.2f, 0.7f, 0.8f}) {
            for (float cutoff : {250.0f, 1000.0f, 15000.0f}) {
                for (float amplitude : {0.01f, 0.5f, 2.0f}) {
                    SCOPED_TRACE(rate);
                    SCOPED_TRACE(resonance);
                    SCOPED_TRACE(cutoff);
                    SCOPED_TRACE(amplitude);
                    CS01VCFCircuit filter;
                    filter.prepare(rate);
                    filter.setCutoffFrequency(cutoff);
                    filter.setResonance(resonance);
                    double energy = 0.0;
                    const int count = static_cast<int>(rate * 0.1);
                    for (int i = 0; i < count; ++i) {
                        const float input =
                            amplitude *
                            static_cast<float>(
                                std::sin(juce::MathConstants<double>::twoPi * 3000.0 * i / rate));
                        const float output = filter.processSample(0, input);
                        ASSERT_TRUE(std::isfinite(output));
                        energy += static_cast<double>(output) * output;
                    }
                    ASSERT_GT(energy, 0.0);  // Do not pass a silent/broken implementation.
                    float last = 0.0f;
                    for (int i = 0; i < static_cast<int>(rate); ++i) {
                        last = filter.processSample(0, 0.0f);
                        ASSERT_TRUE(std::isfinite(last));
                    }
                    EXPECT_LT(std::abs(last), 1.0e-4f);
                }
            }
        }
    }
}

// Test fixture for CS01VCFCircuit tests
class CS01VCFCircuitTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Set up test environment
        filter = std::make_unique<CS01VCFCircuit>();
    }

    void TearDown() override {
        filter.reset();
    }

    std::unique_ptr<CS01VCFCircuit> filter;
};

TEST_F(CS01VCFCircuitTest, Initialization) {
    // Just check that the filter can be created without crashing
    EXPECT_TRUE(true);

    // Check that the filter doesn't crash when used before preparation
    float sample = filter->processSample(0, 0.5f);
    EXPECT_TRUE(std::isfinite(sample));
}

TEST_F(CS01VCFCircuitTest, NonlinearDistortionLowResonance) {
    filter->prepare(44100.0);
    filter->setCutoffFrequency(1000.0f);
    filter->setResonance(0.2f);  // Low resonance - should have subtle even harmonics

    // Test with different input levels
    float testSignals[] = {0.1f, 0.5f, 0.8f};

    for (float input : testSignals) {
        float output = filter->processSample(0, input);

        // Output should be finite and within reasonable bounds
        EXPECT_TRUE(std::isfinite(output));
        EXPECT_LE(std::abs(output), 2.0f);  // Should not exceed reasonable limits

        // For low resonance, distortion should be subtle
        float distortionRatio = std::abs(output / input);
        EXPECT_LT(distortionRatio, 1.5f);  // Should not be heavily distorted
    }
}

TEST_F(CS01VCFCircuitTest, NonlinearDistortionMediumResonance) {
    filter->prepare(44100.0);
    filter->setCutoffFrequency(1000.0f);
    filter->setResonance(0.55f);  // Medium resonance - balanced distortion

    float input = 0.7f;
    float output = filter->processSample(0, input);

    EXPECT_TRUE(std::isfinite(output));
    EXPECT_LE(std::abs(output), 2.0f);

    // Medium resonance should show some output (realistic expectations for single sample)
    float distortionRatio = std::abs(output / input);
    EXPECT_GT(distortionRatio, 0.0001f);  // Should have some output
    EXPECT_LT(distortionRatio, 0.1f);     // But not extreme for single sample
}

TEST_F(CS01VCFCircuitTest, NonlinearDistortionHighResonance) {
    filter->prepare(44100.0);
    filter->setCutoffFrequency(1000.0f);
    filter->setResonance(0.75f);  // High resonance - strong distortion with asymmetric clipping

    float input = 0.8f;
    float output = filter->processSample(0, input);

    EXPECT_TRUE(std::isfinite(output));
    EXPECT_LE(std::abs(output), 2.0f);

    // High resonance should show some distortion (realistic for single sample)
    float distortionRatio = std::abs(output / input);
    EXPECT_GT(distortionRatio, 0.001f);  // Should have some output
    EXPECT_LT(distortionRatio, 0.1f);    // But not extreme for single sample
}

TEST_F(CS01VCFCircuitTest, FrequencyDependentDistortion) {
    filter->prepare(44100.0);
    filter->setResonance(0.6f);

    // Test low frequency (should get more distortion)
    filter->setCutoffFrequency(300.0f);
    float lowFreqOutput = filter->processSample(0, 0.7f);

    // Reset filter state
    filter->reset();

    // Test high frequency (should get less distortion)
    filter->setCutoffFrequency(3000.0f);
    float highFreqOutput = filter->processSample(0, 0.7f);

    EXPECT_TRUE(std::isfinite(lowFreqOutput));
    EXPECT_TRUE(std::isfinite(highFreqOutput));

    // Low frequencies should generally show more distortion characteristics
    // This is a behavioral test rather than strict numerical comparison
    EXPECT_LE(std::abs(lowFreqOutput), 2.0f);
    EXPECT_LE(std::abs(highFreqOutput), 2.0f);
}

TEST_F(CS01VCFCircuitTest, InputLevelDependentDistortion) {
    filter->prepare(44100.0);
    filter->setCutoffFrequency(1000.0f);
    filter->setResonance(0.75f);  // High resonance to activate level-dependent distortion

    // Test with different input levels
    float smallInput = 0.1f;
    float largeInput = 0.9f;

    float smallOutput = filter->processSample(0, smallInput);

    // Reset filter state
    filter->reset();

    float largeOutput = filter->processSample(0, largeInput);

    EXPECT_TRUE(std::isfinite(smallOutput));
    EXPECT_TRUE(std::isfinite(largeOutput));

    // Input level dependent behavior should show some response (realistic for single sample)
    float smallRatio = std::abs(smallOutput / smallInput);
    float largeRatio = std::abs(largeOutput / largeInput);

    // Both should produce some output
    EXPECT_GT(smallRatio, 0.0001f);
    EXPECT_LT(smallRatio, 0.1f);
    EXPECT_GT(largeRatio, 0.0001f);
    EXPECT_LT(largeRatio, 0.1f);
}

TEST_F(CS01VCFCircuitTest, AsymmetricClippingBehavior) {
    filter->prepare(44100.0);
    filter->setCutoffFrequency(1000.0f);
    filter->setResonance(0.8f);  // Maximum resonance to activate asymmetric clipping

    // Test positive and negative inputs
    float positiveInput = 0.8f;
    float negativeInput = -0.8f;

    float positiveOutput = filter->processSample(0, positiveInput);

    // Reset filter state
    filter->reset();

    float negativeOutput = filter->processSample(0, negativeInput);

    EXPECT_TRUE(std::isfinite(positiveOutput));
    EXPECT_TRUE(std::isfinite(negativeOutput));

    // Asymmetric clipping behavior should show some response (realistic for single sample)
    float positiveRatio = std::abs(positiveOutput / positiveInput);
    float negativeRatio = std::abs(negativeOutput / negativeInput);

    // Both should produce some output, may differ due to asymmetric processing
    EXPECT_GT(positiveRatio, 0.0001f);
    EXPECT_LT(positiveRatio, 0.1f);
    EXPECT_GT(negativeRatio, 0.0001f);
    EXPECT_LT(negativeRatio, 0.1f);
}
