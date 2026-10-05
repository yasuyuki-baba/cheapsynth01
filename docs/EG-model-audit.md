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

### IC4 datasheet-to-wiring correspondence

Source: local `docs/tech/TC7476BP.pdf`, PDF page 1 (printed page 574),
pin assignment and truth table. Here `S_n` and `R_n` denote active-low
asynchronous inputs. To avoid ambiguous first/second numbering, identify each
flip-flop by its set pin: FF-S7 and FF-S2.

| Function | FF-S7 pins | FF-S2 pins | Connection established by the passive transcription |
| --- | --- | --- | --- |
| S_n | 7 | 2 | Pin 7: Tr8 collector/release-drive node; pin 2: storage node E |
| R_n | 8 | 3 | Pin 8: 1 Mohm to ground and D37/D38 junction; pin 3: gate-input node upstream of Tr8 base resistor |
| Q | 11 | 15 | Pin 15: 47 kohm to Tr11 base; pin 11 not yet transcribed |
| Q_bar | 10 | 14 | Pin 10: 47 kohm to Tr13 base; pin 14: D38 to the pin-8 reset network |
| Clock | 6 | 1 | Both tied to ground |
| J | 9 | 4 | Pin 9: -9 V; pin 4: ground |
| K | 12 | 16 | Both tied to -9 V |
| Supply | VDD = 5 | VSS = 13 | Pin 5: ground; pin 13: -9 V (separate IC4 supply/tied-input symbol below EG) |

### Reset-network transcription and bounded state deductions

Source: local overall-circuit PDF, sheet BC1, EG region H4-H6 and the
separate IC4 tied-input symbol below it. This is a drawing transcription;
it is not a measurement of logic thresholds or transistor saturation.

- The gate-input node branches to pin 3 and to the 100 kohm resistor
  feeding Tr8 base. It also has a 22 kohm path to -9 V.
- Tr8 collector branches to pin 7 and to the 100 kohm Tr9 base resistor;
  a 47 kohm resistor pulls this collector node toward -9 V.
- Pin 8 has a 1 Mohm pull toward ground. D37 connects the gate-input
  side to this reset network; D38 connects pin 14 to the same network.
  Do not replace the network with a direct wire or an assumed Boolean OR:
  diode orientation, voltage drops and loaded input levels must be included
  before assigning its logic level.
- The separate IC4 symbol ties pins 1, 4, 5 and 6 to ground, and pins
  9, 12, 13 and 16 to -9 V. Both clock inputs are therefore fixed;
  ordinary clocked J/K transitions are not the intended stage mechanism.
- Logic H/L is relative to IC4's ground/-9 V supply, not to a positive
  0/5 V supply. Negative storage voltage is not automatically logic L.

| Established input condition | Guaranteed logical consequence | Remaining circuit question |
| --- | --- | --- |
| Gate node recognized as L at pin 3 | FF-S2 resets: pin 15 L, pin 14 H, irrespective of E | Gate voltage and diode-network effect on pin 8 |
| E recognized as L at pin 2, while pin 3 is H | FF-S2 sets: pin 15 H, pin 14 L | E switching voltage and resulting D38 current |
| Pin 8 recognized as L | FF-S7 resets: pin 10 H, irrespective of pin 7 | Whether D37/D38 establishes L in each gate phase |
| Pin 7 L and pin 8 H | FF-S7 sets: pin 10 L | Tr8 collector level and reset-network loading |

These conditions expose an asynchronous feedback route from E through
FF-S2/pin 14 and D38 to FF-S7/pin 8. Thus attack termination must be
analyzed as a coupled latch/diode network, not just E crossing a comparator
threshold. The conditional table below resolves diode polarity and the
qualitative sequence, but valid gate/input voltage margins remain unproven.
Pin 11 is not used by
the transcribed EG paths; no connection is inferred for it here.

### D37/D38 polarity and conditional stage sequence

Targeted crop of the same local drawing establishes the cathode bars:
D37 cathode is on the gate-node side, and D38 cathode is on the pin-14
side. Both anodes connect to the pin-8/1 Mohm reset node. A sufficiently
low gate or pin-14 output can therefore sink current from the pull-up
through its diode. This is a low-asserting reset network, not a direct
short between gate and pin 14. Its low voltage includes a forward diode
drop, so valid input margins still require verification.

The following is a **conditional circuit prediction**, not measured gate
timing. H is near ground and L is near -9 V; assume the gate and
diode-clamped reset voltages satisfy IC4 input limits, and the transistor
drives are sufficient. Tr8 and Tr13 are PNP; Tr9 and Tr11 are NPN.

| Phase/condition | Logic and drive prediction | Expected storage-node branch |
| --- | --- | --- |
| Gate L | Pin 3 resets FF-S2 (15 L, 14 H); D37 pulls pin 8 low, resetting FF-S7 (10 H). Tr8 raises its collector toward ground. | Tr13 attack drive removed; Tr11 decay drive removed; Tr9 receives release drive toward ground |
| Gate becomes H, E initially H | Tr8 drive is removed and its collector pulls toward -9 V, asserting pin 7. Pin 14 remains H from reset; D37/D38 no longer clamp pin 8 low. FF-S7 sets (10 L). | Tr13 drives the attack branch toward -9 V; release drive is removed |
| Gate H, E becomes L | Pin 2 sets FF-S2 (15 H, 14 L); D38 pulls pin 8 low. Reset priority overrides pin 7 L, making pin 10 H. | Attack drive removed; Tr11 receives decay drive toward the Tr12 sustain-reference branch |
| Gate H, E later leaves L | FF-S2 retains its set state; pin 14 remains L and keeps FF-S7 reset. | Decay/sustain branch persists; attack does not automatically restart |
| Gate returns L during any phase | Pin 3 resets FF-S2 and D37 asserts FF-S7 reset independently of E. | Release branch selected under the drive assumptions above |

This resolves the qualitative attack-to-decay feedback mechanism. It does
not determine the voltage at which E asserts set, finite stage times,
transistor voltage drops, or loaded sustain equilibrium. Power-up state
and very short gate pulses also remain outside this settled-state table.
Do not integrate this prediction as calibrated DSP until those quantities
and the gate timing are established.

The manufacturer truth table explicitly gives **reset priority** when both
asynchronous inputs are low. Do not substitute the simultaneous-assertion
behavior of another device bearing a 7476-like name.

| R_n | S_n | Q | Q_bar | Meaning |
| --- | --- | --- | --- | --- |
| H | L | H | L | Asynchronous set, independent of J/K/clock |
| L | H | L | H | Asynchronous reset, independent of J/K/clock |
| L | L | L | H | Reset priority, not an unspecified output pair |
| H | H | Depends on stored state and J/K/clock | Complement of Q | Neither asynchronous input asserted |

With both asynchronous inputs high, the datasheet specifies J/K operation on
the falling clock edge: 00 holds, 01 resets, 10 sets and 11 toggles. The rising
edge leaves the output unchanged. The circuit transcription above instead
shows fixed clock inputs; stage transitions must be analyzed through the
asynchronous inputs and their feedback network.

Conditional implications for the already transcribed drive paths:

- FF-S7 set (pin 7 L, pin 8 H) makes pin 10 L. Reset (pin 8 L,
  regardless of pin 7) makes pin 10 H. These are Attack-drive logic states,
  not yet a verified Tr13 ON/OFF table.
- E recognized as L at pin 2, with pin 3 H, makes pin 15 H. Pin 3 L
  makes pin 15 L even if E is L. These are Decay-drive logic states,
  not yet a verified Tr11 ON/OFF table.
- E returning to H merely releases set; it does not by itself reset FF-S2.
  A stage model that treats this input as a memoryless comparator is therefore
  not justified by the truth table.

PDF page 2 (printed page 575) specifies recommended supply voltage of 3--18 V
relative to VSS and input voltage between VSS and VDD. Electrical tables use
VSS = 0 V and supply points of 5, 10 and 15 V. These input-level specifications
are not an exact E-node trip voltage, and must not be directly interpreted as
absolute CS-01 voltages without identifying pins 5 and 13 in the circuit.

The next targeted circuit transcription is pins 8, 3, 5 and 13, followed by
the remaining clock/J/K and inter-flip-flop connections. This is a specific
missing-connection list, not a request to reinspect all previously mapped paths.

### What can now be calculated, and what cannot

For an isolated conducting branch, the drawn series resistance spans roughly
1.8 kohm to 2.0018 Mohm. With 2.2 uF this gives a nominal R*C range of
0.00396 to 4.40396 seconds. These are **branch time constants**, not Attack,
Decay or Release duration specifications; transistor resistance, loading,
potentiometer taper, capacitor tolerance and switching conditions are omitted.

The remaining blocker is no longer image legibility for these paths. It is
mapping the verified IC4 asynchronous behavior to the remaining reset and
supply connections, gate polarity and the E-node switching threshold together
with transistor/FET operating points. The two
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

## Stage equations and sustain-reference loading

This section derives equations from the transcribed external connections,
not from measured transistor parameters. Let `v = V(E)`, `C = 2.2e-6 F`,
and `R_i = 1800 + r_i` ohms, where `r_i` is the actual rheostat resistance
for Attack, Decay or Release. Nominally `0 <= r_i <= 2e6`; the A taper
does not specify its exact dependence on slider position.

For each branch, define `u_i` as the voltage at the transistor-side end
of its rheostat. The capacitor-node equation is

```text
C dv/dt = sum_i ((u_i - v) / R_i) + I_other
```

Only conducting branches belong in this sum; `I_other` represents signed
current into E from IC4 input leakage, FET gate leakage and other omitted
effects. A transistor branch is not necessarily a bidirectional resistor.
Its end voltage and current depend on its drive and operating region.

If exactly one branch conducts, its end voltage is constant, and other
currents are negligible, the reduced solution is

```text
tau = R_i C
v(t) = u_i + (v(0) - u_i) exp(-t/tau)
v_next = u_i + (v_now - u_i) exp(-dt/tau)
```

| Phase | Branch-end voltage in the reduced model | Limitation |
| --- | --- | --- |
| Attack | Tr13 collector; approximately -9 V plus its saturation voltage magnitude if saturated | Base drive and saturation are not established; attack stops at the IC4 threshold, not at the branch target |
| Decay | Tr11 emitter | Depends on Tr11 base drive and its Tr12-driven collector; not justified as a fixed ideal sustain voltage |
| Release | Tr9 emitter | Depends on Tr9 base drive and current; its grounded collector does not make the emitter an ideal ground connection |

The nominal unloaded `R_i C` range is 0.00396 to 4.40396 seconds.
These are **time constants**, not measured slider-end stage durations.
For constant attack target `u_A` and a threshold `v_T` strictly between
the initial voltage and target, the reduced attack duration would be

```text
t_A = R_A C ln((v_0 - u_A) / (v_T - u_A))
```

There is no numerical attack duration until `v_T`, `u_A` and the initial
voltage are established. Decay/release have no finite exact arrival time
in the exponential approximation; a reporting tolerance must be specified.

### Sustain potentiometer: unloaded Thevenin equivalent

Let `x` be the fraction of the nominal B10K track resistance from grounded
terminal 1 to the wiper: `R_top = 10000 x`, `R_bottom = 10000 (1-x)`.
This definition is electrical, not an assertion about UI slider direction.
With Tr12 base disconnected, the 10 kohm track and 12 kohm series resistor
give

```text
V_th(x) = -9 * 10000 x / 22000 V
R_th(x) = (10000 x) || (12000 + 10000 (1-x)) ohms
```

Thus the unloaded wiper spans 0 to approximately -4.09091 V, with
Thevenin resistance spanning 0 to approximately 5454.55 ohms (monotonic
over this interval). Component tolerances and rail variation are excluded.

With positive `I_B12` defined as current leaving the wiper into the NPN
base, `V_B12 = V_th - I_B12 R_th`. In forward-active operation only,
`V_S = V_B12 - V_BE12`, where `V_S` is Tr12 emitter/Tr11 collector.
The 47 kohm resistor draws `(V_S + 9)/47000` amperes toward -9 V;
Tr11 additionally loads this node during decay. Consequently neither a
fixed 0.7 V drop nor the unloaded wiper voltage establishes the loaded
storage-node sustain equilibrium. Tr11 needs its own current/voltage model.

The next quantitative task is to bound Tr12 base loading and Tr11/Tr9
branch behavior from device data or measurements. These equations do not
authorize changing the production ADSR, UI time units or preset meanings.

### Tr12 loading: solvable forward-active approximation

Device source: Toshiba 2SC1815 datasheet, dated 2007-11-01, page 1,
manufacturer PDF hosted by Digi-Key:
`https://media.digikey.com/pdf/Data%20Sheets/Toshiba%20PDFs/2SC1815.pdf`.
The Y rank specifies hFE 120-240 at VCE = 6 V, IC = 2 mA, Ta = 25 C.
This is not a guaranteed beta range at the lower currents in this circuit.
The saturation specification (VCE(sat) <= 0.25 V) is at IC = 100 mA,
IB = 10 mA, not a constant to insert into the EG model.

For a forward-active Tr12 with assumed constant beta and V_BE, let
`I_D` be positive current leaving the sustain node into Tr11 collector.
Emitter-node and base-node current balance give

```text
I_E12 = (V_S + 9)/47000 + I_D
I_B12 = I_E12/(beta + 1)
a = R_th/(beta + 1)
V_S = (V_th - V_BE - a*(9/47000 + I_D)) / (1 + a/47000)
```

This includes loading by both the 47 kohm resistor and the decay branch.
Its small-signal sensitivity to imposed decay current in this approximation
is `dV_S/dI_D = -a/(1+a/47000)` ohms. It excludes transistor dynamic
resistance, beta variation, temperature and collector/base junction effects.

For illustration only, with I_D = 0, V_BE = 0.65 V and assumed beta
120 or 240, the x=1 result is approximately -4.745 V; at x=0 it is
-0.650 V. The base-loading correction at x=1 is only a few millivolts
in this restricted case. These illustrative inputs are not calibration
values and do not establish the capacitor's sustain voltage.

### Tr11 and Tr9: operating region must be checked

For an NPN driven from `V_drive` through base resistance `R_B`, a
forward-active constant-beta approximation would give

```text
I_E = (V_drive - V_BE - v) / (R_stage + R_B/(beta+1))
u_emitter = v + I_E R_stage
```

Use this only for positive I_E and an operating point consistent with
forward-active operation. For Tr11, R_B = 47 kohm and its collector is
V_S, while its base drive approaches ground. Its collector can then be
below its base, forward-biasing the base-collector junction. Therefore
assuming Tr11 is a forward-active emitter follower throughout decay is
not valid: saturation and base-injected current must be included. In
particular, collector current I_D need not remain positive in that region.

Tr9 has R_B = 100 kohm and a grounded collector. Its drive is the loaded
Tr8 collector, not an ideal ground source. The same branch formula is a
candidate only while the active-region assumptions hold; near release
equilibrium, V_BE depends strongly on current. A fixed -0.7 V storage
target cannot be inferred from the TP4 sketch (TP4 is after the buffer).

The equations above identify what a transistor model or measurement must
resolve. Do not substitute the Y-rank test-condition beta limits or a
high-current saturation-voltage limit for guaranteed EG operating bounds.

### Coupled decay equations: conditional fixed-junction-drop model

To expose the coupling without inventing calibrated transistor constants,
suppose both Tr11 junctions are forward biased. Define positive assumed
voltage drops `d_BE11` and `d_BC11`, and let
`d_CE11 = d_BE11 - d_BC11`. These are symbolic approximations, not
datasheet saturation limits. Let `R = R_D`, `B = 47000`, `L = 47000`,
`V_H` be the loaded pin-15 high output voltage, and
`a = R_th/(beta12+1)`. The previous Tr12 active-region approximation then
couples to the saturated Tr11 branch as follows:

```text
V_B11 = V_S + d_BC11
V_E11 = V_S - d_CE11
I_B11 = (V_H - V_S - d_BC11)/B
I_E11 = (V_S - d_CE11 - v)/R
I_D = I_E11 - I_B11
I_E12 = (V_S + 9)/L + I_D
V_S = V_th - d_BE12 - a I_E12
```

Here I_E11 is current leaving Tr11 emitter toward E, I_B11 enters its
base, and I_D enters its collector from the sustain node. Eliminating
the currents gives

```text
D = 1 + a*(1/L + 1/R + 1/B)
V_S = [V_th - d_BE12
       - a*(9/L - (d_CE11 + v)/R - (V_H - d_BC11)/B)] / D
C dv/dt = (V_S - d_CE11 - v)/R
```

Unlike an ideal independent sustain source, this V_S depends on capacitor
voltage, decay resistance and IC4 output drive. If I_B11 exceeds I_E11,
I_D is negative: the base-drive circuit injects current into the sustain
node instead of simply loading it. The previous positive-load example
must not be extrapolated to this case.

This fixed-drop approximation is admissible only while its assumed
junction conditions and positive Tr12 emitter current remain consistent.
If I_E12 becomes negative, the forward-active Tr12 equation fails because
it cannot represent a source that also sinks arbitrary current. Cutoff,
reverse operation, leakage and current-dependent junction voltages require
a different device model. Near zero branch current, treating both junction
drops as constants is especially unreliable; do not use the resulting
algebraic equilibrium as a measured sustain level.

For fixed parameters in this restricted region, V_S = p + q v with
`q = a/(R D)`, so the reduced decay equation has
`tau_eff = R C/(1-q)` and target `(p-d_CE11)/(1-q)`.
These expressions illustrate feedback loading, not an authorization to
replace the production EG or extend this approximation across region changes.

### Reproducible checks and boundary of the approximation

`Tests/eg_decay_equation_check.py` preserves the algebra checks as a
standalone Python 3 unittest suite. Run it from the repository root with
`python3 Tests/eg_decay_equation_check.py -v`. It is not part of the CMake
regression runner and has no third-party dependencies.

The suite checks 72 illustrative parameter combinations for Tr11 current
balance, the loaded Tr12 voltage equation, and equivalence of the branch
derivative to the reduced target/time-constant form. Additional cases check
negative collector current, negative Tr12 emitter current, and disappearance
of feedback for zero Thevenin resistance. Assumed junction drops and beta
values are test inputs only, not fitted or guaranteed device parameters.

An algebraic solution with negative Tr12 emitter current is explicitly a
counterexample to extending the forward-active approximation. Passing these
tests does not validate the assumed transistor operating regions.

Before replacing this approximation with a current-dependent device model,
record the provenance and applicability of its junction-current parameters,
forward/reverse transport parameters, temperature and IC4 output loading.
Solve base, collector and emitter currents simultaneously with the passive
node balances; do not clamp a negative collector current to zero or switch
between fixed drops using an arbitrary voltage threshold. Check node-current
residuals, continuity near zero current and region changes, and sensitivity
to uncertain parameters before comparing trajectories with measurements.
No calibrated nonlinear device model is supplied by this audit yet.

## Circuit-path identification status

The passive EG connections are transcribed in the table above. A complete
operating-state model has not been established: that transcription does not
identify transistor conduction states or effective loaded branch resistance.
Do not infer those states from the presence of a gate signal alone.

| Stage | Required circuit information | Current status |
| --- | --- | --- |
| Attack | Storage node, source voltage, conducting switches/diodes, series and load resistance | Passive path transcribed; conduction and loading unresolved |
| Attack to decay | Switching mechanism and threshold, or absence of a separate threshold | Asynchronous E -> FF-S2 -> D38 -> FF-S7 feedback identified; switching voltage and input margins unresolved |
| Decay | Discharge path and sustain-reference loading | Coupled fixed-junction-drop equations derived conditionally; device parameters and operating-region transitions unresolved |
| Sustain | Reference voltage, potentiometer wiring and buffer transfer | Unloaded wiper Thevenin equivalent derived; Tr12/Tr11 loading and buffer transfer unresolved |
| Release | Gate-off switch states, discharge destination and residual loading | Passive path to Tr9/ground transcribed; switch states and loading unresolved |
| EG output | Storage-node-to-output transfer and polarity | Buffer wiring and TP4 waveform direction transcribed; quantitative transfer unresolved |

This table distinguishes passive connections from missing operating-state
evidence. No ON/OFF states or analog thresholds should be filled from guesswork.

## Decision and bounded next step

Keep the production linear ADSR and its regression tests. Stop adding generic
RC tests as a substitute for identifying the circuit. The next circuit-model
step is to bound the transistor branch voltages, sustain-reference loading,
and IC4 switching voltage. Conditional logic states and reduced stage equations
are now recorded above, but they are not calibrated circuit behavior. The passive
transcription above already supplies the identified paths; repeating overview
image inspection is not the next step. Record any specific unresolved junction
or missing logic connection before requesting further source inspection.

The existing RC tests remain experimental mathematical checks, not evidence of
CS-01 envelope fidelity. This audit is complete as an implementation decision;
the circuit reconstruction itself is incomplete.