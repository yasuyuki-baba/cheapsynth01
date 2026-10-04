#pragma once

// Provisional IG02600 transfer model, not a reconstruction of its internal circuit.
// Controls are normalized software values, not physical pin voltages.
// External coupling, output buffer and panel volume mapping belong to VCAProcessor.
class IG02600 {
   public:
    float processSample(float input, float egValue, float egDepth, float breathInput,
                        float breathDepth, float volumeGain) const;
};