#include <JuceHeader.h>

#include "CS01Synth/CS01IIVCFCircuit.h"
#include "CS01Synth/CS01VCFCircuit.h"
#include "CS01Synth/VCAProcessor.h"
#include "Parameters.h"
#include "reference/DspBeforeRefactor.h"

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {
// Exact float bit comparison also distinguishes signed zero; no ULP tolerance.
void expectSameBits(float actual, float before, int sample) {
    ASSERT_EQ(std::bit_cast<uint32_t>(actual), std::bit_cast<uint32_t>(before)) << sample;
}

float stimulus(int sample) {
    // Exercise silence, DC, impulse, small signal, and driven nonlinear behavior.
    if (sample < 128 || sample >= 6000)
        return 0.0f;
    if (sample == 128)
        return 3.0f;
    const float level = sample < 2000 ? 0.01f : 2.0f;
    return 0.1f + level * static_cast<float>(std::sin(sample * 0.137));
}
}  // namespace

TEST(DspStructureRegressionTest, OriginalCircuitMatchesBeforeRefactorExactly) {
    for (double rate : {44100.0, 48000.0, 96000.0, 192000.0}) {
        SCOPED_TRACE(rate);
        CS01VCFCircuit actual;
        BeforeOriginalCircuit before;
        actual.prepare(rate);
        before.prepare(rate);
        for (int pass = 0; pass < 2; ++pass) {
            for (int offset = 0; offset < 8192; offset += 64) {
                if (offset == 4096) {
                    // Preserve prepare semantics, including coupling states that
                    // are deliberately retained by the pre-refactor wrapper.
                    actual.prepare(rate);
                    before.prepare(rate);
                }
                float output[64], reference[64], cutoff[64];
                const float resonance = static_cast<float>((offset / 64) % 11) / 10.0f;
                for (int i = 0; i < 64; ++i) {
                    output[i] = reference[i] = stimulus(offset + i);
                    cutoff[i] = 20.0f + static_cast<float>((offset + i) * 37 % 19980);
                }
                // Cover both wrapper entry points and per-sample modulation.
                if (offset % 128 == 0) {
                    actual.processBlock(output, 64, cutoff, resonance);
                    before.processBlock(reference, 64, cutoff, resonance);
                } else {
                    actual.setCutoffFrequency(cutoff[0]);
                    before.setCutoffFrequency(cutoff[0]);
                    actual.setResonance(resonance);
                    before.setResonance(resonance);
                    actual.processBlock(output, 64);
                    before.processBlock(reference, 64);
                }
                for (int i = 0; i < 64; ++i)
                    expectSameBits(output[i], reference[i], offset + i);
            }
            actual.reset();
            before.reset();
        }
    }
}

TEST(DspStructureRegressionTest, ModernCircuitMatchesBeforeRefactorExactly) {
    for (double rate : {44100.0, 48000.0, 96000.0, 192000.0}) {
        SCOPED_TRACE(rate);
        CS01IIVCFCircuit actual;
        BeforeModernCircuit before;
        actual.prepare(rate);
        before.prepare(rate);
        for (int pass = 0; pass < 2; ++pass) {
            for (int i = 0; i < 8192; ++i) {
                const float cutoff = 20.0f + static_cast<float>(i * 37 % 19980);
                const float resonance = static_cast<float>(i % 11) / 10.0f;
                actual.setCutoffFrequency(cutoff);
                before.setCutoffFrequency(cutoff);
                actual.setResonance(resonance);
                before.setResonance(resonance);
                expectSameBits(actual.processSample(0, stimulus(i)),
                               before.processSample(0, stimulus(i)), i);
            }
            actual.reset();
            before.reset();
        }
    }
}

TEST(DspStructureRegressionTest, VcaSignalPathMatchesBeforeRefactorExactly) {
    juce::AudioProcessorGraph owner;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& id : {ParameterIds::vcaEgDepth, ParameterIds::breathInput,
                           ParameterIds::breathVca, ParameterIds::volume})
        layout.add(std::make_unique<juce::AudioParameterFloat>(id, id, 0.0f, 1.0f, 0.5f));
    juce::AudioProcessorValueTreeState state(owner, nullptr, "Regression", std::move(layout));
    VCAProcessor actual(state);
    BeforeVCAProcessor before(state);
    juce::MidiBuffer midi;
    for (double rate : {44100.0, 48000.0, 96000.0, 192000.0}) {
        SCOPED_TRACE(rate);
        for (int blockSize : {1, 7, 64, 256}) {
            SCOPED_TRACE(blockSize);
            for (int pass = 0; pass < 2; ++pass) {
                actual.prepareToPlay(rate, 256);
                before.prepareToPlay(rate, 256);
                for (int offset = 0; offset < 8192;) {
                    const float control = static_cast<float>((offset / 512) % 5) / 4.0f;
                    state.getParameter(ParameterIds::vcaEgDepth)->setValueNotifyingHost(control);
                    state.getParameter(ParameterIds::breathVca)
                        ->setValueNotifyingHost(1.0f - control);
                    state.getParameter(ParameterIds::breathInput)->setValueNotifyingHost(control);
                    state.getParameter(ParameterIds::volume)->setValueNotifyingHost(1.0f);
                    const int count = std::min(blockSize, 8192 - offset);
                    juce::AudioBuffer<float> output(2, count), reference(2, count);
                    for (int i = 0; i < count; ++i) {
                        output.setSample(0, i, stimulus(offset + i));
                        output.setSample(1, i, static_cast<float>((offset + i) % 101) / 100.0f);
                    }
                    reference.makeCopyOf(output);
                    actual.processBlock(output, midi);
                    before.processBlock(reference, midi);
                    for (int i = 0; i < count; ++i)
                        expectSameBits(output.getSample(0, i), reference.getSample(0, i),
                                       offset + i);
                    offset += count;
                }
                actual.releaseResources();
                before.releaseResources();
            }
        }
    }
}

TEST(DspStructureRegressionTest, ModelSafetyMatchesBeforeRefactorExactly) {
    IG02610BehavioralModel original;
    BeforeIG02610 beforeOriginal;
    IG05630BehavioralModel modern;
    BeforeIG05630 beforeModern;
    original.prepare(48000.0);
    beforeOriginal.prepare(48000.0);
    modern.prepare(48000.0);
    beforeModern.prepare(48000.0);
    modern.setCutoffFrequency(std::numeric_limits<float>::quiet_NaN());
    beforeModern.setCutoffFrequency(std::numeric_limits<float>::quiet_NaN());
    modern.setResonance(std::numeric_limits<float>::infinity());
    beforeModern.setResonance(std::numeric_limits<float>::infinity());
    for (int i = 0; i < 1024; ++i) {
        const float input = i == 10   ? std::numeric_limits<float>::quiet_NaN()
                            : i == 20 ? std::numeric_limits<float>::infinity()
                            : i == 30 ? std::numeric_limits<float>::max()
                                      : stimulus(i);
        expectSameBits(original.processSample(input, 19000.0f, 1.0f),
                       beforeOriginal.processSample(input, 19000.0f, 1.0f), i);
        expectSameBits(modern.processSample(input), beforeModern.processSample(input), i);
    }
}
