#pragma once

#include <algorithm>
#include <array>
#include <cmath>

// Board/control assumptions, kept outside the IC behavioral solvers.
// These are musical approximations, not calibrated hardware values.
namespace cs01 {
struct OriginalVCFCircuitSettings {
    static constexpr float lowResonance = 0.2f;
    static constexpr float highResonance = 0.7f;
    static constexpr double lowQ = 0.5;
    static constexpr double highQ = 1.6;

    static float panelResonance(float position) {
        return position >= 0.5f ? highResonance : lowResonance;
    }

    static std::array<double, 2> feedback(float resonance) {
        return {damping(resonance), 1.0};
    }

    // Legacy circuit-level control also accepts positions between the two anchors.
    static double damping(float resonance) {
        const double position =
            std::clamp(((std::isfinite(resonance) ? static_cast<double>(resonance) : lowResonance) -
                        lowResonance) /
                           (static_cast<double>(highResonance) - lowResonance),
                       0.0, 1.0);
        return 1.0 / lowQ + position * (1.0 / highQ - 1.0 / lowQ);
    }
};

struct ModernVCFCircuitSettings {
    // Conditional external-summer coefficients: equal integrator frequencies,
    // unloaded state taps and matched input-node conductance are assumed.
    // The actual source impedance and Vin/VQ transfers remain unknown.
    static constexpr double oddStateFeedback = 82.0 / 22.0;
    static constexpr double evenStateFeedback = 82.0 / 15.0;
    // Musical targets: unity DC gain and about +12 dB maximum resonance peak.
    // This replaces the old 3.2 feedback range and its substantial bass loss.
    static constexpr double maximumFeedbackGain = 2.25;
    // Align zero-resonance rolloff with Original Low (0.6436 / 0.4916).
    static constexpr float cutoffAlignment = 1.3091f;

    static float internalCutoff(float cutoff, float maximum) {
        return std::clamp(cutoff * cutoffAlignment, 20.0f, maximum);
    }

    // The playable range preserves the existing tuning through 80%, then
    // crosses the assumed oscillation boundary only near the upper end.
    static double playingGain(float amount) {
        const double r = std::isfinite(amount) ? std::clamp(double(amount), 0.0, 1.0) : 0.0;
        constexpr double start = double(.8f);
        const double initial = maximumFeedbackGain * start;
        return r <= start ? maximumFeedbackGain * r
                          : initial + (3.8 - initial) * (r - start) / (1.0 - start);
    }
    static std::array<double, 4> playingFeedback(float amount) {
        const double t = std::sqrt(std::sqrt(1.0 + playingGain(amount)));
        return {oddStateFeedback / t, evenStateFeedback / (t * t), oddStateFeedback / (t * t * t),
                1.0};
    }
    static double feedbackLimiting(float amount) {
        const double gain = playingGain(amount);
        const double x =
            std::isfinite(amount)
                ? std::clamp((double(amount) - double(.8f)) / (double(.95f) - double(.8f)), 0.0,
                             1.0)
                : 0.0;
        return gain / (1.0 + gain) * x * x * (3.0 - 2.0 * x);
    }

    // Stable reference curve retained for raw-model characterization.
    static std::array<double, 4> feedback(float amount) {
        const float resonance = std::isfinite(amount) ? std::clamp(amount, 0.0f, 1.0f) : 0.0f;
        const double constant = 1.0 + maximumFeedbackGain * static_cast<double>(resonance);
        const double scale = std::sqrt(std::sqrt(constant));
        // Normalize the polynomial's constant term to one, absorbing its fourth
        // root into the frequency scale. Keeps bass gain at unity without an
        // extra gain stage or resonance-dependent oscillator drive.
        return {oddStateFeedback / scale, evenStateFeedback / (scale * scale),
                oddStateFeedback / (scale * scale * scale), 1.0};
    }
};
}  // namespace cs01
