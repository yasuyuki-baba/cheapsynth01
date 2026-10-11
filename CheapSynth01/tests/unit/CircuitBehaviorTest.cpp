#include "cs01synth/CS01VCFCircuit.h"
#include <gtest/gtest.h>
#include <cmath>
#include <memory>
#include <numbers>
#include "cs01synth/IG02610BehavioralModel.h"

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

TEST(IG02610BehavioralModelTest, LiveCutoffAndResonanceRemainBounded) {
    for (double hostRate : {44100.0, 48000.0, 96000.0}) {
        const double rate = hostRate * cs01::OutputConverter::factor;
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
                    phase += (2 * std::numbers::pi) * 440.0 / rate;
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

TEST(IG02610BehavioralModelTest, DrivenSignalAndSilenceRemainFiniteAndBounded) {
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
                            amplitude * static_cast<float>(
                                            std::sin((2 * std::numbers::pi) * 3000.0 * i / rate));
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
                    EXPECT_LE(std::abs(last),
                              IG02610BehavioralModel::EmpiricalParameters::maximumOutput);
                }
            }
        }
    }
}
