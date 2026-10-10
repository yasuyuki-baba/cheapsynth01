# External-circuit audit closeout

## Subsequent provisional implementation

Production EG now uses explicit idle, attack, decay, sustain and release states
with a stateful exponential recurrence and provisional curvature k = 2. Each
moving stage retains its current level. A fresh attack from zero and newly
entered decay/release use the configured reference duration; an attack
retriggered from a residual level reaches its endpoint sooner because its
provisional branch target is fixed. Active-stage time edits change the speed
and recalculate the remaining threshold time without restarting a full stage
or redefining its target. Sustain edits select a new decay branch.
Parameter identifiers, ranges and preset units remain unchanged. See
[the current EG policy](EG-stateful-model.md) for implementation and regression
coverage, and [note onset continuity](Note-onset-continuity.md) for the separate
VCA note gate and free-running sources.

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
reconstruction or hardware calibration of the CS-01. At the historical audit
stage, production DSP, parameter ranges and presets were unchanged; subsequent
software changes are summarized above. The schematics and manufacturer
documents inspected then support connections and conditional equations, but
do not provide all loaded device operating points needed for a calibrated
replacement. Originals are absent from this checkout; see the
[source catalog](Source-catalog.md).

The later [EG circuit audit correction](EG-model-audit.md) flags an electrically
inconsistent Tr13 attack-drive transcription. Device polarity, terminals and
IC4 output mapping are unresolved, so the archived conditional attack-drive
sequence is not established hardware behavior.

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

## Analysis limits

- Gate, IC4, storage-node E and TP4 transition voltages/timing are not calibrated.
- Transistor/FET parameters, temperature and IC4 output loading are not identified
  for the envelope's operating points; high-current datasheet limits do not
  establish low-current behavior.
- Conditional junction-drop equations do not constitute a calibrated nonlinear
  device model across cutoff, saturation or near-zero current.
- Slider resistance curves, stage thresholds and buffer transfer are unmeasured.
- Audio-coupling source/load impedances, including custom-IC inputs, remain unknown.

Production retains the provisional stateful exponential EG. Its software
regressions and curvature do not establish hardware calibration.
