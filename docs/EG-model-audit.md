# CS-01 EG model: implementation decision

## High-resolution transcription (local overall-circuit PDF)

The following is a passive connection transcription, not a claim about switch
conduction states. It supersedes the earlier blanket statement that no paths
could be identified. No production coefficients are changed.

Define `E` as the vertical storage node connected to the negative terminal of
the capacitor marked `2.2/50` (2.2 uF / 50 V); its positive terminal connects
to the ground symbol. The node also connects to the FET1 gate.

| Path | Connections visible in the crop |
| --- | --- |
| Attack | E -> 1.8 kohm -> A2M Attack potentiometer terminal 1; terminals 2 and 3 are strapped -> Tr13 collector; Tr13 emitter -> -9 V |
| Decay | E -> 1.8 kohm -> A2M Decay potentiometer terminal 1; terminals 2 and 3 are strapped -> Tr11 emitter |
| Sustain reference | B10K Sustain terminal 1 -> ground; terminal 3 -> 12 kohm -> -9 V; wiper -> Tr12 base; Tr12 collector -> ground; Tr12 emitter -> Tr11 collector and 47 kohm -> -9 V |
| Release | E -> 1.8 kohm -> A2M PVR16 terminal 1; terminals 2 and 3 are strapped -> Tr9 emitter; Tr9 collector -> ground |
| Feedback | E -> IC4 second flip-flop S input (pin 2) |
| Attack drive | IC4 first flip-flop complementary output pin 10 -> 47 kohm -> Tr13 base |
| Decay drive | IC4 second flip-flop Q pin 15 -> 47 kohm -> Tr11 base |
| Release drive | Tr8 collector -> IC4 first flip-flop S pin 7, 47 kohm -> -9 V, and 100 kohm -> Tr9 base |
| Output buffer | FET1 source-side node -> 3.3 kohm -> Tr14 base and 15 kohm -> -9 V; Tr14 emitter -> TP4; Tr14 collector -> -9 V |
| VCA depth | TP4 -> B10K depth terminal 3; terminal 1 -> ground; wiper -> 39 kohm -> VCA control path |
| VCF depth | TP4 -> D36 -> B10K depth terminal 3; terminal 1 -> ground; wiper -> 33 kohm -> VCF control path |

The parts legend identifies Tr9/11/12 as 2SC1815, Tr13/14 as 2SA1015,
FET1 as 2SK30A and IC4 as TC7476BP. The drawing labels both Sustain and
Decay as PVR14; retain functional names rather than silently correcting the
duplicated designation.

The TP4 waveform sketch moves downward at key-on and returns toward
-0.7 +/- 0.2 V after key-off. Thus the normalized software envelope's positive
direction is not the physical voltage direction. Buffer offsets matter.

### What can now be calculated, and what cannot

For an isolated conducting branch, the drawn series resistance spans roughly
1.8 kohm to 2.0018 Mohm. With 2.2 uF this gives a nominal R*C range of
0.00396 to 4.40396 seconds. These are **branch time constants**, not Attack,
Decay or Release duration specifications; transistor resistance, loading,
potentiometer taper, capacitor tolerance and switching conditions are omitted.

The remaining blocker is no longer image legibility for these paths. It is
deriving IC4 asynchronous set/reset behavior, gate polarity and the E-node
switching threshold together with transistor/FET operating points. The two
timing branches cannot safely be assumed to alternate as a conventional
software ADSR without that derivation. Keep production integration deferred.

The production EG remains JUCE's linear ADSR. Its lifecycle and parameter-update
regressions are covered separately. It is not claimed to reproduce the analog
charging/discharging curve.

## Verified candidate

`Tests/unit/RCEnvelopeModelTest.cpp` evaluates an ideal isolated RC section:

    V(t) = target + (V(0) - target) * exp(-t / tau)
    tau = R * C

The discrete update uses expm1 for small-step accuracy. Tests compare every
sample with the independent continuous-time expression at 44.1, 48 and 96 kHz.
Changing resistance or switching the target retains the stored capacitor voltage.
These tests validate the numerical primitive, not the CS-01 circuit topology.

## Why production replacement is deferred

An isolated RC formula alone does not establish the actual attack target,
switching threshold, loading, effective resistance or sustain buffer behavior.
The current UI parameters are seconds, not identified physical resistor values.
A time constant also differs from a finite stage duration: an ideal exponential
never reaches its target exactly. A termination threshold and conversion from
the existing time parameters would therefore be additional assumptions.

Do not silently introduce an arbitrary overshoot target, threshold, or taper and
call it circuit-derived. Before integration, document each charging/discharging
path, its component values, loading assumptions and stage transitions. Then test
the complete state machine, automation, retriggering and compatibility with the
existing presets. No production sound change is made by this experiment.

## Loading experiment

A third test covers a hypothetical capacitor driven through a source resistance
with a resistive load to ground. Its effective target is the voltage-divider
equilibrium and its time constant uses the parallel resistance. The discrete
model is compared with the independent solution of the capacitor-node KCL
equation. The resistor and capacitor values in this test are deliberately
synthetic, not transcribed CS-01 component values.

Consequently, reading a potentiometer value and multiplying by capacitance is
not sufficient to establish an EG stage duration. Both loading and the driving
voltage must be identified first. This experiment does not identify the actual
CS-01 stage transitions or justify replacing the production ADSR.

## Circuit-path identification status

The actual EG netlist has not been established. Repeated inspection of the
overall diagram did not produce a reliable component-by-component connection
table. Do not infer transistor conduction states from the presence of a gate
signal or assume that a potentiometer alone sets the effective resistance.

| Stage | Required circuit information | Current status |
| --- | --- | --- |
| Attack | Storage node, source voltage, conducting switches/diodes, series and load resistance | Not established |
| Attack to decay | Switching mechanism and threshold, or absence of a separate threshold | Not established |
| Decay | Discharge path and sustain-reference loading | Not established |
| Sustain | Reference voltage, potentiometer wiring and buffer transfer | Not established |
| Release | Gate-off switch states, discharge destination and residual loading | Not established |
| EG output | Storage-node-to-output transfer and polarity | Not established |

This table records missing evidence; it is not a reconstructed schematic.
No component values or ON/OFF states should be filled from guesswork.

## Decision and bounded next step

Keep the production linear ADSR and its regression tests. Stop adding generic
RC tests as a substitute for identifying the circuit. The next circuit-model
step requires a legible EG-only crop with junctions, component designations and
values, followed by explicit node-to-node transcription. If the supplied PDF
does not resolve these details, record the specific ambiguous junctions rather
than repeatedly rereading the same overview. Only then derive stage equations.

The existing RC tests remain experimental mathematical checks, not evidence of
CS-01 envelope fidelity. This audit is complete as an implementation decision;
the circuit reconstruction itself is incomplete.