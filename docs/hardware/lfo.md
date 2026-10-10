# Yamaha CS-01 LFO — Technical Summary

## Recorded external behavior

The cited Japanese owner's manual gives an LFO speed range of 0.8–21 Hz and a
separate PWM speed range of 0.6–12 Hz (PDF page 13 / printed page 24).
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

These interpretations are distinct from the production software LFO and are
not treated as verified hardware topology. Source availability and attribution
are recorded in [the source catalog](Source-catalog.md).
