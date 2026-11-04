# DSP design and modeling limits

This guide describes the current framework-independent C++ core. Hardware
source provenance and unresolved circuit details are recorded in
[the hardware index](hardware/README.md). The implementation uses behavioral
hypotheses and empirical stages; software tests do not establish agreement
with a physical Yamaha instrument.

## Signal path

[SynthEngine](../CheapSynth01/core/SynthEngine.cpp) owns LFO, EG, VCO, both VCF models, VCA
and the output converter. Each host frame advances the selected oversampling
factor's internal samples (four by default):
LFO/EG → VCO → selected VCF → VCA → FIR output converter. MIDI events are
applied before their host frame. The selected LFO route targets pitch or filter;
EG independently feeds both filter and amplifier modulation.

The core processes mono; the product duplicates the result to stereo. Stage
ownership and routing are explicit, with no framework audio graph. Tone/noise
sources run continuously upstream of the VCA gate. Panic resets note ownership,
MIDI/envelope state, filters, VCA and converter state.

## Sound generation

[VCOProcessor](../CheapSynth01/core/cs01synth/VCOProcessor.h) selects tone or white noise.
[ToneGenerator](../CheapSynth01/core/cs01synth/ToneGenerator.cpp) and
[waveform strategies](../CheapSynth01/core/cs01synth/WaveformStrategies.h) implement
Triangle, Sawtooth, Square, Pulse (12.5% narrow duty cycle) and PWM, with bandlimiting where implemented
by the strategy. The model includes an octave selector, pitch bend, fine pitch,
PWM and discrete glissando. Noise uses a deterministic xorshift generator.
Changing tone/noise source transfers note/playback state; EG retains ownership
of release timing. These are software models, not a reconstruction of YM10150
internal circuitry.

## Parameters and modulation

[Parameters.h](../CheapSynth01/core/Parameters.h) defines stable host IDs, ranges, defaults
and skew mappings. Current ranges include LFO 0.8–21 Hz, PWM 0.6–12 Hz,
glissando 0–0.208 seconds per semitone, attack/release RC time constants 3.96 ms–4.40396 s,
and decay RC time constants 8.58 ms–4.40396 s (including an empirical floor for Tr11/Tr12 sustain-branch loading).
Manual panel marks do not establish exact electrical taper or envelope seconds;
these mappings remain provisional. Factory panel readings and their conversion
are documented in [factory presets](Factory-presets.md).

[MIDI flow](MIDI-realtime-control.md) describes note ownership, highest-note
priority, controller publication and queue boundaries. Live parameter edits and
whole-patch application have separate publication paths. Parameter index order
must remain stable.

## Envelope and note gate

[EGProcessor](../CheapSynth01/core/cs01synth/EGProcessor.cpp) retains level,
branch targets and thresholds across retriggers and live timing edits. Attack,
decay and release store RC time constants using an assumed A-taper resistance
mapping. Each branch uses `level += (target - level) * (1 - exp(-dt/tau))`.
Attack retains the provisional target `1 / (1 - exp(-2))`, reaching its peak from
zero after `2*tau`. Decay and release finish at 0.0001 of their initial distance;
these are software thresholds, not calibrated IC voltages.

The non-EG VCA gain has a separate note gate with a 1 ms onset fade and an
EG-owned release clock. Upstream oscillation can remain active while final output
is silent. The complete timing mapping, thresholds, maximum tail and verification
scope are documented in [EG RC timing](EG-RC-timing.md). The
[circuit audit](hardware/EG-model-audit.md) records conditional equations and
unresolved transistor details separately.

## Filter models

The [current decision summary](VCF-model-summary.md) records evidence, musical
choices and the common IC implementation.

The code separates three responsibilities:

- `OriginalVCFProcessor` / `ModernVCFProcessor` apply panel controls and shared
  EG/LFO/breath cutoff modulation.
- `CS01VCFCircuit` / `CS01IIVCFCircuit` own each instrument's external-network
  assumptions. `VCFCircuitSettings.h` records Original's two resonance anchors,
  provisional Q values and control-to-damping mapping, plus Modern's conditional
  resistor-network coefficients and provisional control-to-feedback mapping.
- `IG02610BehavioralModel` / `IG05630BehavioralModel` solve the assumed
  internal stage dynamics and input/output nonlinearities. Both receive the complete
  external feedback coefficient array. Neither embeds panel assignments, board
  resistor ratios or rules for which integrator outputs are fed back.

Both models expose the same API shape: construction with a complete `Feedback`
array, `prepare`, `reset`, `setCutoffFrequency`, `setFeedback(const Feedback&)`
and single-input `processSample`. The array has two entries for Original and
four for Modern. There is no scalar with different model-specific meanings and
no implicit board network in either IC constructor.

`VCFCircuitSettings.h` assembles the complete networks, including the fixed
connections as well as the adjustable resonance branch:

| Circuit | Internal stages | External state-feedback weights |
| --- | --- | --- |
| Original | 2 | `{damping, 1}`; damping is derived from the resonance control |
| Modern | 4 | `{a/t, b/t^2, a/t^3, 1}`; `t=(1+2.25*r)^(1/4)`, with musical DC/frequency normalization |

The IC models pass these arrays unchanged to `VCFIntegratorCore<Stages>`.
The core handles sample rate, double-precision cutoff, state storage and
simultaneous trapezoidal integration. It has no board values or saturation
assumptions. Keeping the solve simultaneous avoids introducing an artificial
sample delay into feedback that crosses the IC/circuit boundary.

Both IC classes instantiate `VCFBehavioralCore<Stages, Parameters>`; `.cpp`
files explicitly instantiate their profiles. The shared processing order is:
input validation, IC-configured soft limiting, integration, recovery and output
limiting. `VCFNonlinearInput.h` shares the asymmetric soft-limiting arithmetic;
each IC model retains its own parameters and chooses where to apply it. Equal
parameter values describe current hypotheses, not proven identical ICs.

This is a software responsibility boundary, not an identified IC schematic.
The Modern feedback network is still solved simultaneously with its internal
integrators; separating its coefficients does not introduce a sample delay.
Existing coefficients and audio behavior are preserved.

Both VCF circuit wrappers now use unity external input/output coupling.
Original's former empirical second-order 20 Hz input high-pass and first-order
8 Hz output high-pass have been removed. Their values and orders were not
established by the recorded circuit components. This intentionally changes
Original's low-frequency level, phase and potentially its nonlinear input response;
there is no compatibility compensation. Modern's path is unchanged.
This omission is not a claim that the physical instruments have no coupling loss.
See the [low-frequency audit](hardware/Low-frequency-circuit-audit.md).

Original VCF wraps [IG02610BehavioralModel](../CheapSynth01/core/cs01synth/IG02610BehavioralModel.h),
a provisional two-integrator TPT lowpass with asymmetric nonlinear input limiting.
LOW/HIGH controls retain circuit-level anchors 0.2/0.7, selecting Q=0.5/1.6.
The small-signal denominator is `p^2 + p/Q + 1`, with
`p = j*tan(pi*f/fs)/tan(pi*cutoff/fs)`. Both settings have unit DC gain before
input limiting; the extra nonlinear lowpass-state subtraction is removed.
The circuit-level resonance control interpolates damping between the anchors,
clamped outside them; the panel still selects exactly two states at 0.5.
These Q values are provisional sound choices, not derived component values.
The external
service material identifies IG02610 and its cutoff/resonance controls; it does
not establish the modeled internal topology, nonlinear placement or calibration.
The adopted external capacitor markings are 22 nF / 2 nF / 22 nF; they do not
alone determine the internal poles or Q.

This deliberate Original replacement changes existing presets and saved
sessions. There is no legacy mode, automatic gain compensation or cutoff
multiplier. Parameter IDs, values, factory resources, coupling stages, VCA,
oversampling and modulation routing are unchanged. Cutoff is the natural-frequency
scale, not a resonance-peak frequency or universal -3 dB point: Low's -3 dB
point is approximately 0.6436 times it and High's peak approximately 0.8970
times it at low cutoff/sample-rate ratios. Low is monotonic; High's small-signal
peak is about +4.53 dB above DC. Removing the former feedback changes absolute
gain, distortion and frequency alignment; it is not a sound-preserving refactor.

The current Modern playable range extends the stable reference curve into
self-oscillation above approximately 0.967 resonance. Its last-state feedback
is smoothly saturated inside the loop and solved implicitly by the shared core.
Original keeps zero nonlinear feedback. See the
[self-oscillation extension](hardware/VCF-musical-model.md#modern-self-oscillation-extension)
for the actual control curve, startup excitation and test conditions. The
+11.93 dB formula below describes the retained stable raw-model reference,
not the maximum playable resonance.

Modern VCF wraps [IG05630BehavioralModel](../CheapSynth01/core/cs01synth/IG05630BehavioralModel.cpp),
a provisional four-integrator, multi-feedback lowpass. The current musical
approximation retains `a=82/22` and `b=82/15` as starting response-shape weights,
not a complete resistor-network reconstruction. Its denominator is:

```text
t = (1 + 2.25 * resonance)^(1/4)
H(p) = 1 / (p^4 + (a/t)*p^3 + (b/t^2)*p^2 + (a/t^3)*p + 1)
```

Normalizing the constant term to one keeps DC gain at unity. Relative to the
previous `1+k` polynomial, the fourth-root frequency normalization also keeps
resonance from substantially shifting its band upward. This is a musical
control law, not a derivation of Vin/VQ. At maximum resonance the small-signal
peak is approximately +11.93 dB; the former model had a -12.46 dB passband and
approximately +26.77 dB peak relative to that passband. Driven signals still
undergo IC-local input shaping and output limiting.

The circuit wrapper multiplies the requested cutoff by 1.3091, subject to its
existing upper limits. This aligns zero-resonance rolloff with Original Low
(0.6436 / 0.4916). At low cutoff/sample-rate ratios both zero-resonance -3 dB
points are approximately 0.644 times the panel value. This alignment is a
product choice, not equal calibrated control currents in the two instruments.
Near the sample-rate/20 kHz clamps, exact alignment is not promised.

All integrator states and feedback are solved simultaneously; no additional
state, feedback delay, matrix solver or empirical coupling filter is added.
The [musical-model note](hardware/VCF-musical-model.md) records targets and
comparison conditions. The earlier [circuit investigation](hardware/Circuit-evidence-status.md#ig05630-external-network-investigation-2026-11-02)
remains useful for wiring evidence and limits, not current tuning coefficients.
Existing Modern presets and sessions change sound; parameter IDs and values
are unchanged and there is no legacy mode.

Both wrappers smooth VCF EG depth with a linear ramp over
`floor(internalSampleRate * 0.005)` samples, not a one-pole exponential smoother.
Retargeting starts a new ramp from the current value. Filter `reset()` preserves
the modulation ramp; `prepare()` initializes it to the current parameter value.
A preset's new depth becomes a ramp target on the next processed sample unless
the processor is re-prepared. The filter models are product sound choices.
Related PS-series evidence
and secondary reverse-engineering interpretations are catalogued in
[the source catalog](hardware/Source-catalog.md); related parts do not establish
CS-01 component values or internal topology.

## VCA and output conversion

[VCAProcessor](../CheapSynth01/core/cs01synth/VCAProcessor.cpp) owns the instrument
control policy and external signal path. `VCAControlCircuit::controlLevel` combines
EG depth, the independent note gate, breath sensitivity and the panel-volume
mapping into one normalized control quantity. EG depth retains its 5 ms smoothing.
`IG02600BehavioralModel` now receives only audio and this control quantity.

The user-supplied Yamaha IC Guide book (PDF p137 / printed 8-3) documents IG02600
control/reference, Bias and output blocks, including an emitter-follower input.
The IC model exposes a provisional `OperatingPoint`: reference/bias gain scale
and internal output-stage gain both default to one. Its control-to-gain law is
provisionally linear, and the existing empirical saturation curve is retained.
These normalized scales are not physical Bias/Ein voltages; the guide does not
specify transfer curves, headroom or follower voltage drops. Changing them is
an explicit model hypothesis, not a calibrated hardware adjustment. See the
[guide review](hardware/Yamaha-IC-guide-review.md).

Default operating-point values preserve the previous output, including the order
of control multiplication before saturation. The internal output stage is distinct
from the existing external Tr7 approximation. Coupling filters, Tr7 asymmetry/
treble emphasis and 15 kHz rolloff remain empirical; the
[low-frequency audit](hardware/Low-frequency-circuit-audit.md) records their limits.

The related IG00156 diagram documents an internal Q-control block, but is not a
block diagram of the target VCF ICs. The VCF's current feedback arrays remain an
effective software representation of the combined response, not evidence that
all physical resonance circuitry resides outside those ICs. No new VCF transfer
or electrical range is inferred from the related part.

The output converter uses a 129-tap Blackman-windowed linear-phase FIR at 4x,
with cutoff at 90% of host Nyquist. Its delay is 16 host samples, reported to
the host. No fixed envelope offset is added.

## Verification

[Active tests](../CheapSynth01/tests/README.md) cover numerical model references, independent
signal-path equations, MIDI boundaries, source/release timelines, live edits,
finite output, allocation-free portable paths, state and presets. The small
`CheapSynth01/tests/reference/` fixtures support current regression assertions.
Observational measurements describe their build and conditions.

`SignalPathTest.FilterSpectrumMatchesFixedProductSoundBaseline` stores independent
numeric baselines for both raw IC models: DC, RMS, fundamental,
second/third projections and selected folded bins. A 12 kHz cutoff, one-second
settling period and one-second integer-Hz measurement window define the stimulus.
The baseline covers driven 500 Hz at 44.1/48/96 kHz and 100 Hz small-signal plus
5/10 kHz driven inputs at 48 kHz, with 0.05% relative / 1e-8 absolute tolerances.
Both sets of values were intentionally revised on 2026-11-02 after independent
transfer/DC tests passed. Original's unit-DC two-pole replacement changes the
48 kHz small-signal 100 Hz fundamental from 0.00444450 to 0.00999857, and the
driven 500 Hz, resonance-0.8 fundamental from 0.368916 to 0.425098.
The earlier Modern replacement's driven 500 Hz, resonance-0.8 fundamental at
48 kHz was 0.119433. The 2026-11-04 musical normalization raises it to 0.425545;
DC changes from 0.00477391 to 0.01699514. Only the three resonant Modern baseline
rows were revised for this tuning, after independent response/DC checks; raw
zero-resonance rows and Original rows are unchanged. The 1.3091 cutoff mapping
is circuit-level and is checked separately. Both frozen historical arithmetic
fixtures remain unchanged.
The absolute floor (-160 dBFS amplitude) accommodates platform math differences
in the raw models' float samples. A historical Windows/macOS folded-bin
difference of about 1.74e-9 remains a tolerance-policy test: it still rejects
loss or a 10% change of that historical 1.58e-7 component and a 0.1% change of
a normal-level component. The 2026-11-04 resonant baseline revision was measured in this Linux workspace;
native Windows/macOS checks have not been rerun for this revision.
Do not regenerate these constants merely to make a DSP edit pass: intentional
sound changes need an explained baseline revision and before/after measurements.

`Observation_FilterSpectrumAndFoldedBins` expands the same measurement to all
three rates and two resonance settings. A projection above Nyquist is a folded
component, not an independent physical harmonic. Selected bins are alias probes,
not total alias power or THD. These tests isolate the raw nonlinear models without
the engine's oversampling or VCA high-pass/DC removal; raw DC is not final-output
DC, and raw folded-bin levels do not establish plugin-output alias audibility.
Neither these baselines nor the arithmetic references prove hardware fidelity.

Native host/editor/device validation remains listed in
[the validation status](iPlug2-migration-plan.md). Passing core tests does not
establish SDK callback behavior, DAW compatibility or hardware calibration.
