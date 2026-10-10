# TC7476BP online investigation

> Availability note (2026-10-10 JST): the local PDF described below was used
> in the earlier audit but is absent from this checkout. This report preserves
> that audit's findings and retrieval history. See the
> [external source catalog](Source-catalog.md) for acquisition references;
> no new source inspection or URL verification is claimed here.

## Historical online retrieval results

An alternative listing was located at Datasheet Archive:
https://www.datasheetarchive.com/?q=tc7476bp

The archive index lists the part, but its datasheet contents have not been
validated. Do not infer pin functions from this listing.

The Toshiba 1988 databook hosted at Bitsavers could not be opened either:
https://www.bitsavers.org/components/toshiba/_dataBook/1988_Toshiba_TC4000_4500_5000_CMOS_Logic.pdf

Its searchable index describes TC7476BP as a dual J-K master-slave flip-flop
and points to printed page 574. A second manufacturer scan was located:
https://www.bitsavers.org/components/toshiba/_dataBook/1985_Toshiba_C2MOS_Integrated_Circuits.pdf
Opening this second scan returned HTTP 403. Neither index entry establishes
the pinout or truth table.

## Recorded local source inspection

The earlier audit used the supplied `docs/tech/TC7476BP.pdf`. That file is absent
from this checkout. The failed online retrievals above are historical acquisition
results, not a current source inspection.

The recorded visual check covered PDF page 1 / printed page 574 (part-specific
pin assignment and truth table), and PDF page 2 / printed page 575 (recommended
operating conditions and electrical limits). Set/reset are active-low and reset
has priority when both are asserted. Guaranteed voltage limits do not establish
an exact analog switching voltage in the CS-01 circuit.

The [EG circuit audit](EG-model-audit.md) records the pin correspondence,
conditional logic states and recorded reset/supply/clock/J/K connections.
Loaded voltages and switching timing remain unverified.
Its Tr13 attack-drive mapping is inconsistent as transcribed. A complete
physical EG operating-state model is not established.

## Current implementation boundary

Production uses the [provisional stateful exponential EG](../software/EG-stateful-model.md).
The part-specific logic findings support conditional analysis; they do not
calibrate software stage durations, curvature or transistor/FET operating points.
