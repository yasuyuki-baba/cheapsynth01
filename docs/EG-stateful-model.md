# Stateful provisional EG

The production EG replaces the previous shaped JUCE ADSR output with explicit
idle, attack, decay, sustain and release states. It is a musical approximation,
not a transistor or custom-IC reconstruction.

Each moving stage starts at the current level and follows
`y(t) = y0 + (end-y0)*(1-exp(-2*t/T))/(1-exp(-2))`.
The equivalent one-pole recurrence uses an overshoot target and double-precision
state. Curvature 2 is provisional. The endpoint is set exactly after
`ceil(T*sampleRate)` samples; therefore seconds denote completion time, not an
RC time constant. No per-sample transcendental evaluation is required.

Attack charges toward 1, decay toward sustain, release toward 0. Retrigger
starts a complete attack from the current value. Editing the active stage's
time restarts that stage from its current value with the new full duration.
Sustain edits during decay/sustain slew to the new value using decay time,
including upward moves. Unrelated edits do not alter an ongoing release.
Immediate stop, resource release and prepare clear level and activity.

Parameter identifiers, ranges and stored values are unchanged; sound changes.
Changing decay/attack repeatedly can delay completion, by explicit policy.
No claims are made about physical switching voltages or measured timing.

Tests retain sample-accurate duration checks at five rates and block-partition
checks, and use the new analytical release midpoint and slope bounds. The graph
peak detector now uses exponential boundary slopes (attack end approximately
0.314/T; decay start at sustain 0.5 approximately 1.157/T). Its five-observation
timing budget covers the inferred peak window and stage-boundary uncertainty;
the independent single-sample duration test retains its original tolerance.
An additional test checks sustain-edit and release-to-attack continuity.
After rebuilding tests and the standalone app, all 185 tests from 38 suites
passed with `--all`, including observations (exit status 0).

Earlier documents describing linear ADSR or k=0.5 shaping describe superseded
implementation stages. This document describes the current production policy.