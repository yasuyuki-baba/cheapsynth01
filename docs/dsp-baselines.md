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
