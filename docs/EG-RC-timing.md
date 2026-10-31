# EG RC timing in iPlug2

Attack, Decay and Release store an RC time constant
in seconds. DSP, native host automation, editor controls and MIDI share this mapping.

Assume a generic A taper with 10% of the 2 MΩ resistance at half travel:

```
Rpot(p) = 2e6 * (81^p - 1) / 80
tau(p) = minTau + (maxTau - minTau) * (81^p - 1) / 80
```

For Attack and Release, `minTau = 1800 * 2.2e-6 = 3.96 ms`, `maxTau = 4.40396 s`, and the midpoint is `443.96 ms`.
For Decay, `minTau = 3900 * 2.2e-6 = 8.58 ms` (~79 ms settling to -80 dB) to account for additional effective series/feedback impedance in the Tr11/Tr12 sustain-reference discharge branch and prevent zero-sustain patches from choking at the bottom of the slider.
This is an assumption requested for this implementation, not a measurement of
the CS01 potentiometers or the EG IC. It uses the series resistance and capacitor
from the circuit audit; IC thresholds and internal current paths remain uncalibrated.

Each branch uses `1-exp(-dt/tau)`. Attack retains the provisional target
`1/(1-exp(-2))` and threshold 1, reaching the peak from zero in `2*tau`.
Decay approaches Sustain and Release approaches zero. They finish when the
remaining distance is 0.0001 of the branch's initial distance (-80 dB), after
`ln(10000)*tau`. These thresholds are software choices, not verified IC thresholds.
Time edits preserve level, target and threshold; retriggers retain residual charge.
A zero-level release still clocks the independent VCA note gate. The host tail
covers the maximum release, approximately 40.56 s, plus output latency.

The six manual factory presets use their documented panel positions. Clavinet's Decay
position 0.2 gives tau ≈81.4 ms (settling ≈750 ms); Synth Bass's position 0.3
gives tau ≈154.5 ms (settling ≈1.42 s). Default uses 100 ms tau values.
XML envelope values represent tau in seconds. Schema-less legacy XML accepts
nonnegative envelope values below today's floor and clamps them to the floor.
Schema-1 Decay values from the old 3.96 ms floor up to 8.58 ms are also clamped;
other out-of-range values are rejected without changing the patch. Re-saving
records the migrated physical value, and subsequent restores retain it.
Normalized automation uses the same A-taper mapping.

The Decay floor change intentionally changes the time at a saved normalized
position; XML migration cannot preserve a DAW's separate automation lane.
Relative to the old mapping, the increase is
`4.62 ms * (1 - (81^p - 1) / 80)`: 4.62 ms at zero, 4.158 ms at the midpoint
(443.96 ms becomes 448.118 ms), and zero at full travel. Physical XML values at
or above 8.58 ms remain unchanged. There is no versioned legacy automation mode.
Core and actual IParam conversion tests guard these differences; opening a saved
project and exercising automation in a real DAW remains a native validation task.

Verification covers analytical branch durations at five internal sample rates,
midpoint/endpoints and inverse mapping, live edits/retriggers, independent note
gate clocks, factory panel positions and audible tails, and the actual iPlug2
IParam conversion. Native Windows/macOS plugin execution requires those platforms.
