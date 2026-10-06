# DSP characterization baselines

Run `python3 tools/dsp_characterize.py` from the repository root to regenerate
the CSV files in `artifacts/dsp/`. `--seconds N` controls benchmark duration.
The benchmark reports host and nominal internal rate, 64-sample block size,
render count, elapsed time, ns/sample and realtime factor. It covers 44.1, 48,
and 96 kHz, low/high resonance, static and modulated cutoff. Its coefficient
update count is an estimate from the current IG02610 structure: one update per
processed sample. The code calls the current coefficient equations including
their sine, cosine and power functions; it is a focused kernel microbenchmark,
not a complete JUCE graph timing. Whole-graph oversampling is consequently not
measured here.

## Meaning and limits

These outputs are regression characterization: deterministic descriptions of
the current model and its observed numeric behavior, useful for comparing a
future code revision against this baseline. They are not correctness limits,
and benchmark timings are machine, compiler and load dependent.

The VCF response uses the current IG02610 biquad design equation. The
harmonics CSV is a documented polynomial coloration estimate, not measured
steady-state THD from a rendered plugin. VCA rows apply the listed empirical
one-pole high-pass/coupling approximations independently and in combinations;
the stage mask enumerates which are included. Its phase is the sum of the
one-pole phase responses. Frequencies include the low notes associated with
32-foot operation. VCO rows are ideal analytic Fourier references, rather than
the oversampled YM10150 output. EG trajectory is a reproducible linear ADSR
reference, not a capture of JUCE's stateful envelope implementation. Thus the
non-VCF CSVs are starting reference probes and not execution captures of the
plugin.

Hardware calibration means comparing the model with controlled measurements
of an identified physical CS-01, with documented instrument, loading, signal
level, component condition and uncertainty. These files do not establish CS-01
hardware accuracy: several model stages are explicitly empirical, and the
response/harmonic/VCO/EG probes above include idealized references. When
measured hardware data becomes available, preserve raw measurements and
metadata separately, then align sample rate, input level, control settings,
loading and measurement bandwidth before plotting residuals against these
model outputs. Never treat a model-to-model match as hardware validation.

## Current cost observations

The dominant known Original VCF cost is coefficient recalculation in
`IG02610::processSample`: every sample adjusts effective cutoff from the
smoothed input level and invokes `updateCoefficients`, which computes sine,
cosine and power terms. The processor also computes three `exp2` modulation
ratios per sample, even when modulation inputs are zero. This makes the
Original VCF cost scale with internal processing rate (including oversampling
when the graph runs at that rate). The committed microbenchmark isolates the
filter coefficient kernel, so it cannot rank total graph stages or assign a
whole-graph oversampling multiplier.

No wall-clock thresholds or uncalibrated CI assertions are defined.

For a comparative probe of the legacy and experimental Original VCF cores,
run `python3 tools/original_vcf_ab_characterize.py`. It writes
`artifacts/dsp/original_vcf_ab.json`; scope and limitations are recorded in
[the experimental VCF design note](Original-VCF-experimental-design.md).
Compiled filter-path timing, response, and harmonic observations are recorded
in `artifacts/dsp/original_vcf_cpp_benchmark.csv`,
`artifacts/dsp/original_vcf_cpp_response.csv`, and
`artifacts/dsp/original_vcf_cpp_harmonics.csv`. They cover Debug and Release
cores with shared coupling stages, not full-graph or hardware behavior.

## Legacy coefficient-update optimization

`IG02610::processSample` always rebuilds coefficients from the input-level
adjusted cutoff before reading them. The per-sample control setters also used
to rebuild coefficients, so in the modulated path those setter results were
discarded before they could affect audio. The setters now only clamp/store
their controls; the sample function still calculates and uses the same
per-sample coefficients, including the same smoothed input-level cutoff
behavior. This removes redundant transcendental work without changing the
filter equation or its modulation rate.

The comparative C++ probe used 65,536 samples at 4x internal rate, in Debug
and Release builds, with static and sinusoidally modulated cutoff. One before
and one after timing were recorded for each case, so treat them as an
observation rather than a machine-independent guarantee. `artifacts/dsp/original_vcf_legacy_update_optimization.csv`
contains the paired values. Across the six cases per build, the observed
legacy runtime fell by about 32–40% in Debug and 38–44% in Release. Output
remained finite; the coefficient math and nonlinear path are unchanged. The
per-sample coefficient calculation remains the next significant cost, and
the processor's separate three-`exp2` modulation calculation has not yet been
optimized.
