> Updated: the upper Modern resonance range now includes bounded self-oscillation.
> The +11.93 dB curve below is retained as the stable raw-model reference;
> it is no longer the maximum playable resonance setting. See the current behavior below.

# Lightweight musical VCF approximation

The selected direction is a musical behavioral model, not a full circuit
simulation. Preserve the shared integrator core and the IC/circuit ownership
boundary. Use the drawings to inform differences without claiming that every
terminal, resistor or capacitor has been reconstructed.

## Current tuning targets

Original retains its two-pole response and Low/High resonance settings. Both
VCFs omit the previously removed empirical input/output high-pass stages.

Modern retains four poles and continuous resonance, with these product choices:

- Unity small-signal DC gain at every resonance setting, matching Original's
  bass-gain policy. This does not imply a constant perceived loudness for all
  waveforms, cutoff positions or driven levels.
- About +12 dB maximum small-signal resonance peak, rather than the former
  near-oscillatory peak of about +26.77 dB relative to its attenuated passband.
- Zero-resonance rolloff close to Original Low at the same panel setting, while
  retaining the different high-frequency slopes and resonance ranges.

These are explicit musical approximations, not measured Yamaha responses.
They do not solve the unknown LP-to-Vin transfer, output impedances, trim settings
or external capacitor network. Those remain documented in the
[drawing review](VCF-external-network-review.md).

## Implementation

Only the Modern circuit settings and cutoff mapping implement the new tuning.
The IC models, nonlinear parameters and shared numerical kernel are unchanged.
Starting from `a=82/22`, `b=82/15`, use `t=(1+2.25*r)^(1/4)` and pass the weights
`{a/t, b/t^2, a/t^3, 1}` to the four-stage model. The last coefficient fixes DC
gain at unity. The resonance/frequency normalization gives a maximum analytic
peak of approximately +11.93 dB and remains below the assumed linear model's
instability boundary.

Multiply the requested Modern cutoff by 1.3091 before the existing frequency
limits. This aligns the low-resonance rolloff with Original Low in the low-frequency
bilinear limit. It is intentionally approximate near upper clamps/Nyquist.
Higher resonance can sound louder because the spectrum changes; there is no
automatic waveform-dependent loudness normalization or change in input drive.

## Review and validation

Independent small-signal response equations, DC-gain tests, cutoff alignment and
resonance-band measurements check these numerical targets. Existing modulation,
state reset, block-partition and extreme-input tests remain applicable.

The resonant Modern raw-spectrum baselines were intentionally revised. At
48 kHz, driven 500 Hz, input peak 0.6 and resonance 0.8, the fundamental changes
from 0.119433 to 0.425545 and DC from 0.00477391 to 0.01699514. Zero-resonance
raw-model baselines and all Original baselines remain unchanged. Circuit cutoff
alignment is outside those raw-model baselines and is tested separately.

A comparison phrase uses a 24-harmonic saw-like source, four bass notes, the
same cutoff envelope and 4x FIR downsampling. Each file contains, in order,
Original Low, Original High, Modern resonance 0.5, Modern resonance 1.0; each
phrase lasts two seconds followed by 0.25 seconds of silence. Before and after
use the same output gain, not independent peak normalization. The Original
phrases are identical and serve as level/timbre references. This is a filter
comparison, not a complete preset/DAW rendering or a listening-test result.

The measurements establish bounded software behavior and the intended response
change. Musical preference still needs listening; they do not prove hardware
accuracy or replace native plugin validation.

## Modern self-oscillation extension

The actual circuit wrapper preserves the preceding curve through resonance 0.8.
Above that it raises the unnormalized loop parameter from 1.8 to 3.8. The assumed
quartic model's linear boundary is `b-2 = 3.46667`; thus onset is near resonance
0.967. This is a chosen musical range, not a calibrated Yamaha slider position.

IC-local saturation replaces part of the last-state feedback by
`L*tanh(output/L)`, with provisional normalized `L=0.25`. Its strength fades
in smoothly between resonance 0.8 and 0.95. The shared integrator solver solves
this scalar nonlinear feedback equation in the same sample using bounded
Newton/bisection iterations, without a feedback delay. At small amplitude the
linearized feedback is preserved; at larger amplitude its effective gain falls,
allowing a stable limit cycle. The final output guard is not the amplitude-control
mechanism. Both IC classes retain identical code and API shape; Original supplies
zero nonlinear-feedback strength and retains its existing sound.

A deterministic 1e-9 state excitation when the nonlinear loop is enabled/reset
represents startup circuit noise. A deterministic pseudo-noise floor of amplitude 1e-10 is also present only while
the nonlinear feedback is active, so a slow sweep from silence can start
oscillation. Neither is a separate audible oscillator. Exact zero states would otherwise remain exactly zero
in a noiseless digital system. Reducing resonance below onset lets oscillation
decay. The filter can oscillate internally with no VCO input; normal VCA EG/gate
still controls whether that oscillation reaches the engine output.

Tests cover zero-input startup, bounded sustained amplitude, decay after lowering
resonance, repeatable reset and sample-rate/cutoff variation. An offline probe at
44.1/48/192 kHz and 100/1000/10000 Hz settles near 0.111 RMS / 0.158 peak in
normalized units. These characterize the implementation, not measured hardware.
The zero-input demo raises resonance into oscillation, lowers cutoff, and then
reduces resonance to stop it. The original +12 dB comparison audio predates this
extension.
