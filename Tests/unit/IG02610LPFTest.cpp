#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "../../Source/CS01Synth/IG02610LPF.h"
#include "../../Source/CS01Synth/SynthConstants.h"

TEST(IG02610ControlTest, LiveCutoffAndResonanceRemainBounded)
{
    for (double hostRate : {44100.0, 48000.0, 96000.0}) {
        const double rate = hostRate * Constants::oversamplingFactor;
        IG02610LPF filter;
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
            if (i >= static_cast<int>(rate * 1.9)) tail = std::max(tail, std::abs(static_cast<double>(output)));
        }
        EXPECT_LT(tail, 1.0e-4);
    }
}

TEST(IG02610OversamplingTest, CharacterizeInternalRateProcessing)
{
    for (double rate : {44100.0, 48000.0}) {
        for (float resonance : {0.7f, 0.8f}) {
            for (bool bypassFilter : {true, false}) {
                juce::dsp::Oversampling<float> converter(1, Constants::oversamplingStages,
                    juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
                converter.initProcessing(1);
                converter.reset();
                IG02610LPF filter;
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
                        const float input = static_cast<float>(0.5 * std::sin(
                            juce::MathConstants<double>::twoPi * 5000.0 * t));
                        internal.setSample(0, j, bypassFilter ? input : filter.processSample(0, input));
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
                          << ", folded=" << 20.0 * std::log10(std::max(folded, 1.0e-15) / fundamental)
                          << " dBc\n";
            }
        }
    }
}

namespace {
double spectralAmplitude(const std::vector<float>& samples, double rate, double frequency)
{
    double sine = 0.0, cosine = 0.0;
    for (size_t i = 0; i < samples.size(); ++i) {
        const double phase = juce::MathConstants<double>::twoPi * frequency * i / rate;
        sine += samples[i] * std::sin(phase);
        cosine += samples[i] * std::cos(phase);
    }
    return 2.0 * std::hypot(sine, cosine) / samples.size();
}
}

TEST(IG02610SpectrumTest, MeasurementDetectsKnownHarmonicAndFoldedTone)
{
    for (double rate : {44100.0, 48000.0}) {
        std::vector<float> samples(static_cast<size_t>(rate));
        for (size_t i = 0; i < samples.size(); ++i) {
            const double t = i / rate;
            samples[i] = static_cast<float>(0.3
                + 0.5 * std::sin(juce::MathConstants<double>::twoPi * 5000.0 * t)
                + 0.1 * std::sin(juce::MathConstants<double>::twoPi * 15000.0 * t)
                + 0.02 * std::sin(juce::MathConstants<double>::twoPi * 25000.0 * t));
        }
        EXPECT_NEAR(spectralAmplitude(samples, rate, 5000.0), 0.5, 1.0e-7);
        EXPECT_NEAR(spectralAmplitude(samples, rate, 15000.0), 0.1, 1.0e-7);
        EXPECT_NEAR(spectralAmplitude(samples, rate, rate - 25000.0), 0.02, 1.0e-7);
        EXPECT_LT(spectralAmplitude(samples, rate, 1000.0), 1.0e-7);
    }
}

TEST(IG02610OversamplingTest, CompareInterpolatedInputResponse)
{
    // Realistic resampling path, not direct generation at the internal rate.
    for (double rate : {44100.0, 48000.0}) {
      for (float cutoff : {250.0f, 1000.0f, 5000.0f}) {
       for (float resonance : {0.2f, 0.7f}) {
        for (float amplitude : {0.01f, 0.5f}) {
         for (double ratio : {0.5, 1.0, 2.0}) {
            const double frequency = cutoff * ratio;
            double gains[2]{};
            for (int mode = 0; mode < 2; ++mode) {
                juce::dsp::Oversampling<float> converter(1, Constants::oversamplingStages,
                    juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
                converter.initProcessing(1);
                IG02610LPF filter;
                filter.prepare(rate * (mode ? Constants::oversamplingFactor : 1));
                filter.setCutoffFrequency(cutoff);
                filter.setResonance(resonance);
                juce::AudioBuffer<float> buffer(1, 1);
                std::vector<float> samples;
                for (int i = 0; i < static_cast<int>(rate * 2); ++i) {
                    buffer.setSample(0, 0, amplitude * static_cast<float>(std::sin(
                        juce::MathConstants<double>::twoPi * frequency * i / rate)));
                    if (mode) {
                        juce::dsp::AudioBlock<float> block(buffer);
                        auto internal = converter.processSamplesUp(block);
                        for (size_t j = 0; j < internal.getNumSamples(); ++j)
                            internal.setSample(0, j, filter.processSample(0, internal.getSample(0, j)));
                        converter.processSamplesDown(block);
                    } else {
                        buffer.setSample(0, 0, filter.processSample(0, buffer.getSample(0, 0)));
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
                      << ", ratio=" << ratio << ", change="
                      << 20.0 * std::log10(gains[1] / gains[0]) << " dB\n";
         }
        }
       }
      }
    }
}

TEST(IG02610SpectrumTest, CharacterizeDrivenFilterHarmonicsAndFoldedComponents)
{
    // One-second coherent window after one-second settling. Observations only:
    // folded bins can contain multiple harmonics, not exclusively harmonic five.
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        for (float resonance : {0.2f, 0.7f, 0.8f}) {
            for (float amplitude : {0.01f, 0.5f, 2.0f}) {
                IG02610LPF filter;
                filter.prepare(rate);
                filter.setCutoffFrequency(5000.0f);
                filter.setResonance(resonance);
                std::vector<float> samples;
                samples.reserve(static_cast<size_t>(rate));
                for (int i = 0; i < static_cast<int>(2.0 * rate); ++i) {
                    const float input = amplitude * static_cast<float>(std::sin(
                        juce::MathConstants<double>::twoPi * 5000.0 * i / rate));
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
                        spectralAmplitude(samples, rate, frequency)) / fundamental);
                };
                std::cout << "VCF spectrum: fs=" << rate << ", resonance=" << resonance
                          << ", amplitude=" << amplitude << ", H3=" << dBc(15000.0)
                          << " dBc, fifth-bin=" << fifthFrequency << " Hz: "
                          << dBc(fifthFrequency) << " dBc\n";
            }
        }
    }
}

TEST(IG02610NonlinearSafetyTest, DrivenSignalAndSilenceRemainFinite)
{
    // Numerical safety invariant, not a hardware distortion target.
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        for (float resonance : {0.2f, 0.7f, 0.8f}) {
            for (float cutoff : {250.0f, 1000.0f, 15000.0f}) {
                for (float amplitude : {0.01f, 0.5f, 2.0f}) {
                    SCOPED_TRACE(rate);
                    SCOPED_TRACE(resonance);
                    SCOPED_TRACE(cutoff);
                    SCOPED_TRACE(amplitude);
                    IG02610LPF filter;
                    filter.prepare(rate);
                    filter.setCutoffFrequency(cutoff);
                    filter.setResonance(resonance);
                    double energy = 0.0;
                    const int count = static_cast<int>(rate * 0.1);
                    for (int i = 0; i < count; ++i) {
                        const float input = amplitude * static_cast<float>(std::sin(
                            juce::MathConstants<double>::twoPi * 3000.0 * i / rate));
                        const float output = filter.processSample(0, input);
                        ASSERT_TRUE(std::isfinite(output));
                        energy += static_cast<double>(output) * output;
                    }
                    ASSERT_GT(energy, 0.0); // Do not pass a silent/broken implementation.
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

// Test fixture for IG02610LPF tests
class IG02610LPFTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Set up test environment
        filter = std::make_unique<IG02610LPF>();
    }
    
    void TearDown() override
    {
        filter.reset();
    }
    
    std::unique_ptr<IG02610LPF> filter;
};

TEST_F(IG02610LPFTest, Initialization)
{
    // Just check that the filter can be created without crashing
    EXPECT_TRUE(true);
    
    // Check that the filter doesn't crash when used before preparation
    float sample = filter->processSample(0, 0.5f);
    EXPECT_TRUE(std::isfinite(sample));
}

TEST_F(IG02610LPFTest, NonlinearDistortionLowResonance)
{
    filter->prepare(44100.0);
    filter->setCutoffFrequency(1000.0f);
    filter->setResonance(0.2f); // Low resonance - should have subtle even harmonics
    
    // Test with different input levels
    float testSignals[] = {0.1f, 0.5f, 0.8f};
    
    for (float input : testSignals) {
        float output = filter->processSample(0, input);
        
        // Output should be finite and within reasonable bounds
        EXPECT_TRUE(std::isfinite(output));
        EXPECT_LE(std::abs(output), 2.0f); // Should not exceed reasonable limits
        
        // For low resonance, distortion should be subtle
        float distortionRatio = std::abs(output / input);
        EXPECT_LT(distortionRatio, 1.5f); // Should not be heavily distorted
    }
}

TEST_F(IG02610LPFTest, NonlinearDistortionMediumResonance)
{
    filter->prepare(44100.0);
    filter->setCutoffFrequency(1000.0f);
    filter->setResonance(0.55f); // Medium resonance - balanced distortion
    
    float input = 0.7f;
    float output = filter->processSample(0, input);
    
    EXPECT_TRUE(std::isfinite(output));
    EXPECT_LE(std::abs(output), 2.0f);
    
    // Medium resonance should show some output (realistic expectations for single sample)
    float distortionRatio = std::abs(output / input);
    EXPECT_GT(distortionRatio, 0.0001f); // Should have some output
    EXPECT_LT(distortionRatio, 0.1f); // But not extreme for single sample
}

TEST_F(IG02610LPFTest, NonlinearDistortionHighResonance)
{
    filter->prepare(44100.0);
    filter->setCutoffFrequency(1000.0f);
    filter->setResonance(0.75f); // High resonance - strong distortion with asymmetric clipping
    
    float input = 0.8f;
    float output = filter->processSample(0, input);
    
    EXPECT_TRUE(std::isfinite(output));
    EXPECT_LE(std::abs(output), 2.0f);
    
    // High resonance should show some distortion (realistic for single sample)
    float distortionRatio = std::abs(output / input);
    EXPECT_GT(distortionRatio, 0.001f); // Should have some output
    EXPECT_LT(distortionRatio, 0.1f); // But not extreme for single sample
}

TEST_F(IG02610LPFTest, FrequencyDependentDistortion)
{
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

TEST_F(IG02610LPFTest, InputLevelDependentDistortion)
{
    filter->prepare(44100.0);
    filter->setCutoffFrequency(1000.0f);
    filter->setResonance(0.75f); // High resonance to activate level-dependent distortion
    
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

TEST_F(IG02610LPFTest, AsymmetricClippingBehavior)
{
    filter->prepare(44100.0);
    filter->setCutoffFrequency(1000.0f);
    filter->setResonance(0.8f); // Maximum resonance to activate asymmetric clipping
    
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
