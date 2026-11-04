#pragma once

// Provisional IG02600 transfer model, not a reconstruction of its internal circuit.
// Guide PDF p137 / printed 8-3 establishes control/reference, Bias and output
// blocks, not quantitative transfer curves. Values below are normalized hypotheses.
// EG/breath/gate/volume composition and external coupling belong outside the IC.
class IG02600BehavioralModel {
   public:
    struct EmpiricalParameters {
        // Normalized software threshold/curve, not physical IC specifications.
        static constexpr float saturationThreshold = 0.7f;
        static constexpr float saturationCurve = 0.5f;
    };
    struct OperatingPoint {
        // Provisional reference/bias-to-gain scale and unity internal output stage.
        // These are not the physical Bias/Ein pin voltages or the external Tr7.
        float referenceGain = 1.0f;
        float outputStageGain = 1.0f;
    };

    IG02600BehavioralModel() = default;
    explicit IG02600BehavioralModel(OperatingPoint operatingPoint) : point(operatingPoint) {}
    float processSample(float input, float controlLevel) const;

   private:
    OperatingPoint point;
};
