#include <JuceHeader.h>

#include "CS01AudioProcessor.h"
#include "CS01Synth/ToneGenerator.h"
#include "reference/ToneBeforeOptimization.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <vector>

namespace {
void control(CS01AudioProcessor& owner, const juce::String& id, float value) {
    auto* parameter = owner.apvts.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void originalVcoBlock(BeforeVcoTone& generator, juce::AudioBuffer<float>& buffer, float depth) {
    generator.updateBlockRateParameters();
    auto* output = buffer.getWritePointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        generator.setLfoValue(output[i] * depth * 1.0f);
        output[i] = 0.0f;
        if (generator.isActive())
            generator.renderNextBlock(buffer, i, 1);
    }
}

void fillModulation(juce::AudioBuffer<float>& buffer, int offset) {
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        buffer.setSample(0, i, 0.7f * static_cast<float>(std::sin((offset + i) * 0.019)));
        for (int channel = 1; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, i, (offset + i) % 13 == 0 ? -0.0f : -0.03f);
    }
}

void sameSamples(const juce::AudioBuffer<float>& actual, const juce::AudioBuffer<float>& before,
                 int offset) {
    for (int channel = 0; channel < actual.getNumChannels(); ++channel)
        for (int i = 0; i < actual.getNumSamples(); ++i) {
            ASSERT_TRUE(std::isfinite(actual.getSample(channel, i)));
            ASSERT_EQ(std::bit_cast<uint32_t>(actual.getSample(channel, i)),
                      std::bit_cast<uint32_t>(before.getSample(channel, i)))
                << "channel=" << channel << " sample=" << offset + i;
        }
}
}  // namespace

TEST(VcoOptimizationTest, ModulatedBlocksMatchFrozenSingleSampleRendererExactly) {
    CS01AudioProcessor owner;
    const std::vector<int> events{0,    257,  511,  769,  1025, 1281, 1537,
                                  1793, 2305, 2817, 3331, 3500, 4096};
    for (bool external : {false, true})
        for (double hostRate : {44100.0, 48000.0, 96000.0})
            for (int waveform = 0; waveform < 5; ++waveform)
                for (int feet = 0; feet < 4; ++feet)
                    for (int partition : {1, 7, 64, 255}) {
                        SCOPED_TRACE(::testing::Message()
                                     << external << "/" << hostRate << "/" << waveform << "/"
                                     << feet << "/" << partition);
                        control(owner, ParameterIds::waveType, static_cast<float>(waveform));
                        control(owner, ParameterIds::feet, static_cast<float>(feet));
                        control(owner, ParameterIds::pwmSpeed, 2.5f);
                        control(owner, ParameterIds::pitch, 0.0f);
                        control(owner, ParameterIds::glissando, 0.006f);
                        control(owner, ParameterIds::release, 0.02f);
                        control(owner, ParameterIds::pitchBendUpRange, 12);
                        control(owner, ParameterIds::pitchBendDownRange, 5);
                        ToneGenerator actual(owner.apvts);
                        BeforeVcoTone before(owner.apvts);
                        actual.setExternalOversampling(external);
                        before.setExternalOversampling(external);
                        const double rate = hostRate * (external ? 4 : 1);
                        const juce::dsp::ProcessSpec spec{rate, 256, 1};
                        actual.prepare(spec);
                        before.prepare(spec);
                        actual.startNote(60, 1, 8192);
                        before.startNote(60, 1, 8192);
                        for (size_t event = 0; event + 1 < events.size(); ++event) {
                            const int start = events[event];
                            if (start == 257) {
                                control(owner, ParameterIds::pitch, 0.3f);
                                actual.pitchWheelMoved(14000);
                                before.pitchWheelMoved(14000);
                            } else if (start == 511) {
                                control(owner, ParameterIds::pwmSpeed, 9.99f);
                                control(owner, ParameterIds::waveType,
                                        static_cast<float>((waveform + 1) % 5));
                            } else if (start == 769) {
                                actual.changeNote(76);
                                before.changeNote(76);
                            } else if (start == 1025) {
                                control(owner, ParameterIds::glissando, 0.003f);
                            } else if (start == 1281) {
                                control(owner, ParameterIds::pitchBendUpRange, 3);
                                control(owner, ParameterIds::pitchBendDownRange, 7);
                            } else if (start == 1537) {
                                actual.stopNote(true);
                                before.stopNote(true);
                            } else if (start == 1793) {
                                control(owner, ParameterIds::release, 0.002f);
                            } else if (start == 2305) {
                                actual.startNote(45, 1, 0);
                                before.startNote(45, 1, 0);
                            } else if (start == 2817) {
                                actual.stopNote(true);
                                before.stopNote(true);
                                actual.setReleaseSamplesRemaining(97);
                                before.setReleaseSamplesRemaining(97);
                            } else if (start == 3331) {
                                actual.startNote(72, 1, 16383);
                                before.startNote(72, 1, 16383);
                                actual.changeNote(52);
                                before.changeNote(52);
                            } else if (start == 3500) {
                                actual.reset();
                                before.reset();
                                actual.startNote(58, 1, 8192);
                                before.startNote(58, 1, 8192);
                            }
                            for (int offset = start; offset < events[event + 1];) {
                                const int size = std::min(partition, events[event + 1] - offset);
                                juce::AudioBuffer<float> output(2, size), reference(2, size);
                                fillModulation(output, offset);
                                reference.makeCopyOf(output);
                                actual.renderModulatedBlock(output, output.getReadPointer(0),
                                                            0.73f);
                                originalVcoBlock(before, reference, 0.73f);
                                sameSamples(output, reference, offset);
                                ASSERT_EQ(actual.isActive(), before.isActive());
                                EXPECT_EQ(actual.getCurrentlyPlayingNote(),
                                          before.getCurrentlyPlayingNote());
                                EXPECT_DOUBLE_EQ(actual.getPlaybackState().releaseSecondsRemaining,
                                                 before.getPlaybackState().releaseSecondsRemaining);
                                offset += size;
                            }
                        }
                    }
}

TEST(VcoOptimizationTest, CachedBindingsSurvivePresetAndSessionStateReplacement) {
    CS01AudioProcessor owner;
    ToneGenerator actual(owner.apvts);
    BeforeVcoTone before(owner.apvts);
    actual.setExternalOversampling(true);
    before.setExternalOversampling(true);
    actual.prepare({192000, 256, 1});
    before.prepare({192000, 256, 1});
    actual.startNote(69, 1, 14000);
    before.startNote(69, 1, 14000);
    juce::MemoryBlock session;
    owner.getStateInformation(session);
    for (int preset = 0; preset < 8; ++preset) {
        if (preset < 7)
            owner.getPresetManager().setCurrentProgram(preset);
        else
            owner.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
        juce::AudioBuffer<float> output(1, 255), reference(1, 255);
        fillModulation(output, preset * 255);
        reference.makeCopyOf(output);
        actual.renderModulatedBlock(output, output.getReadPointer(0), 0.83f);
        originalVcoBlock(before, reference, 0.83f);
        sameSamples(output, reference, preset * 255);
    }
}

TEST(VcoOptimizationObservationTest, Observation_PairedVcoCostWithPwmLfoAndLiveGlissando) {
    CS01AudioProcessor owner;
    for (bool sliding : {false, true})
        for (int hostBlock : {1, 16, 64}) {
            control(owner, ParameterIds::waveType, 4);
            control(owner, ParameterIds::feet, 2);
            control(owner, ParameterIds::pitch, 0);
            control(owner, ParameterIds::pwmSpeed, 12);
            control(owner, ParameterIds::glissando, sliding ? 0.006f : 0.0f);
            ToneGenerator actual(owner.apvts);
            BeforeVcoTone before(owner.apvts);
            actual.setExternalOversampling(true);
            before.setExternalOversampling(true);
            actual.prepare({192000, 256, 1});
            before.prepare({192000, 256, 1});
            actual.startNote(60, 1, 8192);
            before.startNote(60, 1, 8192);
            juce::AudioBuffer<float> output(1, hostBlock * 4), reference(1, hostBlock * 4);
            std::vector<double> oldCosts, newCosts;
            for (int trial = -32; trial < 1000; ++trial) {
                if (sliding && trial >= 0) {
                    control(owner, ParameterIds::glissando, 0.004f + (trial % 3) * 0.002f);
                    if (trial % 16 == 0) {
                        actual.changeNote((trial / 16) % 2 ? 60 : 84);
                        before.changeNote((trial / 16) % 2 ? 60 : 84);
                    }
                }
                fillModulation(output, (trial + 32) * hostBlock * 4);
                reference.makeCopyOf(output);
                const auto measure = [](auto&& render) {
                    const auto begin = std::chrono::steady_clock::now();
                    render();
                    return std::chrono::duration<double, std::micro>(
                               std::chrono::steady_clock::now() - begin)
                        .count();
                };
                double oldCost, newCost;
                const auto original = [&] { originalVcoBlock(before, reference, 0.73f); };
                const auto optimized = [&] {
                    actual.renderModulatedBlock(output, output.getReadPointer(0), 0.73f);
                };
                if (trial % 2 == 0) {
                    oldCost = measure(original);
                    newCost = measure(optimized);
                } else {
                    newCost = measure(optimized);
                    oldCost = measure(original);
                }
                sameSamples(output, reference, trial);
                if (trial >= 0) {
                    oldCosts.push_back(oldCost);
                    newCosts.push_back(newCost);
                }
            }
            std::sort(oldCosts.begin(), oldCosts.end());
            std::sort(newCosts.begin(), newCosts.end());
            std::cout << "vco-paired gliss=" << sliding << " host_block=" << hostBlock
                      << " n=1000 old_median_us=" << oldCosts[499]
                      << " new_median_us=" << newCosts[499] << " old_p99_us=" << oldCosts[989]
                      << " new_p99_us=" << newCosts[989] << " old_max_us=" << oldCosts.back()
                      << " new_max_us=" << newCosts.back() << " (observation, not a deadline)\n";
        }
}
