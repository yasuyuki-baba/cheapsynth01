# CS-01II / IG05630-inspired behavioral model

## Evidence and limits

The CS-01II service drawing identifies IC2 as IG05630; the earlier CS-01 drawing identifies IG02610. The reports supplied for this work suggest an OTA based, four-pole response for IG05630, but the detailed internal topology and control laws remain secondary interpretations. No physical CS-01II or IG05630 measurements are available. This implementation is therefore an **IG05630-inspired behavioral model**, not a circuit reconstruction.

## Implementation

`ExperimentalIG05630` cascades two TPT second-order lowpass sections and places bounded nonlinear feedback around the resonant section. The cascade gives a four-pole LP response. Resonance uses a provisional fourth-power control curve into the feedback loop, with a maximum loop gain of 2.0; this delays self-oscillation to the top of the control range and lowers its maximum level relative to the earlier experimental tuning. These are explicit behavioral hypotheses, not measured IC values. Input drive and output bounds remain separate safety/model parameters. BPF/HPF taps and the alleged internal resonance-control cell are not modeled because no supporting pin-level evidence has been verified.

`CS01IIVCFCircuit` now directly wraps this model. `ModernVCFProcessor` owns parameter and modulation routing; the filter type control selects Original or Modern. There is no Legacy model or model toggle.

## Validation

Automated checks cover finite and bounded output during fast cutoff/resonance changes, bounded impulse resonance, suppression of sustained oscillation at 0.8 resonance while retaining it at maximum resonance, deterministic rendering across block partitions, and the lowpass response shape. These verify numerical behavior, not a match to Yamaha hardware. No THD, resonance curve, self-oscillation threshold, or CV law has been checked against a real IC.

Historical CSVs under `artifacts/characterization` and `artifacts/dsp` contain measurements made while the removed Legacy implementation still existed. They are retained as archival comparisons and cannot be regenerated from the current source tree. New runs characterize the single behavioral implementation.

## Calibration path

Future hardware measurements should record frequency response across cutoff and resonance settings, resonance peak and onset, self-oscillation amplitude/frequency, level-dependent harmonics, and cutoff/resonance modulation transients at documented host rates. Fit the named empirical damping, feedback, and saturation parameters against those measurements, and keep the source data and fitting method alongside any parameter changes.
