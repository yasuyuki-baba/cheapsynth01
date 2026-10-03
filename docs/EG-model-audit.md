# CS-01 EG model: implementation decision

The production EG remains JUCE's linear ADSR. Its lifecycle and parameter-update
regressions are covered separately. It is not claimed to reproduce the analog
charging/discharging curve.

## Verified candidate

`Tests/unit/RCEnvelopeModelTest.cpp` evaluates an ideal isolated RC section:

    V(t) = target + (V(0) - target) * exp(-t / tau)
    tau = R * C

The discrete update uses expm1 for small-step accuracy. Tests compare every
sample with the independent continuous-time expression at 44.1, 48 and 96 kHz.
Changing resistance or switching the target retains the stored capacitor voltage.
These tests validate the numerical primitive, not the CS-01 circuit topology.

## Why production replacement is deferred

An isolated RC formula alone does not establish the actual attack target,
switching threshold, loading, effective resistance or sustain buffer behavior.
The current UI parameters are seconds, not identified physical resistor values.
A time constant also differs from a finite stage duration: an ideal exponential
never reaches its target exactly. A termination threshold and conversion from
the existing time parameters would therefore be additional assumptions.

Do not silently introduce an arbitrary overshoot target, threshold, or taper and
call it circuit-derived. Before integration, document each charging/discharging
path, its component values, loading assumptions and stage transitions. Then test
the complete state machine, automation, retriggering and compatibility with the
existing presets. No production sound change is made by this experiment.