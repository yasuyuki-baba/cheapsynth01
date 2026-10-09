#include "CS01Synth/IG02600BehavioralModel.h"

#include <cmath>

float IG02600BehavioralModel::processSample(float input, float egValue, float egDepth,
                                            float breathInput, float breathDepth, float volumeGain,
                                            float noteGate) const {
    // Uncalibrated control composition retained from the existing implementation.
    // Preserve the original arithmetic when fully open, including floating-point
    // contraction/rounding on ARM. Only the moving gate needs the extra product.
    float controlVoltage = noteGate == 1.0f ? (1.0f - egDepth) + (egValue * egDepth)
                                            : ((1.0f - egDepth) * noteGate) + (egValue * egDepth);
    controlVoltage *= (1.0f - breathDepth) + (breathInput * breathDepth);
    float output = input * (controlVoltage * volumeGain);

    // Empirical saturation; threshold and curve are not established IC specifications.
    if (std::abs(output) > EmpiricalParameters::saturationThreshold) {
        float sign = (output > 0.0f) ? 1.0f : -1.0f;
        float excess = std::abs(output) - EmpiricalParameters::saturationThreshold;
        output = sign * (EmpiricalParameters::saturationThreshold +
                         excess / (1.0f + excess * EmpiricalParameters::saturationCurve));
    }
    return output;
}
