> Circuit evidence and historical measurements retain their recorded provenance. For the current software model, see [DSP design](../DSP-design.md).

# Circuit evidence status

This document summarizes recorded hardware evidence and its limits. Detailed
connections and conditional equations are in [the EG audit](EG-model-audit.md),
and optional provenance is recorded in [the source catalog](Source-catalog.md).
External originals and
private inventories are local-only under `references/` and are excluded from Git.

## How to read evidence limits

Recorded connections preserve earlier inspection findings. Conditional equations
apply only under their stated loading, conduction and device assumptions.
Software regression checks establish implementation behavior; they do not
calibrate a Yamaha device. Missing device transfers and measured responses stay
unknown. The source catalog provides optional provenance, not required input.

## Recorded evidence and limits

| Area | Recorded evidence | Not established |
| --- | --- | --- |
| TC7476BP / EG logic | Earlier inspection of the manufacturer extract recorded pin assignment, active-low set/reset, reset priority and the 3–18 V recommended supply range relative to VSS. | Exact analog switching voltage, loaded logic levels and complete operating-state sequence. The Tr13 attack-drive mapping is electrically inconsistent as transcribed. |
| EG passive branches | Storage capacitance, resistor branches, unloaded sustain Thevenin equivalent and conditional decay equations are transcribed. | Transistor/FET operating points, active-device loading and quantitative TP4 transfer. |
| IG02610 VCF | IG02610 identification and cutoff/High–Low controls are summarized in the [Original VCF model](../DSP-design.md#filter-models). EG depth connection is in the [EG audit](EG-model-audit.md). | Internal topology, control-current-to-cutoff law, input impedance and calibrated resonance/distortion. |
| IG05630 VCF | A user-supplied CS-01II drawing and the secondary article's enlarged excerpt were inspected on 2026-11-02. Selected capacitor, summing-network and resonance-control connections are transcribed [below](#ig05630-external-network-investigation-2026-11-02). | Internal integrators/summer, Vin/VQ transfers, loaded source impedance, control law and measured hardware response. The conditional equations are not an identified IC transfer function. |
| IG02600 VCA | IG02600 identification and audio/EG/breath path roles are retained as overview findings. The [EG audit](EG-model-audit.md) records the TP4-to-depth-to-39 kohm control branch. | Physical control-voltage-to-gain law, input combination, saturation and loading. The software note gate is an implementation policy. |
| YM10150 | DTG identification and waveform/feet selection roles are overview findings. The [GLS audit](Glissando-circuit-audit.md) preserves its timing branch connections. | Internal generation method, hardware keyboard priority/retrigger rules and GLS threshold/divider timing. |
| Devices and controls | Device identifiers and potentiometer markings are transcribed; the owner's manual supplies LFO/PWM rate endpoints and S–L EG labels. | Applicable low-current device parameters, slider resistance curves and hardware stage-duration calibration. |

## Extent of preserved connection details

Detailed connection records exist for EG passive/reset/control branches and the
YM10150 GLS timing network. The low-frequency audit preserves selected component
observations rather than a complete audio netlist. There is no full pin-by-pin
YM10150 waveform/keyboard/control table or complete IG02610/IG02600 audio/breath
netlist in the authored documents. Overview findings do not imply those detailed
connections have been preserved. Software signal-flow diagrams describe software
routing, not a substitute hardware wiring diagram.

## Current software boundary

Production uses a [stateful exponential EG](../DSP-design.md#envelope-and-note-gate), provisional
behavioral VCF/VCA models and empirical control curves. The independent
[note gate](../DSP-design.md#envelope-and-note-gate), numerical recovery and regression tests
establish software behavior; they do not establish physical circuit equivalence.
The [EG RC timing mapping](../EG-RC-timing.md) uses assumed A-taper resistance
and software thresholds, not measured potentiometer or IC behavior. Retriggers retain residual state and time edits recalculate remaining time.

## IG02610 external-network investigation, 2026-11-02

The nominal capacitor values are C1 / pin 3 through 22 nF and C1' / pin 5
through 2 nF to the K0 common line, plus a separate C2 / pin 6 node with 22 nF
to ground and 10 Mohm to -9B. The earlier attribution of C2 to the K0 common
node was incorrect: direct reinspection on 2026-11-04 finds no junction dot at
the crossing. Do not substitute the secondary redraw's two equal 22 nF branches.
The resonance switch shorts the 10 kohm KN-to-K0 branch; the supplied crop does
not label which contact state is High. See the [external-network review](VCF-external-network-review.md)
for the corrected connections, IN/C3 and Q-adjust branches, and unresolved
CS01II LP-to-Vin path.
These unequal external branches do not establish an 11:1 audio-pole ratio.
Unknown IC port transfers and loading prevent a unique audio-filter/Q law.

Original's provisional unit-DC two-pole model removes its former additional
lowpass-state subtraction and uses Low/High Q as explicit sound choices.
Neither those Q values nor the internal state-variable topology are derived
from the capacitor markings. See [the software model](../DSP-design.md#filter-models)
for the intentional sound change and unchanged parameter/modulation surfaces.

## IG05630 external-network investigation, 2026-11-02

This investigation concerns CS-01II, not CS-01's IG02610. Its matched-loading
hypothesis is now used provisionally by Modern; it is not hardware calibration.
The [source catalog](Source-catalog.md) identifies the supplied partial drawing
and the secondary article; neither supplies IG05630 internal documentation.
The article's description of IG05630 as state-variable is an interpretation
of its terminals and wiring, not a manufacturer block diagram.

### Selected connections

Values below are nominal schematic markings. A connection crossing without
a junction dot is not a summing-node connection.

| Branch | Connection observed |
| --- | --- |
| HP | Pin 18 through 82 kohm to IN- / pin 20 |
| BP | Pin 14 through 15 kohm to IN- / pin 20; crosses IN+ without joining it |
| LP feedback | Pin 10 through 82 kohm to IN- / pin 20 |
| IN- bias | Pin 20 through 8.2 kohm to the -4 V reference |
| S1 | Pin 16 through 22 kohm to IN+ / pin 19 |
| S3 | Pin 12 through 22 kohm to IN+ / pin 19 |
| C1 | 680 pF between pin 17 and S1 / pin 16 |
| C2 | 680 pF between pin 15 and BP / pin 14 |
| C3 | 680 pF between pin 13 and S3 / pin 12 |
| C4 | 680 pF between pin 11 and LP / pin 10 |
| Additional LP branch | LP / pin 10 through a separate 82 kohm to Vin / pin 2 |
| Audio input | Tone-generator OUT through 1 uF coupling to IN+; a 100 kohm shunt is on the source side |
| Resonance | PVR11 and Q ADJ. feed VQ / pin 3 through the marked 4.7 kohm branch |
| Audio output | LP / pin 10 through 1 uF coupling toward TP3 and the VCA |

In particular, BP does **not** join the S1/S3 IN+ network. The four capacitors
do **not** share IN- or ground as a common return. Earlier conversational
descriptions grouping these branches were incorrect.

### Conditional ideal-integrator calculation

Assume four equal-frequency **inverting** integrators with state taps
HP = x0, S1 = x1, BP = x2, S3 = x3, LP = x4, so
`xi = -x(i-1)/p`, where `p = s/omega0`. Also assume an ideal differential
summer enforcing IN+ = IN-, ideal unloaded state outputs, and AC ground at
the -4 V reference.

Approximate the source by a constant series resistance Rs. This omits the
1 uF coupling, the unknown custom-IC output impedance, and its 100 kohm load.
The additional LP-to-Vin branch and the internal actions of Vin/VQ are **not
solved**. These are substantial assumptions, not facts established by the
four equal external capacitors.

For small-signal voltages measured relative to bias, the two node equations
under those assumptions are:

```text
Gminus = 2/82k + 1/15k + 1/8.2k
Gplus  = 2/22k + 1/Rs
IN- = (x0/82k + x2/15k + x4/82k) / Gminus
IN+ = (input/Rs + x1/22k + x3/22k) / Gplus
```

Substitution gives the conditional transfer:

```text
H(p) = K / (p^4 + a*p^3 + b*p^2 + a*p + 1)
a = (82/22) * Gminus/Gplus
b = 82/15
K = (82k/Rs) * Gminus/Gplus
```

The resistor ratios alone do not fix `a`: source loading matters. Choosing
Gplus = Gminus gives Rs = 8.190 kohm, a = 3.72727 and b = 5.46667.
This is an illustrative loading choice, **not a measured source resistance**.
With Rs = 4.7/10/22 kohm, a becomes 2.614/4.159/5.822 respectively.
The omitted input capacitor has reactance about 3.18 kohm at 50 Hz and
318 ohm at 500 Hz; it cannot automatically be ignored when calibrating bass loss.

The illustrative matched-loading response reaches -3.0103 dB at
0.49164 * omega0. A four-equal-RC cascade reaches it at 0.43498 * omega0;
a fourth-order Butterworth response reaches it at omega0. Thus a state-tap
network can plausibly have early rolloff without being a transistor ladder.

### Provisional Modern implementation

Modern deliberately replaces the former Butterworth cascade with four
trapezoidal integrators and weighted state feedback. It uses the illustrative
matched-loading values above, normalizes K to 1, and adds k to the denominator's
constant term:

```text
H(p) = 1 / (p^4 + a*p^3 + b*p^2 + a*p + 1+k)
a = 82/22, b = 82/15, k = 3.2 * resonance
```

The linear resonance law and its placement are explicit hypotheses, **not a
derived VQ transfer or a measured slider curve**. DC gain is 1/(1+k).
The quartic Routh criterion gives stability for `0 <= k < b-2` when a > 0
and b > 2. The chosen upper bound 3.2 is below b-2 = 3.46667; this models
decaying resonance rather than proving anything about hardware self-oscillation.

The state feedback is solved simultaneously with all four trapezoidal
integrators, without the previous one-sample feedback delay. The existing
empirical asymmetric input limiting is now upstream of this linear loop;
the former tanh output-feedback stage is removed. Output remains limited
to +/-1.5. Input/output scaling and limiting are not calibrated IC voltages.
The physical source impedance and 1 uF coupling are not simulated; matched
loading is a fixed assumption, not a newly identified circuit parameter.

This is an intentional product-sound change: existing Modern factory sounds,
saved patches and host sessions retain their parameter values but sound
different. There is no legacy Modern mode or parameter migration. Original
is unchanged. The UI cutoff remains the integrator natural-frequency scale,
not the zero-resonance -3 dB frequency (about 0.49164 times that scale).

Independent small-signal tests compare the rendered response with the
polynomial after the bilinear frequency substitution, rather than copying the
state-update arithmetic. DC gain, reset, impulse decay, modulation, partition
independence and nonfinite recovery are also checked. These are software
properties, not physical-instrument calibration.

External-plugin measurements, comparisons and their reproducibility material
are kept only under ignored `references/`, not as shared hardware evidence.

The [TC7476BP logic record](TC7476BP-online-investigation.md) preserves the
part-specific pin assignment, truth table and supply limits. Those records
and the connection tables support this analysis without external originals;
they do not establish unmeasured physical operating conditions.
