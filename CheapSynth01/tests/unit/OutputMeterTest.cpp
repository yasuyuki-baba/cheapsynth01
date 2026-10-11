#include "dsp/OutputMeter.h"
#include <gtest/gtest.h>
#include <array>
#include <limits>
#include <vector>

TEST(OutputMeterTest, WindowsKeepBoundaryTransientsAcrossBlockPartitions) {
    for (double rate : {44100., 48000., 96000.}) {
        const int window = static_cast<int>(rate * .02);
        std::vector<float> samples(window * 4, 0.f);
        samples[window - 1] = -1.25f;
        samples[window] = .75f;
        std::vector<cs01::OutputMeter::Reading> reference;
        for (int blockSize : {1, 7, 64, 127, 256, 1024}) {
            cs01::OutputMeter meter;
            meter.reset(rate);
            std::vector<cs01::OutputMeter::Reading> result;
            for (int offset = 0; offset < static_cast<int>(samples.size()); offset += blockSize) {
                const int end = std::min(offset + blockSize, static_cast<int>(samples.size()));
                for (int i = offset; i < end; ++i) {
                    cs01::OutputMeter::Reading reading;
                    if (meter.process(samples[i], reading))
                        result.push_back(reading);
                }
            }
            ASSERT_EQ(result.size(), 4u);
            EXPECT_FLOAT_EQ(result[0].peak, 1.25f);
            EXPECT_GT(result[0].rms, .02f);
            EXPECT_LT(result[0].rms, .05f);
            if (reference.empty())
                reference = result;
            for (size_t i = 0; i < reference.size(); ++i) {
                EXPECT_FLOAT_EQ(result[i].peak, reference[i].peak);
                EXPECT_FLOAT_EQ(result[i].rms, reference[i].rms);
            }
        }
    }
}

TEST(OutputMeterTest, SteadyRmsAndPeakHoldDecayToSilence) {
    cs01::OutputMeter meter;
    cs01::OutputMeter::Reading reading;
    for (int i = 0; i < 48000; ++i)
        meter.process(i % 2 == 0 ? .5f : -.5f, reading);
    EXPECT_NEAR(reading.rms, .5f, 1.e-6f);
    EXPECT_FLOAT_EQ(reading.peak, .5f);
    for (int i = 0; i < 23040; ++i)
        meter.process(0.f, reading);
    EXPECT_FLOAT_EQ(reading.peak, .5f);
    for (int i = 0; i < 960; ++i)
        meter.process(0.f, reading);
    EXPECT_FLOAT_EQ(reading.peak, 0.f);
    for (int i = 0; i < 48000 * 5; ++i)
        meter.process(0.f, reading);
    EXPECT_FLOAT_EQ(reading.rms, 0.f);
}

TEST(OutputMeterTest, ResetClearsHistoryAndNonfiniteSamplesCannotPoisonReadings) {
    cs01::OutputMeter meter;
    cs01::OutputMeter::Reading reading;
    for (int i = 0; i < 1440; ++i)
        meter.process(2.f, reading);
    meter.reset(44100.);
    for (int i = 0; i < 881; ++i)
        EXPECT_FALSE(meter.process(0.f, reading));
    EXPECT_TRUE(meter.process(0.f, reading));
    EXPECT_FLOAT_EQ(reading.peak, 0.f);
    EXPECT_FLOAT_EQ(reading.rms, 0.f);
    for (int i = 0; i < 882; ++i)
        meter.process(i % 2 ? std::numeric_limits<float>::infinity()
                            : std::numeric_limits<float>::quiet_NaN(),
                      reading);
    EXPECT_FLOAT_EQ(reading.peak, 0.f);
    EXPECT_FLOAT_EQ(reading.rms, 0.f);
}
