# EG RC timing in iPlug2

The former arbitrary 1 ms–2 s mapping cut the zero-sustain Clavinet and Synth Bass
sounds off too quickly. Attack, Decay and Release now store an RC time constant
in seconds. DSP, native host automation, editor controls and MIDI share this mapping.

Assume a generic A taper with 10% of the 2 MΩ resistance at half travel:

```
Rpot(p) = 2e6 * (81^p - 1) / 80
tau(p) = (1800 + Rpot(p)) * 2.2e-6
```

The endpoints are 3.96 ms and 4.40396 s, and the midpoint is 443.96 ms.
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

The six manual factory presets retain their panel positions. Clavinet's Decay
position 0.2 gives tau ≈81.4 ms (settling ≈750 ms); Synth Bass's position 0.3
gives tau ≈154.5 ms (settling ≈1.42 s). Default retains its 100 ms tau values.
Parameter IDs and ordering are unchanged. Existing XML values retain seconds,
but their new interpretation as tau changes envelope shape and duration. Values
outside the new range are clamped. Existing normalized automation changes with
the new taper; no automatic legacy conversion is provided.

Verification covers analytical branch durations at five internal sample rates,
midpoint/endpoints and inverse mapping, live edits/retriggers, independent note
gate clocks, factory panel positions and audible tails, and the actual iPlug2
IParam conversion. Native Windows/macOS plugin execution requires those platforms.
