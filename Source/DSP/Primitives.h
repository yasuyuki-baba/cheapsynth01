#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numbers>
namespace cs01 {
// Independent DSP primitives; no JUCE implementation code is used.
class LinearRamp {
   public:
    void reset(double rate, double seconds) {
        length = std::max(1, static_cast<int>(rate * seconds));
        remaining = 0;
    }
    void setCurrentAndTargetValue(float v) {
        current = target = v;
        remaining = 0;
    }
    void setTargetValue(float v) {
        if (v == target)
            return;
        target = v;
        remaining = length;
        increment = (target - current) / length;
    }
    float getNextValue() {
        if (remaining > 0) {
            current += increment;
            if (--remaining == 0)
                current = target;
        }
        return current;
    }

   private:
    float current = 0, target = 0, increment = 0;
    int length = 1, remaining = 0;
};
class Oscillator {
   public:
    void prepare(double rate) {
        sampleRate = rate;
        reset();
    }
    void initialise(std::function<double(double)> f) {
        waveform = std::move(f);
    }
    void setFrequency(double hz, bool immediate = false) {
        if (hz == target && !immediate)
            return;
        target = hz;
        if (immediate) {
            frequency = target;
            remaining = 0;
        } else {
            remaining = std::max(1, static_cast<int>(sampleRate * .05));
            step = (target - frequency) / remaining;
        }
    }
    void reset() {
        phase = 0;
        frequency = target;
        remaining = 0;
    }
    double processSample(double input = 0) {
        const double out = input + waveform(phase - std::numbers::pi);
        if (remaining > 0) {
            frequency += step;
            if (--remaining == 0)
                frequency = target;
        }
        phase += 2 * std::numbers::pi * frequency / sampleRate;
        phase -= 2 * std::numbers::pi * std::floor(phase / (2 * std::numbers::pi));
        return out;
    }

   private:
    std::function<double(double)> waveform = [](double p) { return std::sin(p); };
    double sampleRate = 44100, phase = 0, frequency = 440, target = 440, step = 0;
    int remaining = 0;
};
class Biquad {
   public:
    void lowPass(double rate, double hz) {
        configure(rate, hz, false);
    }
    void highPass(double rate, double hz) {
        configure(rate, hz, true);
    }
    void firstOrderLowPass(double rate, double hz) {
        const double k = std::tan(std::numbers::pi * hz / rate);
        b0 = static_cast<float>(k / (1 + k));
        b1 = b0;
        b2 = 0;
        a1 = static_cast<float>((k - 1) / (k + 1));
        a2 = 0;
    }
    float processSample(float x) {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() {
        z1 = z2 = 0;
    }

   private:
    void configure(double rate, double hz, bool high) {
        const double w = 2 * std::numbers::pi * std::clamp(hz, 1.0, rate * .49) / rate;
        const double c = std::cos(w), alpha = std::sin(w) / std::sqrt(2.0), inv = 1 / (1 + alpha);
        b0 = static_cast<float>((high ? 1 + c : 1 - c) * .5 * inv);
        b1 = static_cast<float>((high ? -(1 + c) : 1 - c) * inv);
        b2 = b0;
        a1 = static_cast<float>(-2 * c * inv);
        a2 = static_cast<float>((1 - alpha) * inv);
    }
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};
// Linear-phase Blackman-window FIR. 129 taps at 4x => exactly 16 host samples.
// Feed four samples per output, beginning at internal sample zero.
class OutputConverter {
   public:
    static constexpr int factor = 4, taps = 129, latency = 16;
    OutputConverter() {
        double sum = 0;
        for (int i = 0; i < taps; ++i) {
            const int n = i - (taps - 1) / 2;
            constexpr double fc = .1125;  // 90% of host Nyquist at the internal rate.
            const double sinc =
                n == 0 ? 2 * fc : std::sin(2 * std::numbers::pi * fc * n) / (std::numbers::pi * n);
            const double w = .42 - .5 * std::cos(2 * std::numbers::pi * i / (taps - 1)) +
                             .08 * std::cos(4 * std::numbers::pi * i / (taps - 1));
            coefficients[i] = sinc * w;
            sum += coefficients[i];
        }
        for (auto& c : coefficients)
            c /= sum;
    }
    void reset() {
        history.fill(0);
        cursor = 0;
    }
    void push(float sample) {
        history[cursor] = sample;
        cursor = (cursor + 1) % taps;
    }
    float output() const {
        double result = 0;
        int index = (cursor + taps - 1) % taps;
        for (int i = 0; i < taps; ++i) {
            result += coefficients[i] * history[index];
            index = (index + taps - 1) % taps;
        }
        return static_cast<float>(result);
    }

   private:
    std::array<double, taps> coefficients{};
    std::array<float, taps> history{};
    int cursor = 0;
};
struct EnvelopeSettings {
    float attack = .1f, decay = .1f, sustain = .8f, release = .1f;
};
}  // namespace cs01
