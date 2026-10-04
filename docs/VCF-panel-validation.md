# CS-01 VCF panel validation

The production cutoff range is 20-20000 Hz with skew 0.3 and 1 Hz steps.
Before quantization its normalized position p maps to
20 + 19980 * p^(1/0.3). The midpoint is about 2002 Hz.
This is a provisional UI mapping, not a verified potentiometer taper.

OriginalVCFProcessor interprets the value directly as Hz. Resonance is
thresholded at 0.5: Low uses 0.2, High 0.7. The schematic establishes a
resistor-switching control, but does not establish these normalized values
or the biquad Q mapping. Do not infer calibrated resonance from them.

The existing response and peak observations were primarily made before
whole-graph oversampling and are not a replacement for a new internal-rate
response sweep. No numerical cutoff or resonance calibration is claimed here.

The new live-control safety test runs the filter at the shared internal rate
for 44.1/48/96 kHz hosts. It switches between minimum, intermediate and maximum
cutoffs and Low/High without resetting state, checks finite bounded output,
and checks decay after silence. The amplitude bound is a safety criterion,
not a hardware specification or a guarantee of click-free switching.

No production coefficients were changed. Outstanding: internal-rate peak and
gain sweep at actual panel positions, and comparison with circuit information
if IC control characteristics become available.