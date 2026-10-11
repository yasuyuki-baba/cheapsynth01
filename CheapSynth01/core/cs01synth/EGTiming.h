#pragma once

#include <algorithm>

#include <cmath>

namespace EGTiming {
// Assumed 10% midpoint A taper, not a measured CS-01 potentiometer.
constexpr double minimumTau = 1800.0 * 2.2e-6;
constexpr double maximumTau = (1800.0 + 2.0e6) * 2.2e-6;
constexpr double settlingRatio = 1.0e-4;
inline double timeConstant(double position) {
    return minimumTau + 4.4 * std::expm1(std::clamp(position, 0.0, 1.0) * std::log(81.0)) / 80.0;
}
inline double position(double tau) {
    return std::clamp(
        std::log1p((std::clamp(tau, minimumTau, maximumTau) - minimumTau) * 80.0 / 4.4) /
            std::log(81.0),
        0.0, 1.0);
}
inline double maximumTailSeconds() {
    return maximumTau * std::log(1.0 / settlingRatio);
}
}  // namespace EGTiming
