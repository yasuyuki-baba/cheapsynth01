# Circuit evidence status

This document summarizes recorded hardware evidence and its limits. Detailed
connections and conditional equations are in [the EG audit](EG-model-audit.md),
and optional provenance is recorded in [the source catalog](Source-catalog.md).
External originals and
private inventories are local-only under `references/` and are excluded from Git.

## How to read evidence limits

Recorded connections preserve earlier inspection findings. Conditional equations
apply only under their stated loading, conduction and device assumptions.
Software regression checks establish implementation behavior; they do not
calibrate a Yamaha device. Missing device transfers and measured responses stay
unknown. The source catalog provides optional provenance, not required input.

## Recorded evidence and limits

| Area | Recorded evidence | Not established |
| --- | --- | --- |
| TC7476BP / EG logic | Earlier inspection of the manufacturer extract recorded pin assignment, active-low set/reset, reset priority and the 3–18 V recommended supply range relative to VSS. | Exact analog switching voltage, loaded logic levels and complete operating-state sequence. The Tr13 attack-drive mapping is electrically inconsistent as transcribed. |
| EG passive branches | Storage capacitance, resistor branches, unloaded sustain Thevenin equivalent and conditional decay equations are transcribed. | Transistor/FET operating points, active-device loading and quantitative TP4 transfer. |
| IG02610 VCF | IG02610 identification and cutoff/High–Low controls are summarized in the [Original VCF model](../software/Original-VCF-behavioral-design.md). EG depth connection is in the [EG audit](EG-model-audit.md). | Internal topology, control-current-to-cutoff law, input impedance and calibrated resonance/distortion. |
| IG02600 VCA | IG02600 identification and audio/EG/breath path roles are retained as overview findings. The [EG audit](EG-model-audit.md) records the TP4-to-depth-to-39 kohm control branch. | Physical control-voltage-to-gain law, input combination, saturation and loading. The software note gate is an implementation policy. |
| YM10150 | DTG identification and waveform/feet selection roles are overview findings. The [GLS audit](Glissando-circuit-audit.md) preserves its timing branch connections. | Internal generation method, hardware keyboard priority/retrigger rules and GLS threshold/divider timing. |
| Devices and controls | Device identifiers and potentiometer markings are transcribed; the owner's manual supplies LFO/PWM rate endpoints and S–L EG labels. | Applicable low-current device parameters, slider resistance curves and hardware stage-duration calibration. |

## Extent of preserved connection details

Detailed connection records exist for EG passive/reset/control branches and the
YM10150 GLS timing network. The low-frequency audit preserves selected component
observations rather than a complete audio netlist. There is no full pin-by-pin
YM10150 waveform/keyboard/control table or complete IG02610/IG02600 audio/breath
netlist in the authored documents. Overview findings do not imply those detailed
connections have been preserved. Software signal-flow diagrams describe software
routing, not a substitute hardware wiring diagram.

## Current software boundary

Production uses a [stateful exponential EG](../software/EG-stateful-model.md), provisional
behavioral VCF/VCA models and empirical control curves. The independent
[note gate](../software/Note-onset-continuity.md), numerical recovery and regression tests
establish software behavior; they do not establish physical circuit equivalence.
The 1 ms–2 s EG range is a reference-time mapping, not a measured hardware RC
range. Retriggers retain residual state and time edits recalculate remaining time.

The [TC7476BP logic record](TC7476BP-online-investigation.md) preserves the
part-specific pin assignment, truth table and supply limits. Those records
and the connection tables support this analysis without external originals;
they do not establish unmeasured physical operating conditions.
