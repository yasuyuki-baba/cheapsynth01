# VCF external-network implementation proposal

Status: deferred circuit-simulation exploration, not the current implementation
plan. The user selected a lightweight musical approximation instead; see
[the current approach](VCF-musical-model.md). The proposal below is retained
as optional future research, not a requirement for production changes.

This proposal follows the [drawing review](VCF-external-network-review.md).
It separates visible passive connectivity from hypotheses about custom-IC
terminal behavior. It does not imply a unique circuit reconstruction is possible
from the supplied partial drawings.

## Recommended direction

Use a small, fixed-size nodal circuit solver shared by both instruments. Each
instrument declares its own passive network; each IC supplies its own terminal
model. Solve both together at the internal audio rate, without a sample of delay
at their feedback boundary. Keep the existing processor-facing API.

Do not extend the current aggregate feedback arrays with guessed extra constants.
Those arrays express monic all-pole denominators with a fixed numerator. They
cannot generally express terminal loading, additional capacitor states, coupling
zeros or frequency-dependent feedback through C3. The current integrator core
remains useful as a reference for the earlier behavioral model, not as a complete
replacement for the newly transcribed network.

## Responsibilities

| Owner | Owns | Must not infer silently |
| --- | --- | --- |
| `CS01VCFCircuit` / `CS01IIVCFCircuit` | Node names, R/C values, switch wiring, pot wiring, boundary connections | IC internal transfer or unshown control branches |
| IG02610 / IG05630 models | Named IC ports, assumed terminal admittance/transconductance, buffer loading, nonlinear behavior | Board resistor values or panel-to-resistance mappings |
| Source / downstream load models | Source impedance, VCA input load, conversion between synth units and volts | A universal unity-gain or infinite-impedance termination |
| Shared solver | Passive/controlled-source assembly, capacitor history, fixed-size factorization and state updates | Instrument-specific topology or numerical leakage disguised as a component |

Keep observed component values and provisional IC/source/load parameters in
separate types. A port called Vin must remain distinct from IN+ or IN- until an
explicit IC hypothesis connects their behavior. The common IC interface is an
equation-contribution contract, not an identical pinout for different devices.

Conceptual interfaces, not final C++ declarations:

```text
instrument network: addResistor(nodeA, nodeB, ohms)
                    addCapacitor(nodeA, nodeB, farads)
                    connectSwitch(nodeA, nodeB, state)

IC model:           definePorts(...)
                    contributeEquations(system, controls, assumptions)

circuit wrapper:    prepare(sampleRate)
                    setCutoffFrequency(...)
                    setResonance(...)
                    processSample(...)
```

Board controls first produce circuit quantities such as rheostat resistance.
The mapping from cutoff control current or VQ voltage to IC behavior belongs to
the explicit IC hypothesis. Retain existing host parameter IDs/ranges; do not
present trim adjustments or model uncertainty as new end-user controls.

## Numerical method

Start with linear or affine IC-port hypotheses and trapezoidal integration.
Resistors contribute conductances. A capacitor between arbitrary nodes contributes
`2*C/h` conductance and its history current, where `h` is the internal sample
period. Preserve capacitor voltage/history across ordinary control changes.

In descriptor form the complete linearized system is:

```text
E * dx/dt = A * x + B * u

(E - h*A/2) * x[n] = (E + h*A/2) * x[n-1]
                    + h*B*(u[n] + u[n-1])/2
```

This expression assumes constant matrices over the integration interval.
For changing transconductance/control values, define their timing explicitly
and use the corresponding endpoint equations/history consistently. Algebraic
nodes have no artificial capacitor state. Use an appropriate modified nodal
form when ideal constraints or controlled sources require it.

- All arrays and matrix storage have compile-time bounds; no heap allocation
  or topology construction in `processSample`.
- Prebuild the finite switch-topology variants. A closed switch joins nodes;
  do not replace it with an arbitrary tiny resistor. Treat a rheostat's exact
  zero endpoint as a joined-node constraint too.
- Use double precision, pivoting, scaled equations and singular-system checks.
  Do not silently ground floating nodes with a fictitious resistor.
- Reuse a factorization only while its coefficients are unchanged. Audio-rate
  cutoff modulation can require coefficient updates/factorization at each
  internal tick; measure cost at up to 16x oversampling before shipping.
- Initially retain nonlinear shaping as an explicitly identified feed-forward
  behavioral stage where possible. If a chosen IC hypothesis places nonlinear
  elements inside feedback, solve that system together with bounded iterations;
  do not insert a delay merely to break the loop.

Changing the numerical solver does not improve hardware evidence by itself.

## Avoid counting external capacitors twice

The present models use ideal dynamic integrators with cutoff frequency supplied
directly in hertz. Physical OTA-like terminal models would instead produce
currents into external capacitors; those capacitors create the integration.
Adding the drawn capacitors around the existing dynamic integrators without
deriving a consistent terminal model would create extra poles incorrectly.

For a new circuit model, explicitly account for every stored state: which belongs
to a physical external capacitor and which, if any, is an assumed internal IC
state. A transconductance-plus-external-capacitor realization is one hypothesis,
not established IG02610/IG05630 internal documentation. Unequal capacitances do
not by themselves identify unequal pole frequencies or internal gains.

## Per-instrument work

### CS01

Declare the corrected C1/C1'/K0 network, the separate C2 22 nF/10 Mohm branch,
K0–KN 10k/switch, C3–KN VR11 rheostat, IN–C3 1k and C3 100 uF explicitly.
Keep the switch's physical contact state separate from the unverified High/Low
label mapping. VR11's trim position is a model calibration parameter, not a
known factory setting. Do not derive it from the current provisional Q values.

The Ic control network needs complete incoming source information and an
explicit current-to-transconductance hypothesis. The partial crop cannot fully
identify the EG/modulation source circuits. Start by describing this missing
boundary rather than claiming the current exponential frequency mapping is a
component-derived solution.

### CS01II

Declare all four 680 pF branches and both summing-node resistor networks. Include
8.2k to the bias reference in the AC node loading when that reference is assumed
stiff. Remove the automatic matched-conductance and unit-input-gain assumptions
from any claim of a component-derived model.

Declare LP–Vin 82k as a separate branch from LP–IN- 82k. To make the first branch
audibly meaningful, an explicit Vin-port model is required: its impedance and
its influence on internal states, including any VQ dependence. Do not select
that model merely because it recreates `1 + 3.2*resonance`. If multiple plausible
terminal hypotheses survive, report their differing responses instead of
silently selecting one as the identified hardware topology.

### Coupling and loads

The current production policy omits VCF coupling filters. Preserve that policy
while establishing the new circuit model; do not reintroduce the removed
empirical 20/8 Hz stages.

When modelling physical output coupling, include the actual capacitor and
connected loads as a separately assessed change. CS01's 1 uF/47k section has
the isolated 3.39 Hz result only with ideal source and no downstream load.
Source/load assumptions must be explicit, and the CS01II crop does not establish
the same 47k termination. Coordinate ownership with VCA modelling to avoid
counting a coupling capacitor or high-pass response twice.

## Incremental implementation and acceptance

1. **Topology record:** encode the known branches and an explicit list of
   unresolved ports/terminations. Verify node connectivity, capacitor units,
   switch variants and the two separate CS01II LP branches. An unresolved model
   must be marked incomplete, not reported as a solved circuit with omitted ports.
2. **Common numerical kernel:** validate isolated RC networks and a known
   controlled-source filter against independently derived responses. Check
   capacitor history, DC behavior, sample-rate convergence and switch events.
3. **Terminal-model prototypes:** supply documented IC/source/load hypotheses in
   an offline characterization harness. Test CS01 and CS01II using the same
   solver/API shape, retaining their different pin maps. Sweep cutoff, trim,
   resonance, input level and uncertain port parameters; report gain, phase,
   resonance peak, stability and cost. This is model comparison, not calibration.
4. **Evidence review:** distinguish effects caused by observed passive components
   from effects sensitive to assumed IC ports. Select a candidate explicitly;
   retain the unknowns in documentation. If measurements become available,
   fit uncertain IC parameters while keeping observed component values fixed.
5. **Production integration:** replace the aggregate-coefficient path only once
   the complete selected hypothesis runs stably at supported rates/factors and
   its cost and sound have been reviewed. Preserve host/state interfaces and
   highest-note/control timing. Document intentional sound changes.

The first implementation should be a bounded topology/kernel prototype with
an offline harness, not a generic SPICE engine or an immediate replacement of
the released audio path. The existing shared core supplies a numerical reference
for the old all-pole model during that work.
