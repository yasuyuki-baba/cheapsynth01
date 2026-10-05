# TC7476BP online investigation

## Outcome

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

## Local evidence and scope of this record

The local file `docs/tech/TC7476BP.pdf` is available for the next pinout and
truth-table audit. The failed online retrievals above describe the earlier
online investigation, not the current availability of local source material.
They must not be used as a reason to repeat archive searches or overview reads.

`docs/EG-model-audit.md` now contains a passive transcription of IC4-related
connections. Passive wiring alone does not establish asynchronous logic states
or an exact analog switching voltage. The remaining work is to record the
datasheet-to-wiring correspondence explicitly, with page references and any
unresolved connections, rather than infer it from catalog descriptions.

## Local datasheet verification

The local PDF has now been visually checked: PDF page 1 / printed page 574
contains the part-specific pin assignment and truth table; PDF page 2 / printed
page 575 contains recommended operating conditions and electrical limits.
Set and reset are active-low, and reset has priority when both are asserted.
The pin-by-pin correspondence, truth-table summary and conditional drive states
are recorded in `docs/EG-model-audit.md` under IC4 datasheet-to-wiring
correspondence. Remaining reset, supply and clock/J/K wiring is explicitly
listed there; this is not yet a complete EG state model.

## Evidence checklist

- Verified: actual TC7476BP pin diagram and truth table (not substitutes).
- Verified: active-low set/reset and reset-priority simultaneous assertion.
- Supply and input-voltage limits; guaranteed logic thresholds must not be
  treated as an exact analog switching voltage.
- Correspondence between those pins and the CS-01 IC4 wiring.

## Implementation decision

IC-specific logic is now supported by the local datasheet, not the catalog
description. An analog threshold or EG stage duration is still not established.
Keep the current tested generic ADSR until the remaining wiring and operating
states can be validated. This investigation changes no
audio behavior and adds no executable test because no new circuit behavior
has been established.