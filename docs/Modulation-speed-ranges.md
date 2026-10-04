# CS-01 modulation speed ranges

Source: local `docs/tech/CS01J.pdf`, PDF page 13, printed page 24.

- LFO: 0.8–21 Hz.
- PWM: 0.6–12 Hz.

The public parameters now use these ranges. Defaults remain 5 Hz and 2 Hz.
Existing skew factors are retained as approximations, not measured potentiometer
tapers. All seven bundled presets use values within the new ranges.

Legacy saved physical values outside these ranges are clamped on restoration.
Existing DAW automation using normalized positions may change frequency because
the ranges changed; this is not a transparent automation-compatible update.
DSP unit tests may retain broader test-only ranges to isolate stopped modulation.

Endpoint/state-restoration tests do not establish modulation-period accuracy.