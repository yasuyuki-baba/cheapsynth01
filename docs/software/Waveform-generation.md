# Waveform generation from a shared source

The tonal generator uses one shared pitch/phase cycle to produce the selected
waveform. The waveform choices are different transformations of that common
source; they are not five independently tuned oscillators running in parallel.
This document describes the current software. The corresponding hardware
interpretation is recorded in [the YM10150 summary](../hardware/ymf10150.md).

## Shared pitch, phase and square wave

`ToneGenerator` combines the note/glissando position, pitch bend, fine pitch,
pitch LFO and feet octave offset. For that resulting semitone pitch `p`,
`YM10150::generateMasterSquareWave` calculates

```text
frequency = 440 * 2^((p - 69) / 12)
phaseIncrement = frequency / processingSampleRate
```

The shared phase runs through one cycle from 0 to 1. A 50%-duty square is
positive in the first half and negative in the second half. PolyBLEP corrects
its discontinuities to reduce digital aliasing; `tanh(1.2 * square)` then shapes
its amplitude. These are software waveform corrections, not a reconstructed
YM10150 digital clock/divider circuit.

For each generated sample, the master square is evaluated at the current phase,
then the phase advances. The selected waveform strategy receives that square,
the advanced phase and the shared phase increment. This ordering is part of
the implementation; a phase-derived branch is not evaluated at the same phase
instant as the already generated master square.

```mermaid
flowchart TD
    Pitch[Note and pitch controls] --> Cycle[Shared phase and phase increment]
    Cycle --> Square[50% master square with PolyBLEP and shaping]
    Cycle -->|Phase and increment| Strategy[Selected waveform strategy]
    Square --> Strategy
    PWM[Separate PWM modulation oscillator] -->|PWM choice| Strategy
    Strategy --> Shape[Common output shaping]
    Shape --> Output[Tonal VCO output]
```

## How each waveform is obtained

| Choice | Inputs and transformation | Stateful behavior |
| --- | --- | --- |
| Triangle | Integrates the master square, producing alternating rising/falling slopes. Applies leaky integration, DC removal, amplitude scaling and a sine-based coloration term. | Retains an integrator and DC-removal state. |
| Sawtooth | Uses a descending ramp `1 - 2*phase`, adds a small leaky state driven by the master-square sign, then blends ramp and sine-shaped output. It is not obtained solely by integrating the square. | Retains the leaky correction state. |
| Square | Returns the shaped 50% master square to the common output stage. | No additional waveform-strategy state. |
| Pulse | Compares the shared phase with a fixed width of 0.25, then applies PolyBLEP edge correction and `tanh` shaping. | No additional waveform-strategy state. |
| PWM | Compares the shared phase with a varying width `0.5 + 0.4*PWM_LFO`, bounded to 0.05–0.95. Applies PolyBLEP, `tanh` and a small smoothed-output blend. | Retains the smoothing state; pulse width is driven by its separate modulation oscillator. |

All selected strategy outputs pass through `YM10150::shapeOutput`, which applies
`tanh(1.2 * value)`. Square therefore also passes through this final shaping;
it is not an unaltered copy of an ideal square. The gains, leakage, coloration
and smoothing are empirical software choices. The shared timing relates the
waveforms' pitch; it does not make their amplitude, spectrum or phase identical.
The code evaluates the selected strategy rather than mixing all five outputs.

PWM Speed controls the separate pulse-width oscillator; LFO Speed and LFO Target
control pitch/cutoff modulation. Changing width changes the pulse shape within
the common pitch cycle, not the nominal note frequency. Selecting Feet = WN
uses the separate `NoiseGenerator` instead of this tonal generation chain.

Production generates at 4x the host rate and downsamples at the output, as
specified in [oversampling and timing](Oversampling-validation.md). Neither
oversampling nor PolyBLEP establishes alias-free output or hardware equivalence.
After waveform generation, VCF/VCA/EG control the resulting tone as described in
[the user guide](User-guide.md) and [DSP signal paths](DSP-responsibility-boundaries.md).

## Implementation references

- [ToneGenerator.cpp](../../Source/CS01Synth/ToneGenerator.cpp): pitch composition and generation order.
- [YM10150.cpp](../../Source/CS01Synth/YM10150.cpp): master square, strategy selection and final shaping.
- [WaveformStrategies.h](../../Source/CS01Synth/WaveformStrategies.h): per-waveform transformations and empirical state coefficients.
