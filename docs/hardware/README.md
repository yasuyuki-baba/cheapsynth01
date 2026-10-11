# Hardware evidence and technical background

These documents record source provenance, circuit interpretations and modeling
limits. They do not establish calibration against physical Yamaha hardware.
Use [the DSP design guide](../DSP-design.md) for current software models and
[the product documentation index](../README.md) for iPlug2 behavior.

## Sources and unresolved details

- [Source catalog](Source-catalog.md): source identity, relevant pages and availability
- [Circuit evidence status](Circuit-evidence-status.md): established evidence and remaining gaps
- [Circuit model unknowns](Circuit-model-unknowns.md): unresolved component and control behavior
- [External circuit closeout](External-circuit-closeout.md): recorded audit outcome and limits

## Circuit audits

- [EG model audit](EG-model-audit.md): circuit transcription and conditional equations
- [Glissando circuit audit](Glissando-circuit-audit.md): wiring evidence and control hypotheses
- [Low-frequency circuit audit](Low-frequency-circuit-audit.md): coupling stages and empirical approximations
- [TC7476BP investigation](TC7476BP-online-investigation.md): logic-device evidence and source limitations

The conditional EG equation checks are maintained in
`Tests/research/test_eg_decay_equations.py`. From the repository root, run
`python3 -m unittest discover -s Tests/research -v`.

## Device background

- [YM10150](ymf10150.md): tone-generator background and confidence notes
- [LFO](lfo.md): technical background and attribution limits
- [IG00156 filter](filter.md): related CS-series filter background; this is a different part from CS-01's IG02610 and CS-01II's IG05630

Earlier summaries and implementations are available in Git history.
