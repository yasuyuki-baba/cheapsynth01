# CS-01 modulation speed ranges

Recorded source: the Japanese owner's manual previously stored as
`docs/tech/CS01J.pdf`, PDF page 13 / printed page 24. The original is absent
from this checkout; see [source availability](../hardware/Source-catalog.md).

- LFO: 0.8–21 Hz.
- PWM: 0.6–12 Hz.

The public parameters now use these ranges. Defaults remain 5 Hz and 2 Hz.
Existing skew factors are retained as approximations, not measured potentiometer
tapers. All seven bundled presets use values within the new ranges.

Legacy saved physical values outside these ranges are clamped on restoration.
Existing DAW automation using normalized positions may change frequency because
the ranges changed; this is not a transparent automation-compatible update.
DSP unit tests may retain broader test-only ranges to isolate stopped modulation.

Endpoint/state-restoration tests alone do not establish modulation-period accuracy.
The separate 2 Hz PWM period check is described in
[oversampling validation](Oversampling-validation.md); it does not cover the full range.
