# CS-01 glissando control: schematic audit

## Connections read from the overall circuit diagram

The high-resolution upper-left schematic crop shows:

- IC1 is the DTG YM10150.
- Its GLS pin has a 0.022 uF capacitor connected to the -9 V rail.
- The GLS control line runs through a 22 kohm resistor to PVR7.
- PVR7 is marked A1M (1 Mohm, A taper).
- PVR7 terminal 1 is connected to -9 V. Its wiper (terminal 2) is
  strapped to terminal 1; terminal 3 connects to the 22 kohm resistor.
- PVR7 therefore acts as a variable resistance, not a voltage divider.

## Quantities that can be calculated

Ignoring IC input impedance and the potentiometer's residual resistance,
the external timing resistance spans 22 kohm to 1.022 Mohm.
With 22 nF, the corresponding RC product spans 0.484 ms to 22.484 ms.
This RC product is NOT the measured semitone-step duration.
The YM10150 timing thresholds, charging behavior and any internal division
must be known to convert it into a glissando step period.

## Implications for the current software

The current 0..208 ms-per-semitone setting with a squared normalized-position
mapping is not established by this schematic. An A-taper marking alone does
not specify a quadratic law or the potentiometer's midpoint resistance.
The minimum external resistance is nonzero; therefore software's instantaneous
zero setting requires separate justification (for example an internal bypass),
which has not been established here.

Changing PVR7 changes the timing resistance during operation. Updating speed
only on a new note is a software simplification, not supported by the external
connections. The timing phase behavior during a resistance change remains unknown.

## Analysis limits

The YM10150 GLS timing equation, oscillator thresholds/divider and PVR7 taper
(including physical slider orientation) are not established. RC products alone
do not define semitone step times. The software range and squared control law
are provisional rather than circuit-derived.

## Interim live-speed implementation

The current software now reads the glissando duration during an active slide.
Changing duration preserves the fractional progress through the current semitone
step, with integer sample rounding. Setting duration below 1 ms moves directly
to the target note, consistent with the existing zero-speed setting.
This is an explicit software approximation, not a verified YM10150 timing model.
The 0–208 ms range and parameter taper remain unchanged.

A production ToneGenerator regression test compares output with an independently
scheduled sequence of semitone changes for faster, slower, and zero-duration
updates at 44.1, 48 and 96 kHz. The schedule uses the actual parameter-mapped duration
and the existing float-to-integer sample conversion; an initial test mismatch
was caused by using double arithmetic for that conversion. The output tolerance
was not relaxed. The existing fixed-duration timing test also passes.

Repeated changes within one step are also tested at 44.1, 48 and 96 kHz,
comparing scalar rendering against partitions of 1, 7, 64 and 256 samples.
After a zero-duration update, measured pitch reaches the target note.
This checks partition consistency, not an independent schedule for every
intermediate step under repeated changes.

The complete graph test holds two notes and changes duration at explicit event
boundaries. Final outputs match for partitions of 7, 64 and 256 samples at all
three rates, and the reference output is checked for nonzero energy.
These are application-driven parameter changes, not arbitrary sample-offset
host automation within an unsegmented block.

Recorded validation at the time of this implementation: Debug and Release
full suites passed 135 tests each, exit code 0. This is a historical count,
not a new full-suite run for the current checkout.
