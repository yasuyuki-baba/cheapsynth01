# CS-01II / IG05630-inspired behavioral model

## Evidence and limits

The CS-01II service drawing identifies IC2 as IG05630; the earlier CS-01 drawing identifies IG02610. The reports supplied for this work suggest an OTA based, four-pole response for IG05630, but the detailed internal topology and control laws remain secondary interpretations. No physical CS-01II or IG05630 measurements are available. This implementation is therefore an **IG05630-inspired behavioral model**, not a circuit reconstruction.

## Implementation

`IG05630BehavioralModel` cascades two TPT second-order lowpass sections and places bounded nonlinear feedback around the resonant section. The cascade gives a four-pole LP response. Resonance uses a provisional fourth-power control curve into the feedback loop, with a maximum loop gain of 1.2 to keep this implementation below its modeled self-oscillation threshold. This non-oscillating behavior is a conservative hypothesis because the service material found so far does not establish the IC's self-oscillation behavior; it is not a verified property of the hardware. Input drive is an empirical model parameter; the output bound is an implementation-only `SafetyParameters` value. BPF/HPF taps and the alleged internal resonance-control cell are not modeled because no supporting pin-level evidence has been verified.

`CS01IIVCFCircuit` now directly wraps this model. `ModernVCFProcessor` owns parameter and modulation routing; the filter type control selects Original or Modern. There is no Legacy model or model toggle.

## Validation

Automated checks cover finite and bounded output during fast cutoff/resonance changes, impulse-response decay at maximum resonance, deterministic rendering across block partitions, and the lowpass response shape. These verify numerical behavior, not a match to Yamaha hardware. No THD, resonance curve, self-oscillation threshold, or CV law has been checked against a real IC.

Historical CSVs under `artifacts/dsp` contain measurements made while the removed Legacy implementation still existed. They are retained as archival comparisons and cannot be regenerated from the current source tree. The previously cited `artifacts/characterization` directory is absent from this checkout. New runs characterize the single behavioral implementation; see [DSP baselines](../dsp-baselines.md) for archive scope and provenance.

## Calibration path

Future hardware measurements should record frequency response across cutoff and resonance settings, resonance peak and onset, self-oscillation amplitude/frequency, level-dependent harmonics, and cutoff/resonance modulation transients at documented host rates. Fit the named empirical damping, feedback, and saturation parameters against those measurements, and keep the source data and fitting method alongside any parameter changes.

See [DSP responsibility boundaries](../DSP-responsibility-boundaries.md) for the production signal paths and safety ownership.
