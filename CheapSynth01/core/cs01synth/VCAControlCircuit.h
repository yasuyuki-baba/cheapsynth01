#pragma once

namespace cs01 {
// Instrument/software control policy, not the IG02600's internal gain law.
// Values are normalized; physical control-voltage/current conversion is unknown.
struct VCAControlCircuit {
    static float controlLevel(float eg, float egDepth, float breath, float breathDepth,
                              float volumeGain, float noteGate) {
        float level = (1.0f - egDepth) + eg * egDepth;
        if (noteGate != 1.0f)
            level += (1.0f - egDepth) * (noteGate - 1.0f);
        level *= (1.0f - breathDepth) + breath * breathDepth;
        return level * volumeGain;
    }
};
}  // namespace cs01
