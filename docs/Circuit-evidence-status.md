# Circuit evidence status

This document summarizes recorded hardware evidence and its limits. Detailed
transcriptions are in [the EG audit](EG-model-audit.md), and source availability
is recorded in [the source catalog](Source-catalog.md). External originals and
private inventories are local-only under `references/` and are excluded from Git.

## Recorded evidence and limits

| Area | Recorded evidence | Not established |
| --- | --- | --- |
| TC7476BP / EG logic | Earlier inspection of the manufacturer extract recorded pin assignment, active-low set/reset, reset priority and electrical limits on printed pages 574–575. | Exact analog switching voltage, loaded logic levels and complete operating-state sequence. The Tr13 attack-drive mapping is electrically inconsistent as transcribed. |
| EG passive branches | Storage capacitance, resistor branches, unloaded sustain Thevenin equivalent and conditional decay equations are transcribed. | Transistor/FET operating points, active-device loading and quantitative TP4 transfer. |
| IG02610 VCF | CS-01 external signal/control connections and High/Low selection are documented. | Internal topology, control-current-to-cutoff law, input impedance and calibrated resonance/distortion. |
| IG02600 VCA | External audio, EG and breath paths are documented. | Physical control-voltage-to-gain law, input combination, saturation and loading. The software note gate is an implementation policy. |
| YM10150 | Waveform/feet selection and external generator/control connections are recorded. | Internal generation method, hardware keyboard priority/retrigger rules and GLS threshold/divider timing. |
| Devices and controls | Device identifiers and potentiometer markings are transcribed; the owner's manual supplies LFO/PWM rate endpoints and S–L EG labels. | Applicable low-current device parameters, slider resistance curves and hardware stage-duration calibration. |

## Current software boundary

Production uses a [stateful exponential EG](EG-stateful-model.md), provisional
behavioral VCF/VCA models and empirical control curves. The independent
[note gate](Note-onset-continuity.md), numerical recovery and regression tests
establish software behavior; they do not establish physical circuit equivalence.
The 1 ms–2 s EG range is a reference-time mapping, not a measured hardware RC
range. Retriggers retain residual state and time edits recalculate remaining time.

The historical `docs/tech/TC7476BP.pdf` used in the earlier audit is absent from
this checkout. Its recorded findings are preserved in
[the investigation report](TC7476BP-online-investigation.md); no new inspection
of an original is claimed here.
