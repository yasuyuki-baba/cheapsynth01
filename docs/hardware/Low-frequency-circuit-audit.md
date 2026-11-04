# CS-01 low-frequency model audit

Optional source provenance: [external source catalog](Source-catalog.md).

## Scope

This compares the recorded coupling components below with the software
stages. It does not require a drawing and is not hardware calibration.
Existing response tests validate numerical
behavior, not agreement with the instrument.

## Findings

- The recorded capacitor marking `1/50` denotes 1 uF / 50 V, not 0.02 uF.
- The former VCF input model used a second-order 20 Hz high-pass. The previous comment
  citing 0.022 uF and 22 kohm does not establish an audio-input coupling network.
- The former VCF output model added an empirical first-order 8 Hz high-pass.
  Both empirical VCF coupling stages have now been removed.
- The VCA input model adds a second-order 40 Hz high-pass and an additional
  second-order 20 Hz DC blocker. Neither cutoff is established by the recorded components.
- The 82 kohm resistor near TP3 is in the signal path; it must not simply be
  treated as a shunt load for the 1 uF coupling capacitor. The IC input impedance
  and source impedance are needed to establish the effective time constant.
- Buffer and line-output coupling are modeled with empirical first-order poles.
  Their sample-rate scaling is tested, but their hardware values remain uncalibrated.

For an isolated capacitor feeding a resistive load, tau = R_effective * C
and f_c = 1 / (2*pi*R_effective*C). R_effective depends on source and load
impedances. The recorded 82 kohm series resistor alone does not identify it;
therefore the 1 uF capacitor does not supply a numerical hardware cutoff here.

## Current code boundary

`CS01VCFCircuit` and `CS01IIVCFCircuit` both omit external audio-coupling filters.
Original's empirical 20 Hz input and 8 Hz output stages and their implementation
in `VCFCouplingStages.h` have been removed. This is an intentional sound change,
not a sound-preserving refactor: Original no longer has those extra low-frequency
attenuation and phase responses before and after the IC model.

The presence of coupling capacitors in the recorded drawings does not establish
that physical coupling is unity. The software omits their unresolved responses.
Resolve source/load impedances or measure low-frequency responses before assigning
hardware cutoff values. VCA and line-output coupling stages are unchanged.

## Verification limits

Empirical cutoff values are not hardware targets. Existing convergence,
coefficient-precision and cascade-consistency checks validate software behavior.
Source/load impedances and measured line-output responses are not established.
A single isolated passive RC section is first-order; the software second-order
stages are not a circuit-derived replacement on the available evidence.

The original audit did not change audio coefficients. The subsequent VCF coupling
removal described above does change the audio response.
