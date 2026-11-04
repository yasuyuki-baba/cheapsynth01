# VCF external-network review — 2026-11-04 JST

## Sources and scope

Direct inspection of two images supplied in the conversation: first CS01 /
IG02610, then CS01II / IG05630. These are partial drawings without a recorded
manual revision or page number. Some incoming control/audio branches and the
downstream VCA circuit are outside the crops. Original image files are not
available in this checkout; this document preserves the selected observations.
Connections below use visible junction dots; crossings alone are not junctions.

This review changes the connection record, not DSP coefficients. Code separation
does not establish completeness or hardware accuracy of the external networks.

## CS01: selected passive connections

| Connection | Visible component / wiring | Software treatment |
| --- | --- | --- |
| C1 / pin 3 to K0 common line | 0.022 uF (22 nF) | Not represented as a separate capacitor branch |
| C1' / pin 5 to the same K0 line | 0.002 uF (2 nF) | Not represented as a separate capacitor branch |
| C2 / pin 6 to ground | 0.022 uF (22 nF) | Not represented as a separate capacitor branch |
| C2 / pin 6 to -9B | 10 Mohm | Bias/leakage branch omitted |
| K0 to KN | 10 kohm, bypassed by PSW4 | Replaced by two provisional Q settings; resistor/switch equations omitted |
| C3 to KN | VR11 B20K, with wiper tied to the C3 end | Q-adjust rheostat omitted; nominal adjustment range 0–20 kohm, without contact resistance |
| IN / pin 4 to C3 / pin 14 node | 1 kohm | Omitted |
| C3 / pin 14 node to ground | 100 uF / 10 V | Omitted |
| Cutoff control into Ic / pin 1 | PVR10 10KA, 39 kohm, 22 kohm, VR14 B100K and incoming control branches | Software frequency/modulation law, not a solved control-current network |
| GO audio output to TP3 | 1 uF / 50 V series capacitor | Audio coupling omitted by explicit software policy |
| TP3 to ground | 47 kohm | Output loading omitted |
| TP3 toward VCA | 82 kohm series, then additional visible components outside the VCF boundary | Not solved as a connected source/load network |

**Correction to the earlier transcription:** the C2 horizontal line crosses
the K0 capacitor-common vertical line without a visible junction dot. The C2
capacitor/bias node must not be merged with K0. The previous statement that
C1/C1', K0 and C2 share one node is superseded by this inspection.

The switch visibly bypasses 10 kohm; the crop does not label which contact state
is the panel's High position. Neither the switch resistance nor VR11 determines
Q without the IC's terminal transfers. In particular, the production Q values
0.5/1.6 are not derived from this network. The unequal 22 nF / 2 nF capacitors
also do not, by themselves, establish an 11:1 ratio of audio poles.

### What can already be written without inventing an IC model

Let voltages be small-signal perturbations around bias, assume the supply rails
are AC ground, and define each `I_port` as current entering the IC. In the
Laplace domain, selected external-node equations are:

```text
C2: s*22n*V_C2 + V_C2/10M + I_C2 = 0

K0 (switch open):
    s*22n*(V_K0 - V_C1) + s*2n*(V_K0 - V_C1prime)
    + (V_K0 - V_KN)/10k + I_K0 = 0

C3 (finite positive rheostat resistance Rq):
    s*100u*V_C3 + (V_C3 - V_IN)/1k
    + (V_C3 - V_KN)/Rq + I_C3 = 0
```

Switch closure imposes `V_K0 = V_KN`; the zero-resistance rheostat endpoint
similarly imposes `V_C3 = V_KN`. These cases should use joined nodes rather
than a division by zero. The supply assumption is not a power-supply model.

These equations preserve known external connections, but do not close the
system: the IC relationships between terminal currents and voltages are still
unknown. IN/C3 and VR11 must not be dismissed as irrelevant supply bypassing
without that information.

## CS01II: the extra LP-to-Vin branch

The left-hand 82 kohm connects LP / pin 10 to Vin / pin 2. It is distinct from
the right-hand 82 kohm connecting LP to IN- / pin 20. Both must appear in a
complete circuit connection record.

The branch equation is known:

```text
i_LP_to_Vin = (V_LP - V_Vin) / 82k
```

The drawing alone does not supply Vin input impedance, its relationship to
internal current/voltage states, or its interaction with VQ. Consequently:

- It cannot be assumed to duplicate the LP-to-IN- branch.
- Adding another `1` or an `82k` ratio to the current denominator is unjustified.
- The current `1 + 3.2 * resonance` last coefficient is a behavioral hypothesis;
  it is not a derivation of the LP-to-Vin/VQ circuit.
- Modelling the branch requires an explicit IC-port hypothesis or measured
  evidence, stated separately from the known 82 kohm external resistor.

Other visible connections agree with the earlier CS01II transcription: four
680 pF capacitors across their respective integrator terminals; HP/BP/LP
branches of 82k/15k/82k to IN-; S1/S3 branches of 22k to IN+; and 8.2k from
IN- to the -4 V reference. The VQ branch through 4.7k and B bias branch through
47k are visible, but their upstream controls/operating transfers are not fully
provided by this crop.

The production `a=82/22` and `b=82/15` coefficients incorporate only a
conditional reduction of part of this network. In particular, `a` assumes
matched input-node conductance. The 8.2k bias branch contributes to small-signal
node loading under an AC-ground supply assumption; equal node conductance
cannot be inferred from the resistor labels alone. Absolute input gain is
also normalized in the current model, rather than solved from the source load.

## Output coupling: an actual component-based example

CS01 visibly has 1 uF followed by a 47 kohm shunt at TP3. If the IC output were
an ideal zero-impedance voltage source and the branch beyond TP3 drew no current,
that isolated section would have a first-order corner of:

```text
fc = 1 / (2*pi*47k*1u) = approximately 3.39 Hz
```

That is a conditional calculation, not the instrument's measured corner. The
actual GO source impedance and the downstream 82k/VCA branch alter the network.
The CS01II crop also shows a 1 uF output capacitor and an 82k onward branch,
but does not establish an identical 47k shunt or downstream load. This evidence
does not justify restoring the former 20 Hz second-order / 8 Hz first-order
software filters or assigning the same physical coupling to both instruments.

## Follow-up boundary

Known passive connectivity can be represented independently of IC hypotheses.
A complete audible transfer still needs IC terminal models, source/load
impedances and operating-point assumptions. Keep those unknowns explicit rather
than treating the current aggregate feedback arrays as a complete external
netlist. No missing branch is added to production DSP during this review.

For the selected lightweight approach, see the [musical-model note](VCF-musical-model.md).
The [nodal-simulation proposal](VCF-network-implementation-plan.md) is deferred research.
