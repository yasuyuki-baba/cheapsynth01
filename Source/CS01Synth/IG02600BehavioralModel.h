#pragma once

// Provisional IG02600 transfer model, not a reconstruction of its internal circuit.
// Controls are normalized software values, not physical pin voltages.
// External coupling, output buffer and panel volume mapping belong to VCAProcessor.
class IG02600BehavioralModel {
   public:
    struct EmpiricalParameters {
        // Normalized software threshold/curve, not physical IC specifications.
        static constexpr float saturationThreshold = 0.7f;
        static constexpr float saturationCurve = 0.5f;
    };
    float processSample(float input, float egValue, float egDepth, float breathInput,
                        float breathDepth, float volumeGain, float noteGate = 1.0f) const;
};
