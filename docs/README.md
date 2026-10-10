# Documentation guide

Documents describe two subjects: current software behavior and recorded
hardware analysis. `software/` contains implementation and validation documents;
`hardware/` contains circuit analysis and source information.
Usage, build and test instructions are available in the
[project overview](../README.md), [development guide](../CONTRIBUTING.md) and
[test guide](../Tests/README.md).
Design documents describe provisional software models; passing regression tests
does not establish agreement with Yamaha hardware. Validation reports record
the implementation and conditions of their own run, not necessarily the current
checkout. Earlier measurements are retained as dated supporting evidence, not
as current implementation claims or a plan for future work. Analysis distinguishes
recorded circuit facts, conditional equations and unverified interpretations.

## Reading without external originals

The explanations use values, connections, equations and assumptions recorded
in this repository. Reading a manual, drawing or datasheet is not a prerequisite.
External source locations and page numbers are optional provenance in
[the source catalog](hardware/Source-catalog.md).

A recorded finding preserves an earlier inspection; it is not a new inspection
or a complete reproduction of its source. Calculated results state their input
values and assumptions. Unverified interpretations and software policies are
identified separately. Missing device behavior remains an explicit limit of the
analysis rather than a step delegated to an unavailable original.

## Current software

### Usage and distribution

| Document | Purpose |
| --- | --- |
| [User guide](software/User-guide.md) | Playing, controls/ranges/defaults, user presets and session behavior. |
| [Factory presets](software/Factory-presets.md) | Owner's-manual panel readings, conversion and program usage. |
| [MIDI implementation](software/MIDI-Implementation.md) | Supported messages, controls and panic behavior. |
| [Distribution](software/Distribution.md) | ZIP installation, distribution contract and licensing decisions. |

### Implementation and design

| Document | Purpose |
| --- | --- |
| [DSP responsibility boundaries](software/DSP-responsibility-boundaries.md) | Production signal paths, empirical stages and safety ownership. |
| [Original VCF model](software/Original-VCF-behavioral-design.md) | Current Original filter architecture, evidence and tradeoffs. |
| [CS-01II VCF model](software/CS01II-VCF-model.md) | Current Modern filter's IG05630-inspired behavioral model. |
| [Stateful EG model](software/EG-stateful-model.md) | Production envelope stages, residual-state retriggers, time-edit policy and provisional curvature. |
| [Note onset continuity](software/Note-onset-continuity.md) | Free-running sources, independent VCA note gate and transition validation. |
| [MIDI realtime control](software/MIDI-realtime-control.md) | Event handling, realtime flow and subsequent stability changes. |
| [Modulation speed ranges](software/Modulation-speed-ranges.md) | LFO/PWM ranges, defaults and saved-state/automation implications. |

### Current validation coverage

These describe current validation methods and their limits. Use the
[test guide](../Tests/README.md) for commands and coverage. Any measured results
retain their own source revision and conditions; historical runs are indexed below.

| Document | Scope and reading guidance |
| --- | --- |
| [Whole-graph oversampling](software/Oversampling-validation.md) | Production oversampling policy and validation limits; includes historical VCO-only observations. |
| [EG time-range validation](software/EG-time-range-validation.md) | Provisional parameter mapping and production duration checks. |
| [DSP baselines](software/dsp-baselines.md) | Characterization methods, ideal reference probes and archived comparisons; explains what can be regenerated. |

### Historical validation records

These retain measurements and validation of earlier source revisions. Current
behavior is described in the implementation documents above; historical counts
are not a current checkout test result.

| Document | Recorded scope |
| --- | --- |
| [Stability audit](software/Audit-stability.md) | Dated fixes and measurements, grouped by tested source revision. |
| [VCF internal-rate experiment](software/VCF-oversampling-decision.md) | Filter-only experiment and early whole-graph observations. |
| [VCF panel validation](software/VCF-panel-validation.md) | Earlier biquad panel/precision measurements. |
| [VCF musical-model first pass](software/VCF-musical-model-validation.md) | Earlier biquad/coloration implementation and validation. |
| [EG control-link first pass](software/EG-control-link-validation.md) | Depth-ramp implementation and its original validation. |

## Hardware analysis

### Circuit evidence

These documents separate schematic facts, conditional equations and software
approximations. Their investigation outcomes are not hardware calibration.

| Document | Purpose |
| --- | --- |
| [Unresolved circuit details](hardware/Circuit-model-unknowns.md) | Hardware unknowns and the limits of software approximations. |
| [Circuit evidence status](hardware/Circuit-evidence-status.md) | Recorded evidence, source availability and limits of circuit conclusions. |
| [EG circuit audit](hardware/EG-model-audit.md) | Circuit transcription, conditional equations and reasons calibration is deferred; production policy is in the stateful EG model. |
| [External-circuit closeout](hardware/External-circuit-closeout.md) | Historical audit outcome, subsequent EG implementation and analysis limits. |
| [TC7476BP logic record](hardware/TC7476BP-online-investigation.md) | Pin assignment, truth table, supply limits and circuit-analysis boundary. |
| [Glissando circuit audit](hardware/Glissando-circuit-audit.md) | Schematic evidence, unknowns and interim live-speed implementation. |
| [Low-frequency circuit audit](hardware/Low-frequency-circuit-audit.md) | Coupling-network evidence and limitations of empirical low-frequency stages. |
| [External source catalog](hardware/Source-catalog.md) | Source locations, cited pages and availability of original material in this checkout. |

### Technical background

These summaries provide device or related-product context. Source attribution
and confidence are recorded separately from production implementation choices.

| Document | Scope |
| --- | --- |
| [YM10150 summary](hardware/ymf10150.md) | Tone-generator technical background and confidence notes. |
| [LFO summary](hardware/lfo.md) | LFO technical background; public software ranges are documented separately above. |
| [IG00156 filter summary](hardware/filter.md) | Related CS-series filter background; IG00156 is not the CS-01's IG02610 or CS-01II's IG05630. |
