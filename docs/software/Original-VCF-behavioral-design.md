# Original VCF behavioral model

## Evidence categories

The external CS-01 wiring and the existence of the IG02610 filter IC are
circuit facts. Existing Phase 2 response observations are software
characterizations, not measurements of a physical unit. No Phase 2 benchmark
or machine-readable measurement artifact is present in this repository
checkout, and no hardware capture was supplied for this change. The filter
internal topology, cutoff law, resonance curve, and distortion remain
unvalidated against an IG02610.

## Related Yamaha model evidence and structural hypothesis

The PS-1/PS-2/PS-3 service manual groups the three models but does not imply
that they share one filter design. Its PS-2 block diagram emphasizes the GE2
generator and orchestra path; the PS-3 diagram separately labels a VCF for the
Brass/Guitar voice path. This is useful evidence that Yamaha used a dedicated,
voice-specific filter path in a nearby product family, but it does not show
that PS-2 has a CS-01-like VCF or that PS-3's circuit is the IG02610.

The PS-30 service manual identifies an IG02612 as a VCF and documents a solo
tone-generator section. That makes PS-30 the strongest of these references for
Yamaha's contemporary filter implementation and how a filtered solo voice
could sit beside generated orchestral voices. IG02612 is a different part
number from CS-01's IG02610. Without evidence that their internal structures
and external circuits match, its component values and control law must not be
copied as CS-01 facts.

The CS-01 service manual independently establishes the IG02610, cutoff and
resonance controls, and a VCF adjustment procedure that seeks a peak point at
a specified cutoff-control voltage. This supports modeling an adjustable
resonant low-pass response. Available secondary reverse-engineering notes
interpret the CS-01 use as a two-pole (12 dB/octave) low-pass and describe
resonance as a signal-feedback interaction. Those are useful, plausible
structural clues, but the IC has no located Yamaha datasheet or internal block
diagram, so they do not identify its actual internal circuit.

### Review of the supplied IG02610 report

The report's OTA-based, two-integrator SVF is a reasonable structural
hypothesis for the behavioral model. It is more specific than the available
primary evidence supports, however. The reviewed CS-01 service material
identifies the IC and its external use/adjustment, but does not document an
internal OTA, exponential converter, integrator count, or the pin functions
listed in the report. Treat those details as hypotheses until a legible
primary schematic that traces the pins or a hardware investigation confirms
them. In particular, the current TPT SVF is an implementation choice inspired
by that hypothesis, not a reverse-engineered schematic transcription.

The PS-30 service manual's parts list calls out **IG02612** as its VCF. That is
useful evidence for a related Yamaha filter application, but it does not
confirm the report's IG02611 attribution for PS-30 or establish compatibility
with IG02610. The external reverse-engineering article discusses IG02610/11
and is useful as a secondary interpretation; it is not a Yamaha datasheet or
hardware measurement. The report's low-voltage design rationale, detailed
pinout, and proposed soft-clipping behavior likewise remain unverified.

| Claim | Evidence status | Model consequence |
| --- | --- | --- |
| CS-01 uses an IG02610 VCF and exposes cutoff/resonance behavior | Service-manual-supported external facts | Preserve wrapper behavior and exercise cutoff and resonance |
| The filter response is a resonant two-pole low-pass | Consistent with the service adjustment and secondary interpretation | A two-state-variable low-pass is a useful behavioral candidate |
| Internal OTA pair, exponential bias converter, pin-by-pin roles | Provisional circuit hypothesis; not confirmed by the reviewed Yamaha material | Do not encode these as verified circuit facts |
| Feedback saturation reproduces the IC's distortion | Behavioral hypothesis; no hardware THD data | Keep parameters explicit and keep provisional behavior explicitly identified |
| PS-30 is fitted with IG02611 | Contradicted by the reviewed PS-30 manual parts list, which identifies IG02612 | Do not use this attribution as support for IG02610 equivalence |

### Provisional structure

Given those constraints, the behavioral model uses two state variables as a
stable digital stand-in for a two-pole resonant low-pass, with resonance
entering the recursive feedback relation. Following the supplied report's
behavioral proposal, smooth bounded nonlinearities now shape both the
resonant feedback and the signal entering the first integrator. This is a
testable hypothesis about where level-dependent harmonics may arise; it does
not mean the IC's internal circuit is known. The two saturation drives and
damping map are provisional calibration parameters. No asymmetry is included
because neither its direction nor amount is supported by the available
evidence.

The processor maps EG, LFO, and breath modulation to semitone offsets and
combines them with an `exp2` ratio before sending cutoff in hertz to the core.
This gives the behavioral model a smooth exponential frequency response to
those controls. It is not a calibrated volts-per-octave or CV-to-bias law for
IG02610; those mappings remain unknown.

These hypotheses were compared against the former linear biquad in the
archived Phase 2 artifacts. The hardware-specific cutoff law, Q mapping,
nonlinear transfer, and asymmetry remain unknown until a unit or further
primary circuit evidence becomes available.

### Reference documents reviewed

- [PS-1/PS-2/PS-3 service manual](https://www.manualslib.com/manual/4155002/Yamaha-Portasound-Ps-1.html): GE2 generator description, model-specific block diagrams, and overall circuit diagrams.
- [PS-30 service manual, solo generator page](https://www.manualslib.com/manual/4397023/Yamaha-Ps-30.html?page=14) and [electronic components page](https://www.manualslib.com/manual/4397023/Yamaha-Ps-30.html?page=24): solo tone-generator section and IG02612 identified as VCF.
- [CS-01 service manual](https://manuals.plus/m/7bf5a88f7fd7a35ab00af242825c27ab94ff0c3ffd909bbce88c3360fa997a4f): IG02610 identification, control/adjustment data, and overall circuit diagrams.
- [IG02610/11 schematic interpretation](https://ss30m.blogspot.com/2020/05/fun-with-filters-pt2.html): secondary reverse-engineering notes; treated as an interpretation, not Yamaha documentation or hardware measurement.

## Current architecture

`OriginalVCFProcessor` routes control-rate and audio-rate modulation through `CS01VCFCircuit`, which now uses the `IG02610BehavioralModel` TPT state-variable behavioral core. The separate Legacy biquad implementation, model-selection API, and temporary editor toggles have been removed. The existing filter type control continues to choose Original or Modern; both selections use their respective production behavioral models.

The wrapper retains empirical input/output coupling approximations. The core uses explicit provisional damping, feedback-drive, integrator-drive, and output-bound parameters. None is calibrated to a physical IG02610. The selected structure is motivated by the supplied report's two-integrator SVF hypothesis and the numerical need for continuous cutoff modulation; it is not evidence of the IC's actual internals.

## Chosen topology and trade-offs

The behavioral core is a topology-preserving-transform state-variable
low-pass. Its integrator state does not depend on a stored biquad coefficient
set, so cutoff can be changed every sample without coefficient history
interpolation. The mapping uses one tangent and two `tanh` evaluations per
sample. The state update is deterministic, double precision, and bounded by
the selected cutoff interval. Compared with a linear TPT SVF, the nonlinear
feedback and integrator input make the response level dependent and generate
additional harmonics. The extra nonlinear stage has a measurable CPU cost;
updated same-machine compiled benchmarks are recorded below.

The `tanh` feedback and integrator-input shaping, exponential modulation map,
and chosen damping map are behavioral hypotheses. They are not claims about
the IG02610's transistor, OTA, or integrator stages. The final output clamp
and finite-input handling are implementation safety measures, not hardware
behavior.

## Empirical parameters

The named values in `IG02610BehavioralModel::EmpiricalParameters` are
provisional: minimum/maximum damping, feedback drive, and integrator-input drive.
The unchanged maximum output is separately named in `SafetyParameters`; it is
an implementation bound, not a calibration target.
No repeatable hardware measurements calibrate these values. The processor's
cutoff limits and
two-position resonance control is inherited behavior, not claims about a control
voltage law.

## Validation coverage and limitations

The new unit checks cover finite and bounded output under fast cutoff sweeps
at 44.1, 48, and 96 kHz host rates (at the project's 4x internal rate), plus
deterministic output independent of how a sequence is partitioned into
blocks. Original VCF tests cover routing, finite output, modulation behavior, and
bus/control routing. These checks do not validate measured frequency
response, resonance peak, THD, cutoff modulation spectrum, or hardware tone.
They also do not establish block-size consistency for the whole graph.

The archived dual-model JSON and compiled comparisons in `artifacts/dsp/` were
recorded before the Legacy implementation was removed. Their Python harness
has also been removed, so these files are historical rather than regenerable
current-source measurements. They compare model equations and filter-path
timings, not hardware or whole-plugin performance.

The focused compiled checks cover finite/bounded output at 44.1, 48, and 96 kHz
host rates, deterministic block partitioning and reset, level-dependent
nonlinearity, and routing. They are implementation checks, not hardware
validation.

These probes do not validate the complete EG/LFO/breath routing in a running
graph or establish hardware behavior. No matched plugin/hardware capture with
identified units and measurement conditions is recorded here; damping, drive
and control parameters remain uncalibrated.

Compiled response, harmonic, and CPU probes are recorded in
`artifacts/dsp/original_vcf_cpp_response.csv`,
`artifacts/dsp/original_vcf_cpp_harmonics.csv`, and
`artifacts/dsp/original_vcf_cpp_benchmark.csv`. Response and harmonic
measurements render each C++ core with the shared coupling stages. The CPU
probe uses 65,536 samples per case, 4x internal rate, static and sinusoidally
modulated cutoff, and the same wrapper stages. After adding integrator-input
saturation, Debug behavioral processing measured a 137.1 ns/sample median
(133.4–142.3 range) versus 214.9 (209.5–218.2) for legacy. Release measured
59.9 ns/sample median (59.0–62.7) versus 60.5 (60.0–64.3) for legacy. This
meets the target of comparable or lower core cost on this machine, with the
Debug difference larger than the Release difference. These are observational
measurements, not cross-machine guarantees; CPU timing excludes processor-side
modulation generation and whole-graph scheduling. The response and harmonic
CSVs characterize the filter paths, not the whole plugin graph or hardware.
No hardware validation is available; the chosen model is a product sound decision, not evidence of circuit accuracy.

## Current implementation status

The behavioral model is now the sole Original VCF implementation following
the user's listening evaluation; the previous implementation and A/B toggle
have been removed. This is a product sound choice, not confirmation of hardware
accuracy; no physical IG02610 comparison is available. Earlier CSV comparison
artifacts remain archived and describe the code as it existed when measured.

Current stage boundaries and safety ownership are described in [DSP responsibility boundaries](DSP-responsibility-boundaries.md).
