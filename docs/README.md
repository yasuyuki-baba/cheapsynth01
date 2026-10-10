# Documentation guide

Documents describe two subjects: current software behavior and recorded
hardware analysis. Usage, build and test instructions are available in the
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
| [Factory presets](Factory-presets.md) | Owner's-manual panel readings, conversion and program usage. |
| [MIDI implementation](MIDI-Implementation.md) | Supported messages, controls and panic behavior. |
| [Distribution](Distribution.md) | ZIP installation, distribution contract and licensing decisions. |

### Implementation and design

| Document | Purpose |
| --- | --- |
| [DSP responsibility boundaries](DSP-responsibility-boundaries.md) | Production signal paths, empirical stages and safety ownership. |
| [Original VCF model](Original-VCF-behavioral-design.md) | Current Original filter architecture, evidence and tradeoffs. |
| [CS-01II VCF model](tech/CS01II-VCF-model.md) | Current Modern filter's IG05630-inspired behavioral model. |
| [Stateful EG model](EG-stateful-model.md) | Production envelope stages, residual-state retriggers, time-edit policy and provisional curvature. |
| [Note onset continuity](Note-onset-continuity.md) | Free-running sources, independent VCA note gate and transition validation. |
| [MIDI realtime control](MIDI-realtime-control.md) | Event handling, realtime flow and subsequent stability changes. |
| [Modulation speed ranges](Modulation-speed-ranges.md) | LFO/PWM ranges, defaults and saved-state/automation implications. |

### Validation evidence

Test counts and measurements below belong to the documented run. For current
test commands and coverage, use the [test guide](../Tests/README.md); for later
audit results, use the [stability audit](Audit-stability.md).

| Document | Scope and reading guidance |
| --- | --- |
| [Stability audit](Audit-stability.md) | Dated fixes, reproduction, test results and realtime measurements; additions appear later in the file. |
| [Whole-graph oversampling](Oversampling-validation.md) | Production oversampling policy and validation limits; includes historical VCO-only observations. |
| [VCF internal-rate experiment](VCF-oversampling-decision.md) | Historical filter-only decision, superseded by whole-graph integration. |
| [VCF panel validation](VCF-panel-validation.md) | Earlier biquad panel/precision measurements; use the Original VCF design for the current model. |
| [VCF musical-model first pass](VCF-musical-model-validation.md) | Earlier biquad/coloration changes and their checks, preceding the current Original VCF model. |
| [EG time-range validation](EG-time-range-validation.md) | Provisional parameter mapping and production duration checks. |
| [EG control-link first pass](EG-control-link-validation.md) | EG-depth smoothing implementation and checks at that stage. |
| [DSP baselines](dsp-baselines.md) | Characterization methods, ideal reference probes and archived comparisons; explains what can be regenerated. |

## Hardware analysis

### Circuit evidence

These documents separate schematic facts, conditional equations and software
approximations. Their investigation outcomes are not hardware calibration.

| Document | Purpose |
| --- | --- |
| [Unresolved circuit details](Circuit-model-unknowns.md) | Hardware unknowns and the limits of software approximations. |
| [Circuit evidence status](Circuit-evidence-status.md) | Recorded evidence, source availability and limits of circuit conclusions. |
| [EG circuit audit](EG-model-audit.md) | Circuit transcription, conditional equations and reasons calibration is deferred; production policy is in the stateful EG model. |
| [External-circuit closeout](External-circuit-closeout.md) | Historical audit outcome, subsequent EG implementation and analysis limits. |
| [TC7476BP investigation](TC7476BP-online-investigation.md) | Recorded datasheet inspection, retrieval history and analysis limits. |
| [Glissando circuit audit](Glissando-circuit-audit.md) | Schematic evidence, unknowns and interim live-speed implementation. |
| [Low-frequency circuit audit](Low-frequency-circuit-audit.md) | Coupling-network evidence and limitations of empirical low-frequency stages. |
| [External source catalog](Source-catalog.md) | Source locations, cited pages and availability of original material in this checkout. |

### Technical background

These summaries provide device or related-product context. Source attribution
and confidence are recorded separately from production implementation choices.

| Document | Scope |
| --- | --- |
| [YM10150 summary](tech/ymf10150.md) | Tone-generator technical background and confidence notes. |
| [LFO summary](tech/lfo.md) | LFO technical background; public software ranges are documented separately above. |
| [IG00156 filter summary](tech/filter.md) | Related CS-series filter background; IG00156 is not the CS-01's IG02610 or CS-01II's IG05630. |
