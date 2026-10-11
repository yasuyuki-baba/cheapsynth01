#include "CS01Synth/ToneGenerator.h"
#include "CS01Synth/WaveformStrategies.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <numbers>
#include <iostream>

namespace {
struct SpectrumObservation {
    double mean = 0.0, rms = 0.0, fundamental = 0.0, third = 0.0;
};
template <typename Generate>
SpectrumObservation observeSpectrum(double rate, double frequency, Generate generate) {
    double sum = 0.0, power = 0.0, sine = 0.0, cosine = 0.0;
    double sine3 = 0.0, cosine3 = 0.0;
    const int count = static_cast<int>(rate);
    for (int i = 0; i < count * 3; ++i) {
        const double phase = (2 * std::numbers::pi) * frequency * i / rate;
        const double value = generate(i, phase);
        if (i < count * 2)
            continue;
        sum += value;
        power += value * value;
        sine += value * std::sin(phase);
        cosine += value * std::cos(phase);
        sine3 += value * std::sin(phase * 3.0);
        cosine3 += value * std::cos(phase * 3.0);
    }
    return {sum / count, std::sqrt(power / count), 2.0 * std::hypot(sine, cosine) / count,
            2.0 * std::hypot(sine3, cosine3) / count};
}
}  // namespace

TEST(VcoSpectrumTest, MeasurementWithKnownSine) {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        const auto result = observeSpectrum(
            rate, 110.0, [](int, double phase) { return 0.3 + 0.5 * std::sin(phase); });
        EXPECT_NEAR(result.mean, 0.3, 1.0e-10);
        EXPECT_NEAR(result.rms, std::sqrt(0.09 + 0.125), 1.0e-10);
        EXPECT_NEAR(result.fundamental, 0.5, 1.0e-10);
        EXPECT_NEAR(result.third, 0.0, 1.0e-10);
    }
}

namespace {
template <typename Generate>
double measureSpectralAmplitude(double rate, double frequency, Generate generate) {
    double sine = 0.0, cosine = 0.0;
    const int count = static_cast<int>(rate);
    for (int i = 0; i < count * 3; ++i) {
        const double value = generate(i);
        if (i < count * 2)
            continue;
        const double phase = (2 * std::numbers::pi) * frequency * i / rate;
        sine += value * std::sin(phase);
        cosine += value * std::cos(phase);
    }
    return 2.0 * std::hypot(sine, cosine) / count;
}
}  // namespace

TEST(VcoSpectrumTest, AliasProbeWithKnownSignals) {
    // At 48 kHz, 25 kHz samples fold to 23 kHz. Integer-Hz bins over
    // a one-second window prevent ordinary spectral leakage in this control.
    const auto signal = [](int i) {
        return 0.5 * std::sin((2 * std::numbers::pi) * 5000.0 * i / 48000.0) +
               0.1 * std::sin((2 * std::numbers::pi) * 25000.0 * i / 48000.0);
    };
    EXPECT_NEAR(measureSpectralAmplitude(48000.0, 5000.0, signal), 0.5, 1.0e-9);
    EXPECT_NEAR(measureSpectralAmplitude(48000.0, 23000.0, signal), 0.1, 1.0e-9);
    EXPECT_NEAR(measureSpectralAmplitude(48000.0, 3000.0, signal), 0.0, 1.0e-9);
}

namespace {
struct ProductionVcoHarness {
    cs01::ParameterState state;
    ToneGenerator generator{state};
    ProductionVcoHarness(int waveform, double rate, double frequency) {
        state.set(cs01::Param::WaveType, waveform);
        state.set(cs01::Param::ModDepth, 0);
        generator.prepare(rate);
        generator.startNote(69, 1, 8192);
        generator.setPitchBend(static_cast<float>(12 * std::log2(frequency / 440.)));
        generator.updateBlockRateParameters();
    }
};
}  // namespace
TEST(VcoSpectrumTest, Observation_HighFrequencyAliasCharacterization) {
    // Probe one non-harmonic bin only, not total alias power or a hardware target.
    for (double rate : {44100.0, 48000.0}) {
        const double aliasFrequency = rate - 25000.0;
        for (int waveform = 0; waveform < 4; ++waveform) {
            double amplitudes[2]{};
            for (int bin = 0; bin < 2; ++bin) {
                std::unique_ptr<IWaveformStrategy> strategy;
                switch (waveform) {
                    case 0:
                        strategy = std::make_unique<TriangleWaveformStrategy>();
                        break;
                    case 1:
                        strategy = std::make_unique<SawtoothWaveformStrategy>();
                        break;
                    case 2:
                        strategy = std::make_unique<SquareWaveformStrategy>();
                        break;
                    default:
                        strategy = std::make_unique<PulseWaveformStrategy>();
                        break;
                }
                cs01::Oscillator pwm;
                float previous = 0.0f;
                const float increment = static_cast<float>(5000.0 / rate);
                amplitudes[bin] =
                    measureSpectralAmplitude(rate, bin == 0 ? 5000.0 : aliasFrequency, [&](int i) {
                        const float phase = static_cast<float>(std::fmod(i * 5000.0 / rate, 1.0));
                        float master = phase < 0.5f ? 1.0f : -1.0f;
                        master += poly_blep(phase, increment);
                        master -= poly_blep(std::fmod(phase + 0.5f, 1.0f), increment);
                        master = std::tanh(master * 1.2f);
                        return std::tanh(strategy->generate(master, phase, increment,
                                                            static_cast<float>(rate), previous,
                                                            pwm) *
                                         1.2f);
                    });
            }
            ASSERT_GT(amplitudes[0], 0.0);
            ASSERT_TRUE(std::isfinite(amplitudes[1]));
            std::cout << "VCO alias probe: fs=" << rate << ", waveform=" << waveform
                      << ", bin=" << aliasFrequency << ", dBc="
                      << 20.0 * std::log10(std::max(amplitudes[1], 1.0e-15) / amplitudes[0])
                      << '\n';
        }
    }
}

TEST(VcoSpectrumTest, Observation_SquareAliasProcessingStageCharacterization) {
    for (double rate : {44100.0, 48000.0}) {
        for (int stage = 0; stage < 4; ++stage) {
            const auto generate = [&](int i) {
                const float phase = static_cast<float>(std::fmod(i * 5000.0 / rate, 1.0));
                const float increment = static_cast<float>(5000.0 / rate);
                float value = phase < 0.5f ? 1.0f : -1.0f;
                // Stage 0 is the raw square control; stage 1 adds BLEP.
                if (stage >= 1) {
                    value += poly_blep(phase, increment);
                    value -= poly_blep(std::fmod(phase + 0.5f, 1.0f), increment);
                }
                if (stage >= 2)
                    value = std::tanh(value * 1.2f);
                if (stage >= 3)
                    value = std::tanh(value * 1.2f);
                return value;
            };
            const double fundamental = measureSpectralAmplitude(rate, 5000.0, generate);
            ASSERT_GT(fundamental, 0.0);
            for (int harmonic : {5, 7, 9}) {
                const double wrapped = std::fmod(5000.0 * harmonic, rate);
                const double bin = std::min(wrapped, rate - wrapped);
                const double amplitude = measureSpectralAmplitude(rate, bin, generate);
                ASSERT_TRUE(std::isfinite(amplitude));
                std::cout << "VCO alias stage: fs=" << rate << ", stage=" << stage
                          << ", harmonic=" << harmonic << ", bin=" << bin
                          << ", amplitude=" << amplitude << ", dBc="
                          << 20.0 * std::log10(std::max(amplitude, 1.0e-15) / fundamental) << '\n';
            }
        }
    }
}

TEST(VcoSpectrumTest, Observation_WaveformStrategySampleRateCharacterization) {
    // Isolate strategies using a controlled master; this is not a full-VCO test.
    for (double frequency : {110.0, 440.0}) {
        for (double rate : {44100.0, 48000.0, 96000.0}) {
            for (int waveform = 0; waveform < 5; ++waveform) {
                std::unique_ptr<IWaveformStrategy> strategy;
                switch (waveform) {
                    case 0:
                        strategy = std::make_unique<TriangleWaveformStrategy>();
                        break;
                    case 1:
                        strategy = std::make_unique<SawtoothWaveformStrategy>();
                        break;
                    case 2:
                        strategy = std::make_unique<SquareWaveformStrategy>();
                        break;
                    case 3:
                        strategy = std::make_unique<PulseWaveformStrategy>();
                        break;
                    default:
                        strategy = std::make_unique<PWMWaveformStrategy>();
                        break;
                }
                cs01::Oscillator pwm;
                pwm.initialise([](float x) {
                    return std::asin(std::sin(x)) * 2.0f / std::numbers::pi_v<float>;
                });
                pwm.prepare(rate);
                pwm.setFrequency(0.0f, true);
                float previous = 0.0f;
                const float increment = static_cast<float>(frequency / rate);
                const auto result = observeSpectrum(rate, frequency, [&](int i, double) {
                    const float phase = static_cast<float>(std::fmod(i * frequency / rate, 1.0));
                    float master = phase < 0.5f ? 1.0f : -1.0f;
                    master += poly_blep(phase, increment);
                    master -= poly_blep(std::fmod(phase + 0.5f, 1.0f), increment);
                    master = std::tanh(master * 1.2f);
                    return std::tanh(strategy->generate(master, phase, increment,
                                                        static_cast<float>(rate), previous, pwm) *
                                     1.2f);
                });
                ASSERT_TRUE(std::isfinite(result.rms));
                ASSERT_GT(result.fundamental, 0.0);
                EXPECT_LE(result.rms, 1.0);
                std::cout << "VCO spectrum: f=" << frequency << ", fs=" << rate
                          << ", waveform=" << waveform << ", mean=" << result.mean
                          << ", RMS=" << result.rms << ", fundamental=" << result.fundamental
                          << ", H3=" << result.third << '\n';
            }
        }
    }
}

TEST(ToneGeneratorRealTest, Observation_ProductionAliasAndOnset) {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        for (double frequency : {3000.0, 5000.0}) {
            for (int waveform = 0; waveform < 5; ++waveform) {
                ProductionVcoHarness harness(waveform, rate, frequency);
                std::vector<float> output(static_cast<size_t>(rate * 3));
                int onset = -1;
                for (size_t i = 0; i < output.size(); ++i) {
                    output[i] = harness.generator.getNextSample();
                    ASSERT_TRUE(std::isfinite(output[i]));
                    if (onset < 0 && std::abs(output[i]) > 1.0e-5f)
                        onset = static_cast<int>(i);
                }
                ASSERT_GE(onset, 0);
                EXPECT_LT(onset, static_cast<int>(rate / frequency));
                const double fundamental =
                    measureSpectralAmplitude(rate, frequency, [&](int i) { return output[i]; });
                ASSERT_GT(fundamental, 0.0);
                for (int harmonic : {9, 17, 33}) {
                    if (frequency * harmonic <= rate * 0.5)
                        continue;
                    const double wrapped = std::fmod(frequency * harmonic, rate);
                    const double bin = std::min(wrapped, rate - wrapped);
                    if (bin == 0 ||
                        std::abs(bin / frequency - std::round(bin / frequency)) < 1.0e-8)
                        continue;  // Cannot distinguish aliases that coincide with true harmonics.
                    const double amplitude =
                        measureSpectralAmplitude(rate, bin, [&](int i) { return output[i]; });
                    std::cout << "Production VCO: fs=" << rate << ", f=" << frequency
                              << ", waveform=" << waveform << ", onset=" << onset << ", bin=" << bin
                              << ", dBc="
                              << 20 * std::log10(std::max(amplitude, 1.0e-15) / fundamental)
                              << '\n';
                }
            }
        }
    }
}
