#include "ParameterSetup.h"
#include <gtest/gtest.h>
#include <array>
#include <string>

TEST(ParameterPresentationTest, TypedMillisecondsPreserveCoreSeconds) {
    for (auto id :
         {cs01::Param::Attack, cs01::Param::Decay, cs01::Param::Release, cs01::Param::Glissando}) {
        iplug::IParam parameter;
        cs01::initializeHostParameter(parameter, id);
        const double entered = parameter.StringToValue("125");
        EXPECT_DOUBLE_EQ(entered, 125.0);
        EXPECT_DOUBLE_EQ(cs01::fromHostValue(id, entered), .125);
        parameter.Set(entered);
        WDL_String displayed;
        parameter.GetDisplay(displayed);
        EXPECT_STREQ(displayed.Get(), "125");
        EXPECT_STREQ(parameter.GetLabel(), id == cs01::Param::Glissando ? "ms/st" : "ms");
        EXPECT_NEAR(cs01::fromHostValue(id, parameter.GetDefault()), cs01::definition(id).initial,
                    1.e-8);
        EXPECT_NEAR(parameter.StringToValue("999999"),
                    cs01::toHostValue(id, cs01::definition(id).maximum), 1.e-8);
    }
}

TEST(ParameterPresentationTest, EveryHostParameterKeepsCoreNormalizedAutomation) {
    for (int i = 0; i < cs01::parameterCount; ++i) {
        const auto id = static_cast<cs01::Param>(i);
        const auto& d = cs01::definition(id);
        SCOPED_TRACE(d.name);
        iplug::IParam parameter;
        cs01::initializeHostParameter(parameter, id);
        for (double fraction : {0., .1, .5, .9, 1.}) {
            const double plain = double(d.minimum) + fraction * (double(d.maximum) - d.minimum);
            const double core = d.integer ? std::round(plain) : plain;
            const double expected =
                cs01::isEnvelopeTime(id)
                    ? EGTiming::position(core)
                    : std::pow((core - d.minimum) / (double(d.maximum) - d.minimum),
                               double(d.skew));
            EXPECT_NEAR(parameter.ToNormalized(cs01::toHostValue(id, core)), expected, 1.e-6);
            EXPECT_NEAR(cs01::fromHostValue(id, parameter.FromNormalized(expected)), core, 1.e-6);
        }
    }
}

TEST(ParameterPresentationTest, GroupsIncludeEnumsAndPreservePerformanceFlags) {
    const std::array<const char*, cs01::parameterCount> expected{
        "VCO",
        "VCO",
        "VCO",
        "VCO",
        "VCO",
        "VCF",
        "VCF",
        "VCF",
        "VCA",
        "EG",
        "EG",
        "EG",
        "EG",
        "LFO",
        "LFO",
        "Performance",
        "Performance",
        "VCF",
        "VCA",
        "Performance",
        "Performance",
        "VCA",
        "Performance",
        "VCF",
        "Quality",
    };
    for (int i = 0; i < cs01::parameterCount; ++i) {
        iplug::IParam parameter;
        cs01::initializeHostParameter(parameter, static_cast<cs01::Param>(i));
        EXPECT_STREQ(parameter.GetGroup(), expected[i]);
        EXPECT_EQ(parameter.GetCanAutomate(), i != static_cast<int>(cs01::Param::PitchBend) &&
                                                  i != static_cast<int>(cs01::Param::ModDepth));
    }
}

TEST(ParameterPresentationTest, EnvelopeHostTaperMatchesCoreAtEveryPanelPosition) {
    for (auto id : {cs01::Param::Attack, cs01::Param::Decay, cs01::Param::Release}) {
        iplug::IParam parameter;
        cs01::initializeHostParameter(parameter, id);
        for (int i = 0; i <= 1000; ++i) {
            const double position = i / 1000.;
            const double seconds = cs01::fromHostValue(id, parameter.FromNormalized(position));
            EXPECT_NEAR(seconds, EGTiming::timeConstant(position), 1e-6);
            EXPECT_NEAR(cs01::toNormalized(id, float(seconds)), position, 1e-6);
            EXPECT_NEAR(parameter.ToNormalized(cs01::toHostValue(id, seconds)), position, 1e-6);
        }
    }
}

TEST(ParameterPresentationTest, OversamplingLabelsAndDefaultMatchPanelChoices) {
    iplug::IParam parameter;
    cs01::initializeHostParameter(parameter, cs01::Param::Oversampling);
    EXPECT_DOUBLE_EQ(parameter.GetDefault(), 2);
    const std::array labels{"OFF", "2x", "4x", "8x", "16x"};
    for (int stage = 0; stage < static_cast<int>(labels.size()); ++stage) {
        parameter.Set(stage);
        WDL_String displayed;
        parameter.GetDisplay(displayed);
        EXPECT_STREQ(displayed.Get(), labels[stage]);
        EXPECT_DOUBLE_EQ(parameter.StringToValue(labels[stage]), stage);
    }
}
