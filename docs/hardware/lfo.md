# Yamaha CS-01 LFO — Technical Summary

## Recorded external behavior

The recorded LFO speed range is 0.8–21 Hz, with a separate PWM speed range
of 0.6–12 Hz. These are preserved owner's-manual endpoints; source provenance
is in [the source catalog](Source-catalog.md).
The LFO target control selects VCO pitch or VCF cutoff modulation.
[Modulation speed ranges](../software/Modulation-speed-ranges.md) records the software
parameters, defaults and saved-state implications.

## Internal implementation: unverified interpretation

The earlier background summary described a triangle source and a digital-logic
LFO, including a TC7476-based attribution and integration with YM10150. Exact
service-page or community-source attribution for those internal claims is not
recorded. The TC7476BP analysis in this project concerns EG switching logic;
it does not by itself establish an LFO topology or YM10150 integration.
Unit-to-unit stability and thermal behavior have not been measured here.

The production software ranges are documented in the modulation guide above.
The shared interpretation/evidence distinction is in
[circuit evidence status](Circuit-evidence-status.md).
