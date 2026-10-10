# VCF musical-model validation: first implementation pass

> Historical implementation pass: this report describes the earlier biquad and
> output-coloration model. The current Original VCF uses a nonlinear TPT
> state-variable model; see [Original VCF behavioral design](Original-VCF-behavioral-design.md)
> and [DSP responsibility boundaries](DSP-responsibility-boundaries.md).
> Test counts and before/after measurements below belong to this earlier pass.

## Goal and scope

Improve consistency of the existing provisional VCF without claiming custom-IC
reconstruction. This pass retains the double-precision biquad and existing
control mappings. EG redesign, VCA/breath smoothing and nonlinear resonance
feedback are subsequent work, not implemented by this pass.

## Changes

- Replace the piecewise tanh approximation with `std::tanh`; the previous
  expressions disagreed at absolute input 1.
- Retain the input hard safety clamp and identify it accurately in comments.
- Blend empirical coloration continuously at resonance 0.4 and 0.7 rather
  than dropping one branch before enabling the next.
- Interpolate the high-resonance frequency coloration between 500 and 5000 Hz
  rather than switching abruptly at those cutoffs.
- Recompute effective coefficients for each sample, including near-zero level
  modulation, without retaining a previous modulation value's coefficients.
- Bound coefficient-design frequency to 0.45 times the processing rate.
- Correct the cubic-coloration description to odd harmonics.

This is still output coloration following a linear resonant filter, not
saturation inside a resonance feedback loop. It intentionally changes some
large-signal behavior. Per-sample coefficient calculation and standard tanh
were not evaluated for whole-plugin performance in this pass; small-signal
timing is not a CPU benchmark.

## Reproducible checks

New `IG02610ControlTest` cases in `Tests/unit/CS01VCFCircuitTest.cpp` compare
near-identical trajectories on both sides of resonance 0.4/0.7 at input
amplitudes 0.01, 0.8 and 1.2, and verify that reapplying identical controls
does not change the trajectory. The five control tests, including existing
panel-response observations and live-control safety checks, passed after rebuild.
The rebuilt executable also passed all 184 tests from 38 suites with `--all`,
including observation tests (exit status 0).

Before/after `Observation_ProductionPanelResponse` logs were compared at 112
matching small-signal points. Maximum difference in printed gains was about
0.00001 dB. Logs are `/tmp/vcf-before.log` and `/tmp/vcf-after.log`; these
temporary files are not permanent fixtures or hardware calibration data.

No comparison WAV files or listening judgment are supplied by this pass.
This pass does not establish large-signal listening quality, modulation-click
behavior, low-end balance, whole-plugin CPU cost or a hardware need for feedback
saturation. Passing safety tests does not establish musical superiority.
