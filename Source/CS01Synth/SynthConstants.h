#pragma once

#include <cstddef>

namespace Constants {
// Shared processing-quality setting: factor must remain a power of two.
constexpr std::size_t oversamplingStages = 2;
constexpr std::size_t oversamplingFactor = std::size_t{1} << oversamplingStages;
constexpr float pitchBendSemitones = 12.0f;
constexpr float pitchBendMaxValue = 8192.0f;
constexpr float maxGlissandoPerSemitoneSeconds = 0.208f;
}  // namespace Constants

enum class Feet { Feet32, Feet16, Feet8, Feet4, WhiteNoise };

enum class Waveform { Triangle, Sawtooth, Square, Pulse, Pwm };

enum class LfoTarget { Vco, Vcf };
