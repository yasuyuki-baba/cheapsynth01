# Stateful provisional EG

The production EG replaces the previous shaped JUCE ADSR output with explicit
idle, attack, decay, sustain and release states. It is a musical approximation,
not a transistor or custom-IC reconstruction.

Each moving stage retains its current level and follows an exponential approach
to a provisional branch target, using double-precision state and curvature 2.
For a fresh attack from zero, and a newly entered decay/release, the existing
curve remains `y(t) = y0 + (end-y0)*(1-exp(-2*t/T))/(1-exp(-2))`.
The parameter T denotes that reference transition time; the branch time constant
is T/2. Neither value is a calibrated CS-01 RC constant.

Attack always approaches `1/(1-exp(-2))`, independent of the current level.
Retriggering from a residual level therefore shortens the remaining attack.
An attack already at its peak enters decay on the next sample. Decay and release
retain their previous provisional entry-level-derived targets: loaded sustain
behavior and the physical low-current release tail have not been reconstructed.

For target U, current level y and endpoint E, remaining time is
`(T/2)*ln(abs((U-y)/(U-E)))`, rounded upward to samples. A branch at its endpoint
advances on the next sample. A zero-distance decay/release retains its reference
clock; this allows the independent non-EG gate to release when sustain is zero.
Time edits rescale that clock by the new/old reference time. Endpoints remain clamped
exactly for bounded software state and lifecycle management. No per-sample
transcendental evaluation is required.

Attack charges toward 1, decay toward sustain, release toward 0. Retrigger
continues charging toward the same attack target from the current value.
Editing the active stage's time changes only its coefficient and recalculates
the remaining time to the existing endpoint, retaining its level and target.
Sustain edits during decay/sustain slew to the new value using decay time,
including upward moves. Unrelated edits do not alter an ongoing release.
Immediate stop, resource release and prepare clear level and activity.

Parameter identifiers, ranges and stored values are unchanged; sound changes.
The production VCA also consumes a separate [note gate](Note-onset-continuity.md)
for its non-EG gain while the oscillator runs continuously. This does not change
the EG output or stage durations described here.
Repeated time edits integrate the changed speeds rather than restarting a full
transition each time. Sustained slow settings can still delay completion.
No claims are made about physical switching voltages or measured timing.

Tests retain sample-accurate duration checks at five rates and block-partition
checks, and use the new analytical release midpoint and slope bounds. The graph
peak detector now uses exponential boundary slopes (attack end approximately
0.314/T; decay start at sustain 0.5 approximately 1.157/T). Its five-observation
timing budget covers the inferred peak window and stage-boundary uncertainty;
the independent single-sample duration test retains its original tolerance.
An additional test checks sustain-edit and release-to-attack continuity.
Historical validation before the residual-state update: all 185 tests from 38 suites
passed with `--all`, including observations (exit status 0).

Earlier documents describing linear ADSR or k=0.5 shaping describe superseded
implementation stages. This document describes the current production policy.

## Circuit-analysis boundary

The branch update adopts the capacitor-state principle that changing resistance
changes speed without resetting stored voltage or redefining the drive target.
It does not integrate illustrative Ebers-Moll coefficients as device calibration.
The previous Tr13 transcription (PNP, emitter at -9 V, low-output activation)
is electrically inconsistent with a normal forward-operated PNP switch and must
be rechecked against the original schematic, including the output-pin mapping.
The attack target, k=2 curve, 1 ms–2 s ranges and control taper remain provisional.
This update does not establish click removal or hardware timing agreement.

New regressions compare retriggered and held attack trajectories, measure the
remaining attack after releasing from several levels, check the independent
continuous solution across speed edits in all moving stages, and compare repeated
edits/retriggers across block sizes 1, 7, 64 and 256. A zero-sustain regression
checks that release edits preserve the separate VCA gate's clock through silence.

Validation of the residual-state update: all 270 tests from 58 suites passed in
one run, including optional audio/stage captures. The Debug Standalone build,
self-contained header checks, pinned clang-format, whitespace check and four
independent EG decay algebra checks passed. No hardware/listening calibration
is claimed by these checks.
