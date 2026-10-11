#pragma once

#include "dsp/Primitives.h"

// External CS-01 coupling approximations. Frequencies are empirical, not
// derived from capacitor markings or an established effective circuit load.
class EmpiricalVCFInputCoupling {
   public:
    static constexpr float cutoffHz = 20.0f;
    void prepare(double sampleRate) {
        filter.highPass(sampleRate, cutoffHz);
        // Initialise the new coefficient order before the first audio callback.
        filter.reset();
    }
    void reset() {
        filter.reset();
    }
    float processSample(float sample) {
        return filter.processSample(sample);
    }

   private:
    cs01::Biquad filter;
};

class EmpiricalVCFOutputCoupling {
   public:
    static constexpr float cutoffHz = 8.0f;
    void prepare(double rate) {
        sampleRate = rate;
    }
    void reset() {
        previousInput = previousOutput = 0.0f;
    }
    float processSample(float sample) {
        // Preserve the original mixed float/double coefficient calculation.
        const float alpha =
            1.0f / (1.0f + 2.0f * std::numbers::pi_v<float> * cutoffHz / sampleRate);
        previousOutput = alpha * (previousOutput + sample - previousInput);
        previousInput = sample;
        return previousOutput;
    }

   private:
    float previousInput = 0.0f;
    float previousOutput = 0.0f;
    double sampleRate = 44100.0;
};
