#pragma once
#include <algorithm>
#include <cmath>

namespace cs01 {
// Streaming windows keep short peaks intact across arbitrary host block sizes.
// Analysis does not change the audio or allocate on the rendering thread.
class OutputMeter {
   public:
    struct Reading {
        float peak = 0.f;
        float rms = 0.f;
    };
    OutputMeter() {
        reset(48000.);
    }
    void reset(double sampleRate) {
        if (!std::isfinite(sampleRate) || sampleRate < 1000.)
            sampleRate = 48000.;
        window = std::max(1, static_cast<int>(sampleRate * .02));
        hold = std::max(window, static_cast<int>(sampleRate * .5));
        attack = std::exp(-double(window) / (sampleRate * .01));
        decay = std::exp(-double(window) / (sampleRate * .25));
        count = age = 0;
        peak = heldPeak = smoothed = 0.f;
        squares = 0.;
    }
    bool process(float sample, Reading& reading) {
        if (!std::isfinite(sample))
            sample = 0.f;
        peak = std::max(peak, std::abs(sample));
        squares += double(sample) * sample;
        if (++count < window)
            return false;
        age += window;
        if (peak >= heldPeak || age >= hold) {
            heldPeak = peak;
            age = 0;
        }
        const double rms = std::sqrt(squares / window);
        const double coefficient = rms > smoothed ? attack : decay;
        smoothed = static_cast<float>(rms + coefficient * (smoothed - rms));
        if (smoothed < 1.e-7f)
            smoothed = 0.f;
        reading = {heldPeak, smoothed};
        count = 0;
        squares = 0.;
        peak = 0.f;
        return true;
    }

   private:
    int window = 960, hold = 24000, count = 0, age = 0;
    double attack = 0., decay = 0., squares = 0.;
    float peak = 0.f, heldPeak = 0.f, smoothed = 0.f;
};
}  // namespace cs01
