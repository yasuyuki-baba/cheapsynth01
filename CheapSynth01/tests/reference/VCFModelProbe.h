#pragma once

#include "cs01synth/IG02610BehavioralModel.h"

namespace cs01::test {
// Convenience for existing probes that sweep both controls at every sample.
inline float processOriginalSample(IG02610BehavioralModel& model, float input, float cutoff,
                                   double damping) {
    model.setCutoffFrequency(cutoff);
    model.setFeedback({damping, 1.0});
    return model.processSample(input);
}
}  // namespace cs01::test
