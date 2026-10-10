# TC7476BP: recorded logic and supply limits

This document preserves the part-specific findings used by the EG analysis.
The pin assignment, logic rules and numerical supply limits needed to follow
that analysis are given below. External originals are not a reading prerequisite.
[The source catalog](Source-catalog.md) retains source pages, acquisition links
and historical retrieval results. These findings are earlier inspection records,
not a new datasheet verification.

## Pin assignment

TC7476BP is recorded as a dual J-K master-slave flip-flop. Each flip-flop is
named here by its asynchronous set pin, avoiding ambiguous first/second numbering.
`S_n` and `R_n` are active-low asynchronous set and reset; `Q_bar` is the
complementary output.

| Function | FF-S7 pin | FF-S2 pin |
| --- | --- | --- |
| S_n | 7 | 2 |
| R_n | 8 | 3 |
| Q | 11 | 15 |
| Q_bar | 10 | 14 |
| Clock | 6 | 1 |
| J | 9 | 4 |
| K | 12 | 16 |

The shared supply pins are VDD = 5 and VSS = 13.

## Asynchronous truth table

H and L denote recognized logic levels relative to the device supply; they
are not exact analog switching voltages.

| R_n | S_n | Q | Q_bar | Behavior |
| --- | --- | --- | --- | --- |
| H | L | H | L | Set, independent of J/K/clock |
| L | H | L | H | Reset, independent of J/K/clock |
| L | L | L | H | Reset priority |
| H | H | Stored/clocked state | Complement of Q | No asynchronous input asserted |

With both asynchronous inputs H, the recorded falling-edge clock behavior is:
J/K = 00 holds, 01 resets, 10 sets, and 11 toggles. A rising edge leaves
Q unchanged. Reset priority is a part-specific recorded rule, not an inference
from another device with a similar 7476 name.

## Supply limits and CS-01 interpretation

- Recommended VDD–VSS: 3–18 V.
- Recommended input-voltage range: VSS through VDD.
- Recorded electrical tables use VSS = 0 V, with supply points 5, 10 and 15 V.
- Recorded CS-01 connections place pin 5 at ground and pin 13 at -9 V,
  giving a nominal 9 V supply difference. Thus circuit H is near ground and
  L near -9 V, rather than positive 0/5 V logic.

These ranges do not identify an exact E-node trip voltage or guarantee that
the diode-clamped reset voltages meet logic input margins. Loaded voltages,
output drive and switching timing are unmeasured.

## Circuit and software boundary

[The EG circuit audit](EG-model-audit.md) records the storage node, reset
network, fixed clock/J/K connections and conditional latch states. Its Tr13
attack-drive mapping is electrically inconsistent as transcribed. The logical
rules above do not establish a complete physical EG operating-state model.

Production uses the [provisional stateful exponential EG](../software/EG-stateful-model.md).
These logic records do not calibrate software stage durations, curvature or
transistor/FET operating points.
