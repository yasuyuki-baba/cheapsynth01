#include "cs01synth/IG02600BehavioralModel.h"

#include <cmath>

float IG02600BehavioralModel::processSample(float input, float controlLevel) const {
    // Linear control-to-gain is provisional; the guide supplies no measured law.
    float output = input * (controlLevel * point.referenceGain);

    // Empirical saturation; threshold and curve are not established IC specifications.
    if (std::abs(output) > EmpiricalParameters::saturationThreshold) {
        float sign = (output > 0.0f) ? 1.0f : -1.0f;
        float excess = std::abs(output) - EmpiricalParameters::saturationThreshold;
        output = sign * (EmpiricalParameters::saturationThreshold +
                         excess / (1.0f + excess * EmpiricalParameters::saturationCurve));
    }
    // Provisional unity transfer for the internal output stage. The guide's
    // Ein/Out/Bias blocks do not specify a calibrated headroom or follower drop.
    return output * point.outputStageGain;
}
