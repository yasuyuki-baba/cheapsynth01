#pragma once

#include "cs01synth/VCFIntegratorCore.h"
#include "cs01synth/VCFNonlinearInput.h"
#include <cstdint>

namespace cs01 {
// Shared behavioral implementation, not evidence of identical physical ICs.
// Each IC selects its stage count and its own provisional nonlinear parameters;
// the caller supplies the complete effective feedback network.
template <std::size_t Stages, class Parameters>
class VCFBehavioralCore {
   public:
    using Feedback = std::array<double, Stages>;
    using EmpiricalParameters = Parameters;

    explicit VCFBehavioralCore(Feedback feedback) : feedback(feedback) {}

    void prepare(double sampleRate) {
        noiseState = 0x12345678u;
        core.prepare(sampleRate);
        seed();
    }
    void reset() {
        noiseState = 0x12345678u;
        core.reset();
        seed();
    }
    void setCutoffFrequency(float frequency) {
        core.setCutoffFrequency(frequency);
    }
    void setFeedback(const Feedback& newFeedback, double limitingGain = 0.0) {
        const bool starting = feedbackLimiting == 0.0 && limitingGain > 0.0;
        feedback = newFeedback;
        feedbackLimiting = limitingGain;
        if (starting)
            seed();
    }
    float processSample(float sample) {
        if (!std::isfinite(sample))
            sample = 0.0f;
        double input =
            asymmetricSoftLimit(sample, Parameters::inputDrive, Parameters::negativeKneeThreshold,
                                Parameters::negativeClipAmount);
        if (feedbackLimiting > 0.0) {
            // Deterministic inaudible circuit-noise surrogate also starts
            // oscillation after slowly crossing the threshold from silence.
            noiseState ^= noiseState << 13;
            noiseState ^= noiseState >> 17;
            noiseState ^= noiseState << 5;
            input += (double(noiseState) / 4294967295.0 * 2.0 - 1.0) * Parameters::feedbackNoise;
        }
        const double output =
            core.processSample(input, feedback, feedbackLimiting, Parameters::feedbackLimit);
        if (!std::isfinite(output)) {
            reset();
            return 0.0f;
        }
        return static_cast<float>(std::clamp(output,
                                             -static_cast<double>(Parameters::maximumOutput),
                                             static_cast<double>(Parameters::maximumOutput)));
    }

   private:
    void seed() {
        // Deterministic, inaudible startup excitation represents circuit noise.
        if (feedbackLimiting > 0.0)
            core.excite(1e-9);
    }
    std::uint32_t noiseState = 0x12345678u;
    double feedbackLimiting = 0.0;
    VCFIntegratorCore<Stages> core;
    Feedback feedback;
};
}  // namespace cs01
