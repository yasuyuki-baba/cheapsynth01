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

Original VCF wraps [IG02610BehavioralModel](../CheapSynth01/core/cs01synth/IG02610BehavioralModel.h),
a two-integrator state-variable lowpass with asymmetric nonlinear limiting at the input
and symmetric limiting in the resonant feedback. LOW/HIGH controls map to modeled resonance values 0.2/0.7. The external
service material identifies IG02610 and its cutoff/resonance controls; it does
not establish the modeled internal topology, nonlinear placement or calibration.

Modern VCF wraps [IG05630BehavioralModel](../CheapSynth01/core/cs01synth/IG05630BehavioralModel.cpp),
two cascaded TPT second-order lowpass sections with asymmetric input limiting and bounded nonlinear feedback.
The resonance feedback uses a fourth-power mapping with a conservative maximum
to avoid modeled self-oscillation. The CS-01II drawing identifies IG05630, but
its internal topology and physical resonance behavior are unresolved.

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

[VCAProcessor](../CheapSynth01/core/cs01synth/VCAProcessor.cpp) combines envelope depth,
breath control, volume and the independent note gate. Its coupling filters,
Tr7 asymmetry/treble emphasis and 15 kHz rolloff include empirical behavior;
[the low-frequency audit](hardware/Low-frequency-circuit-audit.md) distinguishes
recorded components from those approximations. EG depth is smoothed over 5 ms.

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
numeric baselines captured at ff9245a for both raw IC models: DC, RMS, fundamental,
second/third projections and selected folded bins. A 12 kHz cutoff, one-second
settling period and one-second integer-Hz measurement window define the stimulus.
The baseline covers driven 500 Hz at 44.1/48/96 kHz and 100 Hz small-signal plus
5/10 kHz driven inputs at 48 kHz, with 0.05% relative / 1e-8 absolute tolerances.
The absolute floor (-160 dBFS amplitude) accommodates platform math differences
in the raw models' float samples; the known Windows/macOS folded-bin difference
is about 1.74e-9. The tolerance still rejects loss or a 10% change of that
1.58e-7 component and a 0.1% change of a normal-level component.
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
