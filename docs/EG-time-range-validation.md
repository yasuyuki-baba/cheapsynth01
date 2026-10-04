# EG time range audit

The supplied Japanese owner's manual, printed page 24 (PDF page 13),
specifies Attack, Decay and Release only as S-L. It gives no numeric
minimum or maximum duration. The descriptions on printed pages 16-18
define the stage endpoints, not a duration calibration.

The current 0.001-2 second ranges and skew 0.3 remain provisional.
They must not be described as derived from the A2M potentiometers.
The skew maps normalized position p to approximately
0.001 + 1.999 * p^(1/0.3), before 1 ms quantization.
The midpoint is consequently approximately 0.199 seconds.

The circuit's branch RC constants are not directly stage completion times.
Without the switching thresholds and effective load, expanding or contracting
these ranges would be a usability choice, not verified circuit calibration.

Decision: retain the current ranges and linear ADSR for this change; correct
the misleading comments and test the parameter mappings independently.
Existing envelope tests cover sample-rate scaling and stage behavior, but
do not establish hardware agreement.
 
## Production duration verification

The timing test uses the production APVTS rather than the unit fixture (whose
release range and skew differ). It measures the first sample reaching 1 for
attack, 0.5 for decay, and 0 for release with sustain fixed at 0.5.
Attack includes the peak sample; decay starts at the following sample.
Before note-off, any pre-rendered block remainder is consumed so that the
release event is applied at the actual processing boundary.

Minimum, midpoint and maximum settings are checked at 44.1, 48, 96, 192 and
384 kHz. The numerical acceptance criterion is 1% of the configured duration
plus two samples, not a hardware tolerance. This includes high internal rates
but is not an end-to-end synth graph timing measurement. Maximum-error
aggregation and a tighter numerical error budget remain future work.

## Full graph timing check

The integration test sends MIDI note-on/off through the production synth graph
at 44.1, 48 and 96 kHz and observes the EG after each single host sample.
Minimum, midpoint and maximum positions are checked with sustain at 0.5.
This exercises the shared oversampled graph without confusing EG timing with
the audio filters' transient response.

Because only the last internal sample is observable, the exact attack peak
can occur between observations. The attack detection threshold is
1 - 1/(attackSeconds * hostSampleRate), derived from the linear attack slope
and one host sample's observation interval. Decay timing consequently has up
to approximately one host sample of boundary uncertainty. The acceptance
budget remains 1% plus two host samples. This validates the existing linear
model and its clock scaling, not an analog circuit's stage thresholds.