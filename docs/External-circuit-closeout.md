# External-circuit audit closeout

## Subsequent provisional implementation

Production EG now uses explicit idle, attack, decay, sustain and release states
with a stateful exponential recurrence and provisional curvature k = 2. Each
moving stage begins at the current level and reaches its endpoint after the
configured duration. Retriggers and active-stage edits preserve level continuity;
sustain edits slew using decay time. Parameter identifiers, ranges and preset
units remain unchanged. See [the current EG policy](EG-stateful-model.md) for
implementation and regression coverage.

This software policy supersedes both the linear JUCE ADSR and the intermediate
k = 0.5 output-shaping implementation. Neither the current curvature nor the
intermediate shape is verified hardware behavior. Transistor operating points,
slider taper, switching thresholds and output-buffer calibration remain incomplete.
The unchanged-DSP decisions below refer to the historical audit phase.

Historical verification of the intermediate k = 0.5 implementation: 24
EG/Envelope/RC tests passed after updating midpoint/slope expectations, and all
182 tests from 38 suites passed with `--all`. These are historical results, not
validation counts for the current implementation.

## Scope and decision

This closes the currently supportable external-circuit analysis pass, not the
reconstruction or hardware calibration of the CS-01. Production DSP, parameter
ranges and presets are unchanged. Existing local schematics and manufacturer
documents support connections and conditional equations, but do not provide all
loaded device operating points needed for a calibrated replacement.

## Completed evidence and implementation work

| Area | Completed | Remaining limitation |
| --- | --- | --- |
| EG logic | TC7476BP truth table, supply, fixed inputs, reset diodes and feedback mapped; conditional stage sequence recorded | Analog input thresholds, gate levels, loaded outputs and transient behavior |
| EG passive branches | 2.2 uF storage, 1.8 kohm plus variable stage resistances and sustain Thevenin equivalent recorded | Tolerances, actual slider curves and active-device loading |
| Decay/sustain coupling | Conditional fixed-drop equations and effective time constant derived; executable algebra checks added | Current-dependent junction behavior and operating-region transitions |
| Release | Tr9 path and loaded Tr8 drive identified | Loaded drive, low-current equilibrium and output-buffer transfer |
| Buffer | FET1/Tr14 connection and physical output polarity recorded | Device operating points, gain, offsets and nonlinear behavior |
| Audio coupling | Existing low-frequency audit distinguishes empirical poles from schematic evidence | Source/load impedances, including custom-IC inputs |
| Controls | Existing timing audit distinguishes software seconds/skew from hardware S-L markings | Position/resistance measurements and stage endpoint calibration |

Detailed records: [EG circuit audit](EG-model-audit.md),
[EG time-range validation](EG-time-range-validation.md), and
[low-frequency circuit audit](Low-frequency-circuit-audit.md).

## Verification performed

- `python3 Tests/eg_decay_equation_check.py -v`: four tests passed, including
  72 algebra-check combinations and explicit invalid-region examples.
- Existing local Debug test executable, filter
  `*EG*:*Envelope*:*RCEnvelope*`: 24 tests passed from seven suites.
  This run used the already-built executable, not a fresh rebuild. Its result
  confirms that executable's regression behavior, not source/build freshness.
- `git diff --check`: used to check edited tracked text for whitespace errors.

These checks verify equations and software behavior, not measured sound or
hardware timing. The Python checks are separate from the CMake test runner.

## Evidence gates for the next implementation

1. Record gate-input, IC4 pin-7/pin-8/pin-14/pin-15, storage-node E and TP4
   voltages across key-on, attack completion and key-off. Preserve both
   steady-state levels and transition timing; do not substitute normalized EG.
2. Establish applicable transistor/FET model parameters and temperature, with
   provenance. Datasheet high-current beta or saturation test limits alone do
   not identify low-current envelope behavior. Include IC4 output loading.
3. Solve transistor junction currents and passive node balances together.
   Check residuals and continuity across cutoff/saturation and near zero
   current before using the result as a stage model.
4. Measure slider position versus resistance and storage/output trajectories
   at several A/D/R/S settings. Separate stage threshold time from RC constant
   and buffer offset. Preserve parameter/preset compatibility explicitly.
5. For audio coupling, establish source/load impedances or measured response
   before changing empirical poles. Custom-IC-dependent loads remain outside
   this external-only pass.

Do not introduce guessed junction constants, arbitrary region thresholds or
uncalibrated sustain targets into production merely to mark the work complete.
When these evidence gates are met, add independently expected circuit tests,
replace the corresponding approximation, rebuild, and run full regression and
observation categories. Until then, retain the provisional stateful exponential EG and its software
regressions; do not describe its curvature as hardware-calibrated.
