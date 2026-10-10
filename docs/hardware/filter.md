# Yamaha CS-series VCF (IG00156): recorded background

This concerns IG00156 in related CS-series products, not the CS-01's IG02610
or CS-01II's IG05630. The earlier summary attributed its claims to service
material and community interpretation without exact model/page identifiers.
The claims and their limits are preserved below as unverified interpretations.
Optional source provenance is recorded in [the source catalog](Source-catalog.md).

## Unverified claims retained from the earlier summary

| Subject | Recorded claim | Evidence limit |
| --- | --- | --- |
| Topology | 12 dB/octave state-variable filter. | Internal topology and applicable product are not independently established here. |
| Control sensitivity | Approximately 0–0.25 V for the practical cutoff range. | Control pin, loading and transfer law lack traced source attribution. |
| Resonance | Maximum Q around 10, without self-oscillation. | Neither a measurement nor a part-specific guarantee is recorded. |
| Modulation | LFO ±3 octaves; EG up to +10 octaves in related models. | Exact models and measurement conditions are unidentified. These are not CS-01 specifications. |
| Summing network | R51 22 kohm for cutoff, R52 68 kohm for LFO, R53/R54 33 kohm for EG/tracking, R55 470 ohm for attenuation. | Drawing, model and pin connections are unidentified; these values do not establish a CS-01 network. |
| Frequency span | Roughly 20 Hz–20 kHz. | This was a range interpretation, not a recorded sweep. A hypothetical +10-octave shift of 20 Hz gives 20,480 Hz. |

These related-product interpretations do not establish IG02610/IG05630
equivalence. Current software architecture and its limits
are documented in [Original VCF design](../software/Original-VCF-behavioral-design.md)
and [CS-01II VCF model](../software/CS01II-VCF-model.md).
