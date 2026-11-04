#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace cs01 {
// Numerical solver only: N cascaded trapezoidal integrators with instantaneous
// weighted state feedback. Each IC model supplies its assumed feedback topology.
template <std::size_t Stages>
class VCFIntegratorCore {
    static_assert(Stages > 0);

   public:
    void prepare(double rate) {
        sampleRate = std::max(1.0, rate);
        reset();
    }

    void reset() {
        integrators.fill(0.0);
    }

    void setCutoffFrequency(float frequency) {
        cutoff = std::clamp(std::isfinite(frequency) ? static_cast<double>(frequency) : 1000.0,
                            20.0, sampleRate * 0.45);
    }

    void excite(double amount) {
        integrators[0] += amount;
    }

    // feedback[i] weights the output of integrator i (denominator coefficient
    // of p^(Stages-i-1)); the leading p^Stages coefficient is one.
    double processSample(double input, const std::array<double, Stages>& feedback,
                         double limitingGain = 0.0, double limit = 0.25) {
        const double g = std::tan(std::numbers::pi_v<double> * cutoff / sampleRate);
        double offset = 0.0;
        double numerator = input;
        for (std::size_t i = 0; i < Stages; ++i) {
            offset = g * offset + integrators[i];
            numerator -= feedback[i] * offset;
        }
        double denominator = feedback.back();
        for (std::size_t i = Stages - 1; i > 0; --i)
            denominator = feedback[i - 1] + g * denominator;
        const double linearDenominator = 1.0 + g * denominator;
        double state = numerator / linearDenominator;
        if (limitingGain > 0.0) {
            // Replace part of the last-state feedback by L*tanh(output/L).
            // Solve in the same sample; saturation acts inside the loop.
            const double power = std::pow(g, Stages);
            const double baseDenominator = linearDenominator - limitingGain * power;
            const double rhs = numerator + limitingGain * offset;
            double low = (rhs - limitingGain * limit) / baseDenominator;
            double high = (rhs + limitingGain * limit) / baseDenominator;
            state = std::clamp(state, low, high);
            for (int iteration = 0; iteration < 40; ++iteration) {
                const double shape = std::tanh((power * state + offset) / limit);
                const double residual =
                    baseDenominator * state + limitingGain * limit * shape - rhs;
                if (std::abs(residual) < 1e-12 * (1.0 + std::abs(rhs)))
                    break;
                if (residual > 0)
                    high = state;
                else
                    low = state;
                const double derivative =
                    baseDenominator + limitingGain * power * (1.0 - shape * shape);
                const double next = state - residual / derivative;
                state = next > low && next < high ? next : (low + high) * .5;
            }
        }
        for (double& memory : integrators) {
            const double output = g * state + memory;
            memory = 2.0 * output - memory;
            state = output;
        }
        return state;
    }

   private:
    double sampleRate = 44100.0;
    double cutoff = 1000.0;
    std::array<double, Stages> integrators{};
};
}  // namespace cs01
