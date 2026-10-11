#include "../../CheapSynth01/config.h"
#include "Parameters.h"
#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <string_view>

// Fixed compatibility contract from the JUCE release and initial iPlug2 index
// manifest. This guards the OOS move against template-generated fresh identities.
TEST(CompatibilityTest, PluginIdentityAndParameterOrderRemainStable) {
    EXPECT_EQ(std::string_view(PLUG_NAME), "CheapSynth01");
    EXPECT_EQ(static_cast<uint32_t>(PLUG_UNIQUE_ID), 0x43533031u);
    EXPECT_EQ(static_cast<uint32_t>(PLUG_MFR_ID), 0x42414241u);
    EXPECT_EQ(PLUG_TYPE, 1);
    constexpr std::array<uint32_t, 4> component{VST3_PROCESSOR_UID};
    constexpr std::array<uint32_t, 4> controller{VST3_CONTROLLER_UID};
    EXPECT_EQ(component, (std::array<uint32_t, 4>{0xABCDEF01, 0x9182FAEB, 0x42414241, 0x43533031}));
    EXPECT_EQ(controller,
              (std::array<uint32_t, 4>{0xABCDEF01, 0x1234ABCD, 0x42414241, 0x43533031}));
    constexpr std::array<std::string_view, 24> ids{"WAVE_TYPE",
                                                   "FEET",
                                                   "PWM_SPEED",
                                                   "PITCH",
                                                   "GLISSANDO",
                                                   "CUTOFF",
                                                   "RESONANCE",
                                                   "VCF_EG_DEPTH",
                                                   "VCA_EG_DEPTH",
                                                   "ATTACK",
                                                   "DECAY",
                                                   "SUSTAIN",
                                                   "RELEASE",
                                                   "LFO_SPEED",
                                                   "LFO_TARGET",
                                                   "MOD_DEPTH",
                                                   "PITCH_BEND",
                                                   "BREATH_VCF",
                                                   "BREATH_VCA",
                                                   "PITCH_BEND_UP_RANGE",
                                                   "PITCH_BEND_DOWN_RANGE",
                                                   "VOLUME",
                                                   "BREATH_INPUT",
                                                   "FILTER_TYPE"};
    ASSERT_GE(cs01::parameterCount, static_cast<int>(ids.size()));
    for (size_t i = 0; i < ids.size(); ++i)
        EXPECT_EQ(cs01::parameterDefinitions[i].id, ids[i]) << i;
}
