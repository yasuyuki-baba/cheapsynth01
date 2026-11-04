#pragma once

#include "cs01synth/VCFBehavioralCore.h"

namespace cs01 {
// IC-local nonlinear hypotheses, not measured voltages or a reconstructed IC.
// Separate types deliberately allow either IC's approximation to evolve alone.
struct IG02610Parameters {
    static constexpr float inputDrive = 2.0f;
    static constexpr float negativeKneeThreshold = 0.2f;
    static constexpr float negativeClipAmount = 0.35f;
    // Provisional internal feedback saturation scale, normalized audio units.
    static constexpr double feedbackLimit = 0.25;
    static constexpr double feedbackNoise = 1e-10;
    static constexpr float maximumOutput = 1.5f;
};
}  // namespace cs01

// Provisional 2-stage model. Effective feedback belongs to the circuit model;
// the IG00156 guide is related-part evidence, not this IC's internal schematic.
class IG02610BehavioralModel : public cs01::VCFBehavioralCore<2, cs01::IG02610Parameters> {
    using Base = cs01::VCFBehavioralCore<2, cs01::IG02610Parameters>;

   public:
    using Base::Base;
};
