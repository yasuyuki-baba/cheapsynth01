#include "IG02600.h"
#include <cmath>

float IG02600::processSample(float input, float egValue, float egDepth, float breathInput,
                             float breathDepth, float volumeGain) const {
    // Uncalibrated control composition retained from the existing implementation.
    float controlVoltage = (1.0f - egDepth) + (egValue * egDepth);
    controlVoltage *= (1.0f - breathDepth) + (breathInput * breathDepth);
    float output = input * (controlVoltage * volumeGain);

    // Empirical saturation; threshold and curve are not established IC specifications.
    if (std::abs(output) > 0.7f) {
        float sign = (output > 0.0f) ? 1.0f : -1.0f;
        float excess = std::abs(output) - 0.7f;
        output = sign * (0.7f + excess / (1.0f + excess * 0.5f));
    }
    return output;
}