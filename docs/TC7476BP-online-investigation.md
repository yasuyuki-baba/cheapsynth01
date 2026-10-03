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

## Required evidence

- Actual TC7476BP pin diagram and truth table (not 7476 or TC74HC76 substitutes).
- Set/reset polarity and simultaneous assertion behavior.
- Supply and input-voltage limits; guaranteed logic thresholds must not be
  treated as an exact analog switching voltage.
- Correspondence between those pins and the CS-01 IC4 wiring.

## Implementation decision

No IC-specific logic, analog threshold or EG time constant is inferred from the
catalog description. Keep the current tested generic ADSR until the actual
datasheet contents and wiring can be validated. This investigation changes no
audio behavior and adds no executable test because no new circuit behavior
has been established.