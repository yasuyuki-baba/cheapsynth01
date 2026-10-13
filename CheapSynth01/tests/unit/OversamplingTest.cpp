#include "SynthEngine.h"
#include <gtest/gtest.h>
#include <array>
#include <vector>

TEST(OutputConverterTest, EveryFactorHasFixedLatencyUnityGainAndAliasRejection) {
    for (int stage = 0; stage <= 4; ++stage) {
        SCOPED_TRACE(stage);
        cs01::OutputConverter converter;
        converter.setStages(stage);
        const int factor = converter.getFactor();
        ASSERT_EQ(factor, 1 << stage);
        std::array<float, 64> response{};
        for (int i = 0; i < 64 * factor; ++i) {
            converter.push(i == 0 ? 1.f : 0.f);
            if (i % factor == 0)
                response[i / factor] = converter.output();
        }
        EXPECT_EQ(std::max_element(response.begin(), response.end()) - response.begin(),
                  SynthEngine::latency);
        converter.reset();
        for (int i = 0; i < 64 * factor; ++i)
            converter.push(1.f);
        EXPECT_NEAR(converter.output(), 1.f, 1.e-6);
        if (stage == 0)
            continue;
        converter.reset();
        double power = 0;
        for (int i = 0; i < 4096 * factor; ++i) {
            // Tone above host Nyquist, below internal Nyquist even at 2x.
            converter.push(static_cast<float>(std::sin(2 * std::numbers::pi * .75 * i / factor)));
            if (i > 64 * factor && i % factor == 0)
                power += std::pow(converter.output(), 2);
        }
        EXPECT_LT(power, 1.e-5);
    }
}

TEST(EngineTest, OversamplingSwitchPreservesHeldNotesForBothSourcesAndFilters) {
    for (double rate : {8000., 44100., 48000., 96000., 192000., 384000.}) {
        for (int source : {2, 4}) {
            for (int filter : {0, 1}) {
                cs01::ParameterState parameters;
                parameters.set(cs01::Param::Feet, source);
                parameters.set(cs01::Param::FilterType, filter);
                parameters.set(cs01::Param::Attack, .001f);
                parameters.set(cs01::Param::Release, .01f);
                SynthEngine engine(parameters);
                engine.prepare(rate);
                engine.handleMidi({0x90, 69, 100});
                for (int stage : {0, 1, 2, 3, 4, 2, 0, 4}) {
                    parameters.set(cs01::Param::Oversampling, stage);
                    double energy = 0;
                    for (int i = 0; i < 2048; ++i) {
                        const float sample = engine.renderSample();
                        ASSERT_TRUE(std::isfinite(sample));
                        energy += sample * sample;
                    }
                    EXPECT_GT(energy, 0);
                    EXPECT_EQ(engine.currentNote(), 69);
                    EXPECT_TRUE(engine.externalNotes().test(69));
                }
                engine.handleMidi({0x80, 69, 0});
                for (int i = 0; i < static_cast<int>(rate * .3); ++i) {
                    if (i == static_cast<int>(rate * .01))
                        parameters.set(cs01::Param::Oversampling, 1);
                    ASSERT_TRUE(std::isfinite(engine.renderSample()));
                }
                EXPECT_EQ(engine.currentNote(), 0);
                EXPECT_TRUE(engine.externalNotes().none());
                EXPECT_NEAR(engine.renderSample(), 0.f, 1.e-5);
            }
        }
    }
}

TEST(EGContinuityTest, OversamplingRateChangePreservesReleaseLevelAndRemainingTime) {
    for (int before = 0; before <= 4; ++before) {
        for (int after = 0; after <= 4; ++after) {
            cs01::ParameterState parameters;
            parameters.set(cs01::Param::Release, .1f);
            EGProcessor envelope(parameters);
            envelope.prepare(48000 * (1 << before));
            envelope.startEnvelope();
            for (int i = 0; i < 48000 * (1 << before); ++i)
                envelope.processSample();
            envelope.releaseEnvelope();
            for (int i = 0; i < 1200 * (1 << before); ++i)
                envelope.processSample();
            const double remainingFrames =
                static_cast<double>(envelope.getReleaseSamplesRemaining()) / (1 << before);
            const float level = envelope.getLastOutputForTesting();
            envelope.setSampleRate(48000 * (1 << after));
            EXPECT_FLOAT_EQ(envelope.getLastOutputForTesting(), level);
            EXPECT_NEAR(static_cast<double>(envelope.getReleaseSamplesRemaining()) / (1 << after),
                        remainingFrames, 1.);
        }
    }
}

TEST(EngineTest, AllOversamplingFactorsProduceIdenticalAudioAcrossBlockPartitions) {
    for (int stage = 0; stage <= 4; ++stage) {
        std::vector<float> reference;
        for (int blockSize : {1, 7, 64, 511}) {
            cs01::ParameterState control, render;
            control.set(cs01::Param::Oversampling, stage);
            ASSERT_TRUE(control.tryCopyTo(render));
            SynthEngine engine(render);
            engine.prepare(48000);
            std::vector<float> audio;
            for (int frame = 0; frame < 4096; ++frame) {
                if (frame % blockSize == 0)
                    ASSERT_TRUE(control.tryCopyTo(render));
                if (frame == 0)
                    engine.handleMidi({0x90, 69, 100});
                if (frame == 2000)
                    engine.handleMidi({0x80, 69, 0});
                audio.push_back(engine.renderSample());
            }
            if (blockSize == 1)
                reference = audio;
            else
                EXPECT_EQ(audio, reference);
        }
    }
}
