#pragma once

#include <algorithm>
#include <cmath>

namespace cs01 {
// Shared arithmetic only. Each IC model owns its nonlinear-stage hypotheses:
// whether/where to use this curve and which normalized parameters to supply.
inline double asymmetricSoftLimit(float sample, float drive, float negativeKnee,
                                  float negativeAmount) {
    const double excessNegative =
        std::min(0.0, static_cast<double>(sample) + static_cast<double>(negativeKnee));
    const double driven = sample - excessNegative * static_cast<double>(negativeAmount);
    return std::tanh(driven * drive) / drive;
}
}  // namespace cs01
