# Whole-graph oversampling validation

The shared setting in `SynthConstants.h` selects 4x internal generation.
Production runs the complete AudioProcessorGraph at 4x the host sample rate:
tonal VCO, noise, EG, LFO, the selected VCF and VCA. ToneGenerator uses external
oversampling in this graph and does not add a second oversampling stage. The
standalone ToneGenerator API retains its separate local oversampler for unit tests.
JUCE polyphase IIR downsampling is used in production. FIR experiments are not
evidence of the production filter's response.

## Automated checks

- Production pitch: all five waveforms, three notes, four feet settings and
  44.1/48/96 kHz, with a 0.1% frequency tolerance.
- Re-preparation reproduces a fresh generator's output in the tested conditions.
- Scalar and partitioned block rendering agree.
- VCO LFO modulation is consumed per sample; partition sizes 1/7/64/256 agree.
- Alias observations use 3/5 kHz fundamentals and selected folded bins across
  all five waveforms. These are observations, not total alias-power limits.
- The spectral measurement has independently specified sinusoidal controls.

Historical VCO-only experiment (original report omitted commit/toolchain metadata;
not a current graph measurement): Debug and Release reportedly passed 99 tests.
The local Release VCO-only benchmark at
48 kHz took approximately 5.8–9.1 ms per audio second. This is not a whole-plugin
CPU benchmark or a real-time deadline guarantee; allocation counting was not done.

## Limits

Selected bins still reach approximately -35 dBc for a 5 kHz sawtooth.
Oversampling does not make all waveforms alias-free.
The sample-wise implementation is retained; no block-processing optimization
was justified by the VCO-only timing observation.

In tested high-note conditions output first exceeded 1e-5 within one period.
This is an onset observation, not a latency measurement. IIR group delay depends
on frequency; no fixed host latency or EG alignment compensation was added.
Glissando is tested against a separately scheduled semitone progression at all
three sample rates. A 2 Hz PWM setting is tested using carrier-cycle means:
0.5-second repetition is distinguished from a 0.25-second shift. This validates
the tested setting, not the full PWM speed range.

The graph now renders segments separated by MIDI event positions, because its
MIDI processor previously applied all events before generating audio. A graph
test verifies silence before a mid-block note-on and audible output afterwards
with 64/256-sample blocks. MIDI control is explicitly applied before graph audio
rendering, since direct generator references do not impose graph execution order.
A separate test verifies that mid-block note-off leaves earlier output unchanged
and changes output after the event. The audit adds whole-graph release lifetime checks with partitions 1/7/64,
Tone/Noise switching and depth 0/1. These distinguish source termination from
filter/coupling/downsampling residual decay; they do not assert bitwise full-graph
partition equivalence. No claim of complete sample-accurate
envelope/modulation alignment is made. OS/device latency and hardware fidelity
are outside these checks.
Current audit conditions, source revision, commands and limitations: [Audit-stability.md](Audit-stability.md).
Ideal-model CSVs and historical FIR/VCO-only experiments are not production DSP measurements.
