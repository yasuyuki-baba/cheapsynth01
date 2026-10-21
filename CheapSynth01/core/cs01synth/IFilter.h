#pragma once

#include "dsp/Primitives.h"
#include "Parameters.h"

#include <algorithm>
#include <cmath>

/**
 * Common base for VCF processors sharing cutoff modulation (EG + LFO + Breath).
 */
class IFilter {
   public:
    explicit IFilter(cs01::ParameterState& parameters) : parameters(parameters) {}

   protected:
    ~IFilter() = default;

    void prepareCutoffModulation(double sampleRate) {
        egDepthControl.reset(sampleRate, .005);
        egDepthControl.setCurrentAndTargetValue(parameters.get(ParameterIds::vcfEgDepth));
    }

    float nextModulatedCutoff(float eg, float lfo, float maxFreq = 20000.0f) {
        egDepthControl.setTargetValue(parameters.get(ParameterIds::vcfEgDepth));
        const float depth = egDepthControl.getNextValue();
        const float base = std::clamp(parameters.get(ParameterIds::cutoff), 20.0f, 20000.0f);
        const float semitones =
            eg * depth * 36.f +
            std::clamp(lfo, -1.f, 1.f) * parameters.get(ParameterIds::modDepth) * 24.f +
            parameters.get(ParameterIds::breathInput) * parameters.get(ParameterIds::breathVcf) *
                24.f;
        float cutoff = base * std::exp2(semitones / 12.f);
        if (!std::isfinite(cutoff))
            cutoff = base;
        return std::clamp(cutoff, 20.f, maxFreq);
    }

    cs01::ParameterState& parameters;
    cs01::LinearRamp egDepthControl;
};
