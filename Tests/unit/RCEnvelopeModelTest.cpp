#include <gtest/gtest.h>

#include <cmath>

namespace {
// Experimental ideal RC section only. Not a calibrated CS-01 envelope.
class RCSection {
   public:
    double step(double target, double timeConstant, double sampleRate) {
        value += (target - value) * -std::expm1(-1.0 / (timeConstant * sampleRate));
        return value;
    }
    double value = 0.0;
};
}  // namespace

TEST(RCEnvelopeModelTest, MatchesAnalyticalChargeAndDischarge) {
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        RCSection section;
        const double tau = 0.2;
        const int count = static_cast<int>(sampleRate);
        for (int i = 0; i < count; ++i) {
            const double output = section.step(1.0, tau, sampleRate);
            EXPECT_NEAR(output, 1.0 - std::exp(-(i + 1) / (sampleRate * tau)), 1.0e-11);
        }
        const double initial = section.value;
        for (int i = 0; i < count; ++i) {
            const double output = section.step(0.0, tau, sampleRate);
            EXPECT_NEAR(output, initial * std::exp(-(i + 1) / (sampleRate * tau)), 1.0e-11);
        }
    }
}

TEST(RCEnvelopeModelTest, ChangingRateAndTargetPreservesCapacitorState) {
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        RCSection section;
        const int count = static_cast<int>(sampleRate * 0.1);
        for (int i = 0; i < count; ++i)
            section.step(1.0, 0.2, sampleRate);
        const double initial = section.value;
        // A new resistance changes the slope, not the stored voltage.
        for (int i = 0; i < count; ++i) {
            const double output = section.step(0.0, 0.5, sampleRate);
            EXPECT_NEAR(output, initial * std::exp(-(i + 1) / (sampleRate * 0.5)), 1.0e-11);
            EXPECT_GT(output, 0.0);
            EXPECT_LT(output, initial);
        }
    }
}

TEST(RCEnvelopeModelTest, LoadedCapacitorMatchesIndependentCircuitSolution) {
    // Hypothetical network, not identified CS-01 component values:
    // Vs -- Rs -- capacitor node -- Rl -- ground.
    const double sourceVoltage = 5.0;
    const double sourceResistance = 100000.0;
    const double loadResistance = 200000.0;
    const double capacitance = 1.0e-6;
    const double initialVoltage = 0.3;
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        RCSection section;
        section.value = initialVoltage;
        const double parallelResistance = 1.0 / (1.0 / sourceResistance + 1.0 / loadResistance);
        const double target = sourceVoltage * loadResistance / (sourceResistance + loadResistance);
        const double tau = parallelResistance * capacitance;
        // Independent solution of C*dV/dt = Vs/Rs - V*(1/Rs + 1/Rl).
        const double rate = (1.0 / sourceResistance + 1.0 / loadResistance) / capacitance;
        const double equilibrium = (sourceVoltage / sourceResistance / capacitance) / rate;
        for (int i = 0; i < static_cast<int>(sampleRate * 0.5); ++i) {
            const double time = (i + 1) / sampleRate;
            const double expected =
                equilibrium + (initialVoltage - equilibrium) * std::exp(-rate * time);
            EXPECT_NEAR(section.step(target, tau, sampleRate), expected, 1.0e-11);
        }
        // Loading changes both the asymptote and the time constant.
        EXPECT_LT(target, sourceVoltage);
        EXPECT_LT(tau, sourceResistance * capacitance);
    }
}