#pragma once

namespace Constants {
constexpr float pitchBendSemitones = 12.0f;
constexpr float pitchBendMaxValue = 8192.0f;
constexpr float maxGlissandoPerSemitoneSeconds = 0.208f;
}  // namespace Constants

enum class Feet { Feet32, Feet16, Feet8, Feet4, WhiteNoise };

enum class Waveform { Triangle, Sawtooth, Square, Pulse, Pwm };

enum class LfoTarget { Vco, Vcf };
