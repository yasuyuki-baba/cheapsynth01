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

## Current software

### Usage and distribution

| Document | Purpose |
| --- | --- |
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

### Validation evidence

Test counts and measurements below belong to the documented run. For current
test commands and coverage, use the [test guide](../Tests/README.md); for later
audit results, use the [stability audit](software/Audit-stability.md).

| Document | Scope and reading guidance |
| --- | --- |
| [Stability audit](software/Audit-stability.md) | Dated fixes, reproduction, test results and realtime measurements; additions appear later in the file. |
| [Whole-graph oversampling](software/Oversampling-validation.md) | Production oversampling policy and validation limits; includes historical VCO-only observations. |
| [VCF internal-rate experiment](software/VCF-oversampling-decision.md) | Historical filter-only decision, superseded by whole-graph integration. |
| [VCF panel validation](software/VCF-panel-validation.md) | Earlier biquad panel/precision measurements; use the Original VCF design for the current model. |
| [VCF musical-model first pass](software/VCF-musical-model-validation.md) | Earlier biquad/coloration changes and their checks, preceding the current Original VCF model. |
| [EG time-range validation](software/EG-time-range-validation.md) | Provisional parameter mapping and production duration checks. |
| [EG control-link first pass](software/EG-control-link-validation.md) | EG-depth smoothing implementation and checks at that stage. |
| [DSP baselines](software/dsp-baselines.md) | Characterization methods, ideal reference probes and archived comparisons; explains what can be regenerated. |

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
| [TC7476BP investigation](hardware/TC7476BP-online-investigation.md) | Recorded datasheet inspection, retrieval history and analysis limits. |
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
