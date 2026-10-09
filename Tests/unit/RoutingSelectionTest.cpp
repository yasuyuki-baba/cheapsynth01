#include <JuceHeader.h>

#include "CS01AudioProcessor.h"
#include "CS01Synth/EGProcessor.h"
#include "CS01Synth/VCOProcessor.h"
#include "Parameters.h"
#include "RealtimeAudit.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

TEST(RoutingSelectionTest, ChoiceChangesApplyOnAudioWithoutMessageLoopIncludingEmptyBlock) {
    CS01AudioProcessor p;
    p.prepareToPlay(48000, 64);
    juce::AudioBuffer<float> audio(2, 64);
    juce::MidiBuffer midi;
    std::thread worker([&] {
        p.apvts.getParameter(ParameterIds::filterType)->setValueNotifyingHost(1);
        p.apvts.getParameter(ParameterIds::lfoTarget)->setValueNotifyingHost(1);
    });
    worker.join();
    p.processBlock(audio, midi);
    EXPECT_EQ(p.getAppliedFilterTypeForTesting(), 1);
    EXPECT_EQ(p.getAppliedLfoTargetForTesting(), 1);
    p.apvts.getParameter(ParameterIds::filterType)->setValueNotifyingHost(0);
    p.apvts.getParameter(ParameterIds::lfoTarget)->setValueNotifyingHost(0);
    audio.setSize(2, 0);
    p.processBlock(audio, midi);
    EXPECT_EQ(p.getAppliedFilterTypeForTesting(), 0);
    EXPECT_EQ(p.getAppliedLfoTargetForTesting(), 0);
}

namespace {
void choice(CS01AudioProcessor& p, const juce::String& id, int value) {
    auto* parameter = p.apvts.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(value)));
}
void scalar(CS01AudioProcessor& p, const juce::String& id, float value) {
    auto* parameter = p.apvts.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void initialise(CS01AudioProcessor& p, int filter, int target, double rate, int size) {
    choice(p, ParameterIds::filterType, filter);
    choice(p, ParameterIds::lfoTarget, target);
    scalar(p, ParameterIds::modDepth, 0.7f);
    scalar(p, ParameterIds::lfoSpeed, 6.0f);
    scalar(p, ParameterIds::release, 0.3f);
    p.prepareToPlay(rate, size);
}
}  // namespace

TEST(RoutingSelectionTest, FixedAndLegacyGraphsMatchForEveryStationaryRoute) {
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int filter : {0, 1})
            for (int target : {0, 1})
                for (int size : {1, 7, 64}) {
                    SCOPED_TRACE(::testing::Message()
                                 << rate << "/" << filter << "/" << target << "/" << size);
                    CS01AudioProcessor fixed, legacy;
                    legacy.useLegacyRoutingForTesting();
                    initialise(fixed, filter, target, rate, 64);
                    initialise(legacy, filter, target, rate, 64);
                    juce::AudioBuffer<float> a(2, size), b(2, size);
                    juce::MidiBuffer am, bm;
                    am.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
                    bm.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
                    for (int offset = 0; offset < 2048; offset += size) {
                        fixed.processBlock(a, am);
                        legacy.processBlock(b, bm);
                        for (int ch = 0; ch < 2; ++ch)
                            for (int i = 0; i < size; ++i) {
                                ASSERT_TRUE(std::isfinite(a.getSample(ch, i)));
                                ASSERT_EQ(a.getSample(ch, i), b.getSample(ch, i));
                            }
                    }
                }
}

TEST(RoutingSelectionTest, SwitchingDuringReleaseKeepsVoiceAndConnectionsAcrossPartitions) {
    for (int size : {1, 7, 64}) {
        CS01AudioProcessor fixed;
        initialise(fixed, 0, 0, 48000, 64);
        const auto connections = fixed.getAudioGraphForTesting().getConnections();
        auto* vco = static_cast<VCOProcessor*>(fixed.getAudioGraphForTesting()
                                                   .getNodeForId(fixed.getVcoNodeIdForTesting())
                                                   ->getProcessor());
        EGProcessor* envelope = nullptr;
        for (auto* node : fixed.getAudioGraphForTesting().getNodes())
            if (auto* eg = dynamic_cast<EGProcessor*>(node->getProcessor()))
                envelope = eg;
        ASSERT_NE(envelope, nullptr);
        juce::AudioBuffer<float> audio(2, size);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
        for (int frame = 0; frame < 24000; frame += size) {
            const int filter = (frame / 127) % 2;
            const int target = (frame / 251) % 2;
            choice(fixed, ParameterIds::filterType, filter);
            choice(fixed, ParameterIds::lfoTarget, target);
            if (frame >= 2000 && frame - size < 2000)
                midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
            if (frame >= 4000 && frame - size < 4000) {
                scalar(fixed, ParameterIds::release, 0.5f);
                choice(fixed, ParameterIds::feet, 4);  // Noise during release.
            }
            if (frame >= 6000 && frame - size < 6000)
                choice(fixed, ParameterIds::feet, 0);
            if (frame >= 10000 && frame - size < 10000)
                scalar(fixed, ParameterIds::release, 0.05f);
            fixed.processBlock(audio, midi);
            ASSERT_EQ(vco->getSoundGenerator()->isActive(), envelope->isActive());
            ASSERT_EQ(fixed.getAppliedFilterTypeForTesting(), filter);
            ASSERT_EQ(fixed.getAppliedLfoTargetForTesting(), target);
            ASSERT_EQ(fixed.getAudioGraphForTesting().getConnections(), connections);
            for (int i = 0; i < size; ++i)
                ASSERT_TRUE(std::isfinite(audio.getSample(0, i)));
        }
        EXPECT_FALSE(envelope->isActive());
        EXPECT_FALSE(vco->getSoundGenerator()->isActive());
    }
}

TEST(RoutingSelectionTest, Observation_PairedLegacyFixedAndDualInputCosts) {
    for (int size : {1, 16, 64})
        for (int filter : {0, 1})
            for (int target : {0, 1}) {
                std::array<std::unique_ptr<CS01AudioProcessor>, 3> processors;
                std::array<juce::AudioBuffer<float>, 3> audio;
                std::array<juce::MidiBuffer, 3> midi;
                std::array<std::vector<double>, 3> costs;
                for (int mode = 0; mode < 3; ++mode) {
                    processors[mode] = std::make_unique<CS01AudioProcessor>();
                    if (mode == 0)
                        processors[mode]->useLegacyRoutingForTesting();
                    if (mode == 2)
                        processors[mode]->useDualFilterInputForTesting(true);
                    initialise(*processors[mode], filter, target, 48000, 64);
                    audio[mode].setSize(2, size);
                    midi[mode].addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
                    costs[mode].reserve(1000);
                }
                for (int trial = -32; trial < 1000; ++trial)
                    for (int order = 0; order < 3; ++order) {
                        const int mode = ((trial + 33) + order) % 3;
                        const auto start = std::chrono::steady_clock::now();
                        processors[mode]->processBlock(audio[mode], midi[mode]);
                        const double elapsed = std::chrono::duration<double, std::micro>(
                                                   std::chrono::steady_clock::now() - start)
                                                   .count();
                        if (trial >= 0)
                            costs[mode].push_back(elapsed);
                    }
                for (int mode = 0; mode < 3; ++mode) {
                    std::sort(costs[mode].begin(), costs[mode].end());
                    std::cout << "routing-paired block=" << size << " filter=" << filter
                              << " target=" << target << " mode=" << mode
                              << " median_us=" << costs[mode][499] << " p99_us=" << costs[mode][989]
                              << " max_us=" << costs[mode].back() << "\n";
                }
            }
}

TEST(RoutingSelectionTest, MatchedSwitchesPreserveLegacyTransientAndReleaseOutput) {
    CS01AudioProcessor fixed, legacy;
    legacy.useLegacyRoutingForTesting();
    initialise(fixed, 0, 0, 48000, 64);
    initialise(legacy, 0, 0, 48000, 64);
    juce::AudioBuffer<float> a(2, 64), b(2, 64);
    juce::MidiBuffer am, bm;
    am.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    bm.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    double peak = 0, switchStep = 0, maxDifference = 0;
    float previous = 0;
    for (int block = 0; block < 1000; ++block) {
        const bool switching = block % 17 == 0;
        if (switching) {
            for (auto* processor : {&fixed, &legacy}) {
                choice(*processor, ParameterIds::filterType, (block / 17) % 2);
                choice(*processor, ParameterIds::lfoTarget, (block / 51) % 2);
            }
            legacy.flushPendingGraphChangesForTesting();
        }
        if (block == 200) {
            am.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
            bm.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
        }
        fixed.processBlock(a, am);
        legacy.processBlock(b, bm);
        for (int sample = 0; sample < 64; ++sample) {
            const float value = a.getSample(0, sample);
            ASSERT_TRUE(std::isfinite(value));
            maxDifference = std::max(maxDifference,
                                     std::abs(static_cast<double>(value) - b.getSample(0, sample)));
            ASSERT_EQ(value, b.getSample(0, sample));
            peak = std::max(peak, std::abs(static_cast<double>(value)));
            if (switching)
                switchStep = std::max(switchStep, std::abs(static_cast<double>(value) - previous));
            previous = value;
        }
    }
    std::cout << "routing-matched peak=" << peak << " switch_step=" << switchStep
              << " max_difference=" << maxDifference
              << " (legacy hard-switch behavior, not click-free certification)\n";
}

#if defined(CHEAPSYNTH_RT_AUDIT)
TEST(RoutingSelectionRealtimeTest, EveryCallbackCanSwitchWithoutHeapOperationsOrGraphMutation) {
    CS01AudioProcessor p;
    initialise(p, 0, 0, 48000, 64);
    const auto connections = p.getAudioGraphForTesting().getConnections();
    juce::AudioBuffer<float> storage(2, 64);
    juce::MidiBuffer midi;
    midi.ensureSize(32768);
    auto* filter = p.apvts.getParameter(ParameterIds::filterType);
    auto* target = p.apvts.getParameter(ParameterIds::lfoTarget);
    std::size_t allocations = 0, frees = 0, locks = 0;
    bool correctSelection = true;
    std::thread audio([&] {
        for (int iteration = 0; iteration < 1000; ++iteration) {
            filter->setValueNotifyingHost(static_cast<float>(iteration % 2));
            target->setValueNotifyingHost(static_cast<float>((iteration / 2) % 2));
            const int length = std::array<int, 4>{0, 1, 16, 64}[iteration % 4];
            juce::AudioBuffer<float> block(storage.getArrayOfWritePointers(), 2, length);
            realtimeAudit::begin();
            p.processBlock(block, midi);
            realtimeAudit::end();
            allocations += realtimeAudit::allocations;
            frees += realtimeAudit::deallocations;
            locks += realtimeAudit::locks;
            correctSelection &= p.getAppliedFilterTypeForTesting() == iteration % 2 &&
                                p.getAppliedLfoTargetForTesting() == (iteration / 2) % 2;
        }
    });
    audio.join();
    EXPECT_TRUE(correctSelection);
    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(frees, 0u);
    EXPECT_EQ(p.getAudioGraphForTesting().getConnections(), connections);
    std::cout << "routing-callback switches=1000 allocations=" << allocations << " frees=" << frees
              << " locks=" << locks << " (host notification outside callback probe)\n";
}
#endif
