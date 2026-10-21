#pragma once

#include <algorithm>

#include <cmath>

namespace EGTiming {
// Assumed 10% midpoint A taper, not a measured CS-01 potentiometer.
constexpr double minimumTau = 1800.0 * 2.2e-6;
// Decay includes an empirical floor for Tr11/Tr12 sustain-branch loading (~79 ms to -80 dB).
constexpr double minimumDecayTau = 3900.0 * 2.2e-6;
constexpr double maximumTau = (1800.0 + 2.0e6) * 2.2e-6;
constexpr double settlingRatio = 1.0e-4;
inline double timeConstant(double position, double minTau = minimumTau) {
    const double span = maximumTau - minTau;
    return minTau + span * std::expm1(std::clamp(position, 0.0, 1.0) * std::log(81.0)) / 80.0;
}
inline double position(double tau, double minTau = minimumTau) {
    const double span = maximumTau - minTau;
    return std::clamp(std::log1p((std::clamp(tau, minTau, maximumTau) - minTau) * 80.0 / span) /
                          std::log(81.0),
                      0.0, 1.0);
}
inline double maximumTailSeconds() {
    return maximumTau * std::log(1.0 / settlingRatio);
}
}  // namespace EGTiming
