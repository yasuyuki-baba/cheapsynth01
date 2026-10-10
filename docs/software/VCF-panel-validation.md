# CS-01 VCF panel validation

> Historical validation: the biquad implementation and test counts described
> here precede the current Original VCF model. For current architecture and
> validation coverage, see [Original VCF behavioral design](Original-VCF-behavioral-design.md)
> and [DSP responsibility boundaries](DSP-responsibility-boundaries.md).
> The measurements below characterize the implementation at the time of each run.

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

No production coefficients were changed in this initial pass. Internal-rate
panel observations were added in the section below; no hardware-calibrated
cutoff or resonance comparison is recorded.

## Internal-rate panel observations

An additional test uses the production cutoff parameter conversion at positions
0, 0.5 and 1 (20, 2002 and 20000 Hz), with Low/High at each shared internal
rate. Integer-Hz sine probes use one second of settling and one second of
measurement, so the analysis window contains whole cycles. Probe frequencies
above 45% of the host rate are omitted. These are filter-core observations,
not final-output measurements including the decimator or VCA.

At a 48 kHz host and midpoint cutoff, Low measured +2.339 dB at 1802 Hz;
High measured +7.464 dB at 2002 Hz. These are maxima among the chosen probes,
not precise resonance peak locations. At minimum cutoff the coupling high-pass
stages also affect the response. Float coefficient precision at very low
cutoffs/high internal rates is a numerical limitation; the observed sample-rate
differences do not establish analog behavior. No calibration change was made.

The new observation test and live-control test pass in Debug and Release.
Full-suite results from before this observation addition were 144/144;
the complete suite has not yet been rerun after this addition.

## Precision improvement and final validation

At 20 Hz the old float calculation of 1-cos(omega) has approximately -6.0%,
+11.3% and +11.3% relative numerator error at 176.4/192/384 kHz respectively.
These are coefficient-construction errors, not directly output gain errors.
An independent identity, 2*sin(omega/2)^2, confirms the cancellation source.

The production low-pass coefficients and recursive states now use double;
b0 uses sin(omega/2)^2 and b1 is twice b0. Audio interfaces and nonlinear
stages remain float. This changes numerical precision, not the intended
cutoff/Q design or provisional panel ranges. Midpoint gains at 48 kHz remain
about +1.721 dB (Low) and +7.464 dB (High) at 2002 Hz.

Debug and Release complete suites pass 146/146 after this change. The
coefficient diagnosis is an isolated mathematical check; it does not yet
assert the production core's low-frequency error against an independent
complete transfer function. Input coupling still uses a float high-pass.
Consequently neither full low-frequency calibration nor all precision
effects are claimed resolved. This historical validation did not include a
Standalone rebuild or listening result.
