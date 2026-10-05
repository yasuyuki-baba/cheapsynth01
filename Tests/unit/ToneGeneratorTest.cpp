#include <JuceHeader.h>

#include "CS01AudioProcessor.h"
#include "CS01Synth/MidiProcessor.h"
#include "CS01Synth/ToneGenerator.h"
#include "CS01Synth/WaveformStrategies.h"
#include "mocks/MockToneGenerator.h"

#include <gtest/gtest.h>

#include <chrono>

TEST(ManualPitchTest, HeldWheelUsesUpdatedRange) {
    CS01AudioProcessor host;
    auto& state = host.getValueTreeState();
    auto set = [&](const juce::String& id, float value) {
        auto* p = state.getParameter(id);
        p->setValueNotifyingHost(p->convertTo0to1(value));
    };
    set(ParameterIds::waveType, 2);
    set(ParameterIds::feet, 2);
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        for (int wheel : {0, 16383}) {
            set(ParameterIds::pitchBendUpRange, 12);
            set(ParameterIds::pitchBendDownRange, 12);
            ToneGenerator generator(state);
            generator.prepare({rate, 256, 1});
            generator.startNote(69, 1.0f, wheel);
            generator.updateBlockRateParameters();
            for (float range : {7.0f, 0.0f, 12.0f}) {
                set(wheel == 0 ? ParameterIds::pitchBendDownRange : ParameterIds::pitchBendUpRange,
                    range);
                generator.updateBlockRateParameters();
                for (int i = 0; i < static_cast<int>(rate * 0.02); ++i)
                    generator.getNextSample();
                std::vector<double> crossings;
                double previous = generator.getNextSample();
                for (int i = 0; i < static_cast<int>(rate * 0.1); ++i) {
                    const double value = generator.getNextSample();
                    if (previous < 0 && value >= 0)
                        crossings.push_back(i - 1.0 - previous / (value - previous));
                    previous = value;
                }
                ASSERT_GT(crossings.size(), 2u);
                const double measured =
                    rate * (crossings.size() - 1) / (crossings.back() - crossings.front());
                const double expected = 440.0 * std::exp2((wheel == 0 ? -range : range) / 12.0);
                EXPECT_NEAR(measured, expected, expected * 0.001);
                EXPECT_EQ(generator.getPlaybackState().pitchWheel, wheel);
            }
        }
    }
}

TEST(ManualPitchTest, MeasuredPitchAndMidiBend) {
    CS01AudioProcessor host;
    auto& state = host.getValueTreeState();
    auto set = [&](const juce::String& id, float value) {
        auto* p = state.getParameter(id);
        p->setValueNotifyingHost(p->convertTo0to1(value));
    };
    set(ParameterIds::waveType, 2);
    set(ParameterIds::feet, 2);
    set(ParameterIds::pitchBendUpRange, 12);
    set(ParameterIds::pitchBendDownRange, 12);
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        for (float pitch : {-1.0f, 0.0f, 1.0f}) {
            for (int wheel : {0, 8192, 16383}) {
                set(ParameterIds::pitch, pitch);
                set(ParameterIds::pitchBend, 0);
                ToneGenerator generator(state);
                generator.prepare({rate, 256, 1});
                MidiProcessor midi(state);
                midi.setSoundGenerator(&generator);
                juce::AudioBuffer<float> buffer(1, 1);
                juce::MidiBuffer events;
                events.addEvent(juce::MidiMessage::pitchWheel(1, wheel), 0);
                events.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);
                midi.processBlock(buffer, events);
                generator.updateBlockRateParameters();
                EXPECT_FLOAT_EQ(state.getRawParameterValue(ParameterIds::pitchBend)->load(),
                                wheel == 0 ? -1.0f : (wheel == 16383 ? 1.0f : 0.0f));
                for (int i = 0; i < static_cast<int>(rate * 0.1); ++i)
                    generator.getNextSample();
                std::vector<double> crossings;
                double previous = generator.getNextSample();
                for (int i = 0; i < static_cast<int>(rate * 0.5); ++i) {
                    const double value = generator.getNextSample();
                    ASSERT_TRUE(std::isfinite(value));
                    if (previous < 0 && value >= 0)
                        crossings.push_back(i - previous / (value - previous));
                    previous = value;
                }
                ASSERT_GE(crossings.size(), 3u);
                const double measured =
                    rate * (crossings.size() - 1) / (crossings.back() - crossings.front());
                const double bend = wheel == 0 ? -12.0 : (wheel == 16383 ? 12.0 : 0.0);
                const double expected = 440.0 * std::exp2((pitch + bend) / 12.0);
                EXPECT_NEAR(measured, expected, expected * 0.001);
            }
        }
    }
}

namespace {
struct ProductionVcoHarness {
    juce::AudioProcessorGraph host;
    juce::AudioProcessorValueTreeState state;
    ToneGenerator generator;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout(int waveform) {
        juce::AudioProcessorValueTreeState::ParameterLayout result;
        const auto add = [&](const juce::String& id, float low, float high, float value) {
            result.add(std::make_unique<juce::AudioParameterFloat>(id, id, low, high, value));
        };
        add(ParameterIds::feet, 0, 4, 2);
        add(ParameterIds::waveType, 0, 4, static_cast<float>(waveform));
        add(ParameterIds::pwmSpeed, 0, 60, 0);
        add(ParameterIds::modDepth, 0, 1, 0);
        add(ParameterIds::pitchBend, -12, 12, 0);
        add(ParameterIds::pitch, -12, 12, 0);
        add(ParameterIds::pitchBendUpRange, 0, 24, 2);
        add(ParameterIds::pitchBendDownRange, 0, 24, 2);
        add(ParameterIds::glissando, 0, 0.208f, 0);
        return result;
    }
    ProductionVcoHarness(int waveform, double rate, double frequency)
        : state(host, nullptr, "PARAMETERS", layout(waveform)), generator(state) {
        generator.prepare({rate, 256, 1});
        generator.startNote(69, 1, 8192);
        generator.setPitchBend(static_cast<float>(12 * std::log2(frequency / 440.0)));
        generator.updateBlockRateParameters();
    }
};
}  // namespace

TEST(ToneGeneratorRealTest, RenderBlockPartitionIndependence) {
    for (int waveform = 0; waveform < 5; ++waveform) {
        ProductionVcoHarness scalar(waveform, 48000, 440), blocked(waveform, 48000, 440);
        for (int size : {1, 7, 64, 255, 512}) {
            juce::AudioBuffer<float> buffer(1, size);
            buffer.clear();
            blocked.generator.renderNextBlock(buffer, 0, size);
            for (int i = 0; i < size; ++i)
                EXPECT_NEAR(buffer.getSample(0, i), scalar.generator.getNextSample(), 1.0e-6);
        }
    }
}

TEST(ToneGeneratorRealTest, PanelSwitchesPreserveHeldNoteAndBlockConsistency) {
    // Panel switches are not key-gate events. Do not assert an undocumented
    // hardware phase reset or require click-free switching.
    ProductionVcoHarness scalar(0, 48000, 440), blocked(0, 48000, 440);
    for (int wave : {0, 1, 2, 3, 4, 0, 4, 1, 0}) {
        for (int feet : {2, 0, 3, 1, 2}) {
            for (auto* harness : {&scalar, &blocked}) {
                auto* waveParameter = harness->state.getParameter(ParameterIds::waveType);
                waveParameter->setValueNotifyingHost(
                    waveParameter->convertTo0to1(static_cast<float>(wave)));
                auto* feetParameter = harness->state.getParameter(ParameterIds::feet);
                feetParameter->setValueNotifyingHost(
                    feetParameter->convertTo0to1(static_cast<float>(feet)));
                harness->generator.updateBlockRateParameters();
                EXPECT_TRUE(harness->generator.isActive());
                EXPECT_EQ(harness->generator.getCurrentlyPlayingNote(), 69);
            }
            juce::AudioBuffer<float> output(1, 256);
            output.clear();
            blocked.generator.renderNextBlock(output, 0, 256);
            for (int i = 0; i < 256; ++i) {
                const float expected = scalar.generator.getNextSample();
                ASSERT_TRUE(std::isfinite(expected));
                ASSERT_TRUE(std::isfinite(output.getSample(0, i)));
                EXPECT_NEAR(output.getSample(0, i), expected, 1.0e-6);
            }
        }
    }
}

TEST(ToneGeneratorRealTest, Observation_ProductionProcessingCost) {
    // Observation only: timing is machine/build dependent, not a correctness threshold.
    for (int waveform = 0; waveform < 5; ++waveform) {
        ProductionVcoHarness harness(waveform, 48000, 440);
        double checksum = 0;
        const auto begin = std::chrono::steady_clock::now();
        for (int i = 0; i < 48000; ++i)
            checksum += harness.generator.getNextSample();
        const double seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        EXPECT_TRUE(std::isfinite(checksum));
        std::cout << "VCO cost: waveform=" << waveform << ", secondsPerAudioSecond=" << seconds
                  << '\n';
    }
}

TEST(ToneGeneratorRealTest, GlissandoUsesHostSampleTime) {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        ProductionVcoHarness sliding(2, rate, 440), reference(2, rate, 440);
        auto* parameter = sliding.state.getParameter(ParameterIds::glissando);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(0.02f));
        sliding.generator.changeNote(72);
        const int step = static_cast<int>(0.02f * static_cast<float>(rate));
        double error = 0;
        for (int i = 0; i < step * 4; ++i) {
            // Independent schedule: one semitone per configured host-time interval.
            if ((i + 1) % step == 0 && (i + 1) / step <= 3)
                reference.generator.setNote(69 + (i + 1) / step, false);
            error =
                std::max(error, std::abs(static_cast<double>(sliding.generator.getNextSample()) -
                                         reference.generator.getNextSample()));
        }
        EXPECT_NEAR(error, 0, 1.0e-6);
    }
}

TEST(ToneGeneratorRealTest, LiveGlissandoSpeedPreservesStepProgress) {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        for (float newDuration : {0.0078125f, 0.03125f, 0.0f}) {
            SCOPED_TRACE(rate);
            SCOPED_TRACE(newDuration);
            ProductionVcoHarness sliding(2, rate, 440), reference(2, rate, 440);
            auto* parameter = sliding.state.getParameter(ParameterIds::glissando);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(0.015625f));
            sliding.generator.changeNote(72);
            const int oldStep = static_cast<int>(
                sliding.state.getRawParameterValue(ParameterIds::glissando)->load() *
                static_cast<float>(rate));
            const int changeAt = oldStep / 2;
            // Change halfway through a step: half the new duration remains.
            const float mappedDuration =
                parameter->convertFrom0to1(parameter->convertTo0to1(newDuration));
            const int newStep = static_cast<int>(mappedDuration * static_cast<float>(rate));
            const int remaining =
                newStep - static_cast<int>(static_cast<double>(changeAt) * newStep / oldStep);
            const int firstStep = changeAt + remaining - 1;
            double maximumError = 0.0;
            for (int i = 0; i < static_cast<int>(rate * 0.2); ++i) {
                if (i == changeAt)
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(newDuration));
                if (newDuration == 0.0f) {
                    if (i == changeAt)
                        reference.generator.setNote(72, false);
                } else {
                    for (int step = 0; step < 3; ++step)
                        if (i == firstStep + step * newStep)
                            reference.generator.setNote(70 + step, false);
                }
                const float actual = sliding.generator.getNextSample();
                ASSERT_TRUE(std::isfinite(actual));
                maximumError =
                    std::max(maximumError, std::abs(static_cast<double>(actual) -
                                                    reference.generator.getNextSample()));
            }
            EXPECT_NEAR(maximumError, 0.0, 1.0e-6);
        }
    }
}

TEST(ToneGeneratorRealTest, RepeatedLiveGlissandoUpdatesArePartitionIndependent) {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        for (int blockSize : {1, 7, 64, 256}) {
            SCOPED_TRACE(rate);
            SCOPED_TRACE(blockSize);
            ProductionVcoHarness scalar(2, rate, 440), blocked(2, rate, 440);
            const auto setDuration = [](ProductionVcoHarness& harness, float duration) {
                auto* parameter = harness.state.getParameter(ParameterIds::glissando);
                parameter->setValueNotifyingHost(parameter->convertTo0to1(duration));
            };
            for (auto* harness : {&scalar, &blocked}) {
                setDuration(*harness, 0.1f);
                harness->generator.changeNote(72);
            }
            // Several changes occur before the initial step could complete.
            const float durations[] = {0.08f, 0.12f, 0.04f, 0.0f};
            int offset = 0;
            for (float duration : durations) {
                const int end = offset + static_cast<int>(rate * 0.005);
                while (offset < end) {
                    const int count = juce::jmin(blockSize, end - offset);
                    juce::AudioBuffer<float> output(1, count);
                    output.clear();
                    blocked.generator.renderNextBlock(output, 0, count);
                    for (int i = 0; i < count; ++i) {
                        const float expected = scalar.generator.getNextSample();
                        ASSERT_TRUE(std::isfinite(output.getSample(0, i)));
                        EXPECT_NEAR(output.getSample(0, i), expected, 1.0e-6);
                    }
                    offset += count;
                }
                setDuration(scalar, duration);
                setDuration(blocked, duration);
            }
            // Compare periods after the zero-duration update, not absolute phase.
            int crossings = 0;
            float previous = 0.0f;
            for (int i = 0; i < static_cast<int>(rate); ++i) {
                const float sample = blocked.generator.getNextSample();
                EXPECT_NEAR(sample, scalar.generator.getNextSample(), 1.0e-6);
                if (i > static_cast<int>(rate * 0.1) && previous <= 0.0f && sample > 0.0f)
                    ++crossings;
                previous = sample;
            }
            EXPECT_NEAR(crossings / 0.9, 440.0 * std::exp2(3.0 / 12.0), 2.0);
        }
    }
}

TEST(ToneGeneratorRealTest, PwmPeriodUsesSecondsNotInternalSamples) {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        ProductionVcoHarness harness(4, rate, 480);
        auto* speed = harness.state.getParameter(ParameterIds::pwmSpeed);
        speed->setValueNotifyingHost(speed->convertTo0to1(2.0f));
        harness.generator.prepare({rate, 256, 1});
        harness.generator.startNote(69, 1, 8192);
        harness.generator.setPitchBend(static_cast<float>(12 * std::log2(480.0 / 440.0)));
        harness.generator.updateBlockRateParameters();
        const int count = static_cast<int>(rate);
        std::vector<float> samples(count);
        for (int i = 0; i < count * 2; ++i)
            harness.generator.getNextSample();
        for (auto& value : samples)
            value = harness.generator.getNextSample();
        // Average each carrier cycle to isolate duty modulation from carrier phase.
        std::vector<double> means(480);
        for (int cycle = 0; cycle < 480; ++cycle) {
            const int begin = static_cast<int>(cycle * rate / 480.0);
            const int end = static_cast<int>((cycle + 1) * rate / 480.0);
            for (int i = begin; i < end; ++i)
                means[cycle] += samples[i];
            means[cycle] /= end - begin;
        }
        double repeatPower = 0, quarterPower = 0;
        for (int i = 0; i < 240; ++i) {
            repeatPower += std::pow(means[i] - means[i + 240], 2);
            quarterPower += std::pow(means[i] - means[i + 120], 2);
        }
        // 2 Hz repeats at 0.5 s; a quarter-second shift must not be equivalent.
        EXPECT_LT(repeatPower / 240, 0.001);
        EXPECT_GT(quarterPower / 240, 0.01);
    }
}

TEST(ToneGeneratorRealTest, PwmManualRangePeriods) {
    for (double hostRate : {44100.0, 48000.0, 96000.0}) {
        for (double multiplier : {1.0, static_cast<double>(Constants::oversamplingFactor)}) {
            const double rate = hostRate * multiplier;
            for (float frequency : {0.6f, 6.3f, 12.0f}) {
                SCOPED_TRACE(rate);
                SCOPED_TRACE(frequency);
                ProductionVcoHarness harness(4, rate, 480.0);
                auto* speed = harness.state.getParameter(ParameterIds::pwmSpeed);
                speed->setValueNotifyingHost(speed->convertTo0to1(frequency));
                harness.generator.prepare({rate, 256, 1});
                harness.generator.startNote(69, 1, 8192);
                harness.generator.setPitchBend(static_cast<float>(12 * std::log2(480.0 / 440.0)));
                harness.generator.updateBlockRateParameters();
                for (int i = 0; i < static_cast<int>(rate); ++i)
                    harness.generator.getNextSample();
                std::vector<double> crossings;
                double previous = 0.0;
                bool armed = false;
                int sampleIndex = 0;
                const int cycles = static_cast<int>(480.0 * 4.0 / frequency);
                for (int cycle = 0; cycle < cycles; ++cycle) {
                    const int end = static_cast<int>((cycle + 1) * rate / 480.0);
                    double mean = 0.0;
                    const int count = end - sampleIndex;
                    while (sampleIndex < end) {
                        mean += harness.generator.getNextSample();
                        ++sampleIndex;
                    }
                    mean /= count;
                    ASSERT_TRUE(std::isfinite(mean));
                    if (mean < -0.1)
                        armed = true;
                    if (armed && cycle > 0 && previous < 0.0 && mean >= 0.0) {
                        crossings.push_back(cycle - 1 + (-previous / (mean - previous)));
                        armed = false;
                    }
                    previous = mean;
                }
                ASSERT_GE(crossings.size(), 3u);
                const double measured =
                    480.0 * (crossings.size() - 1) / (crossings.back() - crossings.front());
                // Carrier-cycle averaging limits temporal resolution; verify 0.5%.
                EXPECT_NEAR(measured, frequency, frequency * 0.005);
            }
        }
    }
}

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
        const double phase = juce::MathConstants<double>::twoPi * frequency * i / rate;
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

TEST(VcoSpectrumTest, TimeConstantsPreserveDecayPerSecond) {
    WaveformTimeConstants constants;
    for (float rate : {44100.0f, 48000.0f, 96000.0f}) {
        constants.update(rate);
        const auto check = [&](double actual, double reference) {
            // Compare log decay per second, independently of waveform output.
            EXPECT_NEAR(std::log(actual) * rate, std::log(reference) * 44100.0, 0.006);
        };
        check(constants.triangleLeak, static_cast<double>(0.9999f));
        check(1.0 - constants.triangleDcAmount, 1.0 - static_cast<double>(0.005f));
        check(constants.sawLeak, static_cast<double>(0.998f));
        check(constants.pwmPole, static_cast<double>(0.98f));
    }
    constants.update(44100.0f);
    EXPECT_FLOAT_EQ(constants.triangleLeak, 0.9999f);
    EXPECT_FLOAT_EQ(constants.triangleDcAmount, 0.005f);
    EXPECT_FLOAT_EQ(constants.sawLeak, 0.998f);
    EXPECT_FLOAT_EQ(constants.pwmPole, 0.98f);
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
        const double phase = juce::MathConstants<double>::twoPi * frequency * i / rate;
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
        return 0.5 * std::sin(juce::MathConstants<double>::twoPi * 5000.0 * i / 48000.0) +
               0.1 * std::sin(juce::MathConstants<double>::twoPi * 25000.0 * i / 48000.0);
    };
    EXPECT_NEAR(measureSpectralAmplitude(48000.0, 5000.0, signal), 0.5, 1.0e-9);
    EXPECT_NEAR(measureSpectralAmplitude(48000.0, 23000.0, signal), 0.1, 1.0e-9);
    EXPECT_NEAR(measureSpectralAmplitude(48000.0, 3000.0, signal), 0.0, 1.0e-9);
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

TEST(VcoSpectrumTest, HighFrequencyAliasCharacterization) {
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
                juce::dsp::Oscillator<double> pwm;
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

TEST(VcoSpectrumTest, SquareAliasProcessingStageCharacterization) {
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

TEST(VcoSpectrumTest, FourTimesNonlinearOversamplingComparison) {
    for (double rate : {44100.0, 48000.0}) {
        for (bool sineControl : {true, false}) {
            std::vector<float> outputs[3];
            for (int mode = 0; mode < 3; ++mode) {
                juce::dsp::Oversampling<float> oversampling(
                    1, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple);
                oversampling.initProcessing(256);
                juce::AudioBuffer<float> buffer(1, 256);
                const int count = static_cast<int>(rate * 3.0);
                outputs[mode].reserve(count);
                for (int offset = 0; offset < count; offset += 256) {
                    for (int i = 0; i < 256; ++i) {
                        const double position = (offset + i) * 5000.0 / rate;
                        float value;
                        if (sineControl) {
                            value = static_cast<float>(
                                0.5 * std::sin(juce::MathConstants<double>::twoPi * position));
                        } else {
                            const float phase = static_cast<float>(std::fmod(position, 1.0));
                            const float increment = static_cast<float>(5000.0 / rate);
                            value = phase < 0.5f ? 1.0f : -1.0f;
                            value += poly_blep(phase, increment);
                            value -= poly_blep(std::fmod(phase + 0.5f, 1.0f), increment);
                        }
                        buffer.setSample(0, i, value);
                    }
                    juce::dsp::AudioBlock<float> block(buffer);
                    const auto saturate = [&](float value) {
                        return sineControl ? value : std::tanh(std::tanh(value * 1.2f) * 1.2f);
                    };
                    if (mode >= 1) {
                        auto up = oversampling.processSamplesUp(block);
                        auto* data = up.getChannelPointer(0);
                        for (size_t i = 0; i < up.getNumSamples(); ++i) {
                            if (mode == 2) {
                                // Replace the interpolated input with direct 4x generation.
                                // The upsampling call only obtains JUCE's internal buffer;
                                // its contents are overwritten before downsampling.
                                const double position = (offset * 4.0 + i) * 5000.0 / (rate * 4.0);
                                if (sineControl) {
                                    data[i] = static_cast<float>(
                                        0.5 *
                                        std::sin(juce::MathConstants<double>::twoPi * position));
                                } else {
                                    const float phase =
                                        static_cast<float>(std::fmod(position, 1.0));
                                    const float increment =
                                        static_cast<float>(5000.0 / (rate * 4.0));
                                    data[i] = phase < 0.5f ? 1.0f : -1.0f;
                                    data[i] += poly_blep(phase, increment);
                                    data[i] -= poly_blep(std::fmod(phase + 0.5f, 1.0f), increment);
                                }
                            }
                            data[i] = saturate(data[i]);
                        }
                        oversampling.processSamplesDown(block);
                    } else {
                        for (int i = 0; i < 256; ++i)
                            buffer.setSample(0, i, saturate(buffer.getSample(0, i)));
                    }
                    for (int i = 0; i < 256 && offset + i < count; ++i)
                        outputs[mode].push_back(buffer.getSample(0, i));
                }
            }
            double fundamental[3]{};
            for (int mode = 0; mode < 3; ++mode) {
                fundamental[mode] =
                    measureSpectralAmplitude(rate, 5000.0, [&](int i) { return outputs[mode][i]; });
                ASSERT_GT(fundamental[mode], 0.0);
                if (sineControl) {
                    EXPECT_NEAR(fundamental[mode], 0.5, 0.001);
                }
            }
            for (int harmonic : {5, 7, 9}) {
                const double wrapped = std::fmod(5000.0 * harmonic, rate);
                const double bin = std::min(wrapped, rate - wrapped);
                double amplitude[3]{};
                for (int mode = 0; mode < 3; ++mode)
                    amplitude[mode] = measureSpectralAmplitude(
                        rate, bin, [&](int i) { return outputs[mode][i]; });
                if (sineControl) {
                    EXPECT_LT(amplitude[1], 1.0e-5);
                    EXPECT_LT(amplitude[2], 1.0e-5);
                } else {
                    std::cout << "VCO nonlinear oversampling: fs=" << rate << ", bin=" << bin
                              << ", 1x=" << 20.0 * std::log10(amplitude[0] / fundamental[0])
                              << ", 4x=" << 20.0 * std::log10(amplitude[1] / fundamental[1])
                              << ", generated4x="
                              << 20.0 * std::log10(std::max(amplitude[2], 1.0e-15) / fundamental[2])
                              << " dBc, fundamentalChange="
                              << 20.0 * std::log10(fundamental[1] / fundamental[0])
                              << ", generatedChange="
                              << 20.0 * std::log10(fundamental[2] / fundamental[0]) << " dB\n";
                }
            }
        }
    }
}

TEST(VcoSpectrumTest, WaveformStrategySampleRateCharacterization) {
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
                juce::dsp::Oscillator<double> pwm;
                pwm.initialise([](float x) {
                    return std::asin(std::sin(x)) * 2.0f / juce::MathConstants<float>::pi;
                });
                pwm.prepare({rate, 256, 1});
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

TEST(ToneGeneratorRealTest, PitchAndFeetFromMeasuredPeriods) {
    // Exercise the production generator, not MockToneGenerator.
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        for (int feet = 0; feet < 4; ++feet) {
            for (int waveform = 0; waveform < 5; ++waveform) {
                for (int note : {45, 69, 93}) {
                    juce::AudioProcessorGraph host;
                    juce::AudioProcessorValueTreeState::ParameterLayout layout;
                    const auto add = [&](const juce::String& id, float low, float high,
                                         float value) {
                        layout.add(
                            std::make_unique<juce::AudioParameterFloat>(id, id, low, high, value));
                    };
                    add(ParameterIds::feet, 0.0f, 4.0f, static_cast<float>(feet));
                    add(ParameterIds::waveType, 0.0f, 4.0f, static_cast<float>(waveform));
                    add(ParameterIds::pwmSpeed, 0.0f, 60.0f, 0.0f);
                    add(ParameterIds::modDepth, 0.0f, 1.0f, 0.0f);
                    add(ParameterIds::pitchBend, -12.0f, 12.0f, 0.0f);
                    add(ParameterIds::pitch, -12.0f, 12.0f, 0.0f);
                    add(ParameterIds::pitchBendUpRange, 0.0f, 24.0f, 2.0f);
                    add(ParameterIds::pitchBendDownRange, 0.0f, 24.0f, 2.0f);
                    juce::AudioProcessorValueTreeState state(host, nullptr, "PARAMETERS",
                                                             std::move(layout));
                    ToneGenerator generator(state);
                    generator.prepare({sampleRate, 256, 1});
                    generator.startNote(note, 1.0f, 8192);
                    generator.setPitchBend(0.0f);  // Isolate tuning from MIDI wheel quantization.
                    generator.updateBlockRateParameters();
                    SCOPED_TRACE(sampleRate);
                    SCOPED_TRACE(feet);
                    SCOPED_TRACE(waveform);
                    SCOPED_TRACE(note);
                    const double expected = 440.0 * std::pow(2.0, (note - 69) / 12.0 + feet - 2);
                    // Allow output coupling to settle, then measure 64 complete periods.
                    // Retain a bounded timeout for silent or incorrectly tuned output.
                    const int settlingSamples = static_cast<int>(sampleRate * 0.1);
                    constexpr int requiredCrossings = 65;
                    double first = 0.0, last = 0.0;
                    int crossings = 0;
                    float previous = generator.getNextSample();
                    for (int i = 1; i < static_cast<int>(sampleRate * 4.0); ++i) {
                        const float value = generator.getNextSample();
                        ASSERT_TRUE(std::isfinite(value));
                        if (i >= settlingSamples && previous <= 0.0f && value > 0.0f) {
                            const double position =
                                i - 1 + (-previous / static_cast<double>(value - previous));
                            if (crossings == 0)
                                first = position;
                            last = position;
                            ++crossings;
                            if (crossings == requiredCrossings)
                                break;
                        }
                        previous = value;
                    }
                    ASSERT_EQ(crossings, requiredCrossings);
                    const double measured = (crossings - 1) * sampleRate / (last - first);
                    EXPECT_NEAR(measured, expected, expected * 0.001);
                    std::cout << "VCO pitch: fs=" << sampleRate << ", feetIndex=" << feet
                              << ", measured=" << measured << ", expected=" << expected << " Hz\n";
                }
            }
        }
    }
}

TEST(ToneGeneratorRealTest, ResetReproducesFreshWaveform) {
    for (int waveform = 0; waveform < 5; ++waveform) {
        juce::AudioProcessorGraph host;
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        const auto add = [&](const juce::String& id, float low, float high, float value) {
            layout.add(std::make_unique<juce::AudioParameterFloat>(id, id, low, high, value));
        };
        add(ParameterIds::feet, 0.0f, 4.0f, 2.0f);
        add(ParameterIds::waveType, 0.0f, 4.0f, static_cast<float>(waveform));
        add(ParameterIds::pwmSpeed, 0.0f, 60.0f, 0.0f);
        add(ParameterIds::modDepth, 0.0f, 1.0f, 0.0f);
        add(ParameterIds::pitchBend, -12.0f, 12.0f, 0.0f);
        add(ParameterIds::pitch, -12.0f, 12.0f, 0.0f);
        add(ParameterIds::pitchBendUpRange, 0.0f, 24.0f, 2.0f);
        add(ParameterIds::pitchBendDownRange, 0.0f, 24.0f, 2.0f);
        juce::AudioProcessorValueTreeState state(host, nullptr, "PARAMETERS", std::move(layout));
        ToneGenerator reference(state), reused(state);
        const auto start = [](ToneGenerator& generator) {
            generator.prepare({48000.0, 256, 1});
            generator.startNote(69, 1.0f, 8192);
            generator.setPitchBend(0.0f);
            generator.updateBlockRateParameters();
        };
        start(reference);
        start(reused);
        for (int i = 0; i < 12345; ++i)
            reused.getNextSample();
        start(reused);
        double maximumError = 0.0;
        for (int i = 0; i < 4096; ++i)
            maximumError =
                std::max(maximumError, std::abs(static_cast<double>(reference.getNextSample()) -
                                                reused.getNextSample()));
        SCOPED_TRACE(waveform);
        EXPECT_NEAR(maximumError, 0.0, 1.0e-6);
    }
}

// Helper class to manage APVTS and processor lifecycle
class APVTSHolder {
   public:
    APVTSHolder() {
        // Create a dummy processor
        dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();

        // Create parameter layout
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add(std::make_unique<juce::AudioParameterFloat>("vco_waveform", "Waveform", 0.0f,
                                                               4.0f, 0.0f));
        layout.add(
            std::make_unique<juce::AudioParameterFloat>("vco_octave", "Octave", -2.0f, 2.0f, 0.0f));

        // Create APVTS
        apvts = std::make_unique<juce::AudioProcessorValueTreeState>(
            *dummyProcessor, nullptr, "Parameters", std::move(layout));
    }

    ~APVTSHolder() = default;

    juce::AudioProcessorValueTreeState& getAPVTS() {
        return *apvts;
    }

   private:
    std::unique_ptr<juce::AudioProcessorGraph> dummyProcessor;
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
};

// Test fixture class for ToneGenerator tests
class ToneGeneratorTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Set up test environment
        apvtsHolder = std::make_unique<APVTSHolder>();
        toneGenerator = std::make_unique<testing::MockToneGenerator>(apvtsHolder->getAPVTS());

        // Prepare the tone generator
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        toneGenerator->prepare(spec);
    }

    void TearDown() override {
        toneGenerator.reset();
        apvtsHolder.reset();
    }

    std::unique_ptr<APVTSHolder> apvtsHolder;
    std::unique_ptr<testing::MockToneGenerator> toneGenerator;
};

TEST_F(ToneGeneratorTest, Initialization) {
    // Check initial state
    EXPECT_FALSE(toneGenerator->isActive());
    EXPECT_EQ(toneGenerator->getCurrentlyPlayingNote(), 0);

    // Check that prepare doesn't crash
    EXPECT_TRUE(true);
}

TEST_F(ToneGeneratorTest, NoteOnOff) {
    // Check initial state
    EXPECT_FALSE(toneGenerator->isActive());
    EXPECT_EQ(toneGenerator->getCurrentlyPlayingNote(), 0);

    // Start a note
    toneGenerator->startNote(60, 1.0f, 8192);

    // Check that note is active
    EXPECT_TRUE(toneGenerator->isActive());
    EXPECT_EQ(toneGenerator->getCurrentlyPlayingNote(), 60);

    // Get a sample and check it's non-zero
    float sample = toneGenerator->getNextSample();
    EXPECT_GT(std::abs(sample), 0.0f);

    // Stop the note without tail off
    toneGenerator->stopNote(false);

    // Check that note is no longer active
    EXPECT_FALSE(toneGenerator->isActive());
    EXPECT_EQ(toneGenerator->getCurrentlyPlayingNote(), 0);

    // Get a sample and check it's zero
    sample = toneGenerator->getNextSample();
    EXPECT_NEAR(sample, 0.0f, 0.0001f);

    // Start a note again
    toneGenerator->startNote(60, 1.0f, 8192);

    // Check that note is active
    EXPECT_TRUE(toneGenerator->isActive());

    // Stop the note with tail off
    toneGenerator->stopNote(true);

    // Check that note is still active during tail off
    EXPECT_TRUE(toneGenerator->isActive());

    // Get a sample and check it's non-zero (tail off)
    sample = toneGenerator->getNextSample();
    EXPECT_GT(std::abs(sample), 0.0f);
}

TEST_F(ToneGeneratorTest, PitchBend) {
    // Start a note with center pitch bend
    toneGenerator->startNote(60, 1.0f, 8192);

    // Get samples with center pitch bend
    float sample1 = toneGenerator->getNextSample();
    float sample2 = toneGenerator->getNextSample();

    // Apply pitch bend up
    toneGenerator->pitchWheelMoved(16383);  // Maximum pitch bend up

    // Get samples with pitch bend up
    float sample3 = toneGenerator->getNextSample();
    float sample4 = toneGenerator->getNextSample();

    // Check that the samples are different (frequency has changed)
    EXPECT_TRUE(std::abs(sample1 - sample3) > 0.0001f || std::abs(sample2 - sample4) > 0.0001f);

    // Apply center pitch bend
    toneGenerator->pitchWheelMoved(8192);

    // Get samples with center pitch bend
    float sample5 = toneGenerator->getNextSample();
    float sample6 = toneGenerator->getNextSample();

    // Apply pitch bend down
    toneGenerator->pitchWheelMoved(0);  // Maximum pitch bend down

    // Get samples with pitch bend down
    float sample7 = toneGenerator->getNextSample();
    float sample8 = toneGenerator->getNextSample();

    // Check that the samples are different (frequency has changed)
    EXPECT_TRUE(std::abs(sample5 - sample7) > 0.0001f || std::abs(sample6 - sample8) > 0.0001f);
}

TEST_F(ToneGeneratorTest, WaveformGeneration) {
    // Start a note
    toneGenerator->startNote(60, 1.0f, 8192);

    // Set waveform to saw
    apvtsHolder->getAPVTS().getParameter("vco_waveform")->setValueNotifyingHost(0.0f);

    // Get samples with saw waveform
    float sawSample1 = toneGenerator->getNextSample();
    float sawSample2 = toneGenerator->getNextSample();

    // Set waveform to square
    apvtsHolder->getAPVTS().getParameter("vco_waveform")->setValueNotifyingHost(0.25f);

    // Get samples with square waveform
    float squareSample1 = toneGenerator->getNextSample();
    float squareSample2 = toneGenerator->getNextSample();

    // Check that the samples are different (waveform has changed)
    EXPECT_TRUE(std::abs(sawSample1 - squareSample1) > 0.0001f ||
                std::abs(sawSample2 - squareSample2) > 0.0001f);

    // Set waveform to triangle
    apvtsHolder->getAPVTS().getParameter("vco_waveform")->setValueNotifyingHost(0.5f);

    // Get samples with triangle waveform
    float triangleSample1 = toneGenerator->getNextSample();
    float triangleSample2 = toneGenerator->getNextSample();

    // Check that the samples are different (waveform has changed)
    EXPECT_TRUE(std::abs(squareSample1 - triangleSample1) > 0.0001f ||
                std::abs(squareSample2 - triangleSample2) > 0.0001f);
}

TEST_F(ToneGeneratorTest, Glissando) {
    // Enable glissando
    toneGenerator->setGlissando(true);

    // Start a note
    toneGenerator->startNote(60, 1.0f, 8192);

    // Get samples with initial note
    float sample1 = toneGenerator->getNextSample();
    float sample2 = toneGenerator->getNextSample();

    // Start a new note (should trigger glissando)
    toneGenerator->startNote(72, 1.0f, 8192);

    // Get samples during glissando
    float sample3 = toneGenerator->getNextSample();
    float sample4 = toneGenerator->getNextSample();

    // Check that the samples are different (frequency is changing)
    EXPECT_TRUE(std::abs(sample1 - sample3) > 0.0001f || std::abs(sample2 - sample4) > 0.0001f);

    // Disable glissando
    toneGenerator->setGlissando(false);

    // Start a note
    toneGenerator->startNote(60, 1.0f, 8192);

    // Get samples with initial note
    sample1 = toneGenerator->getNextSample();
    sample2 = toneGenerator->getNextSample();

    // Start a new note (should not trigger glissando)
    toneGenerator->startNote(72, 1.0f, 8192);

    // Get samples with new note
    sample3 = toneGenerator->getNextSample();
    sample4 = toneGenerator->getNextSample();

    // Check that the samples are different (frequency has changed immediately)
    EXPECT_TRUE(std::abs(sample1 - sample3) > 0.0001f || std::abs(sample2 - sample4) > 0.0001f);
}

TEST_F(ToneGeneratorTest, LfoModulation) {
    // Start a note
    toneGenerator->startNote(60, 1.0f, 8192);

    // Get samples without LFO modulation
    float sample1 = toneGenerator->getNextSample();
    float sample2 = toneGenerator->getNextSample();

    // Apply LFO modulation
    toneGenerator->setLfoModulation(1.0f);

    // Get samples with LFO modulation
    float sample3 = toneGenerator->getNextSample();
    float sample4 = toneGenerator->getNextSample();

    // Check that the samples are different (frequency has changed)
    EXPECT_TRUE(std::abs(sample1 - sample3) > 0.0001f || std::abs(sample2 - sample4) > 0.0001f);

    // Remove LFO modulation
    toneGenerator->setLfoModulation(0.0f);

    // Get samples without LFO modulation
    float sample5 = toneGenerator->getNextSample();
    float sample6 = toneGenerator->getNextSample();

    // Check that the samples are different (frequency has changed back)
    EXPECT_TRUE(std::abs(sample3 - sample5) > 0.0001f || std::abs(sample4 - sample6) > 0.0001f);
}
