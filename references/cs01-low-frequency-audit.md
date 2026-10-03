# CS-01 low-frequency model audit

## Scope

Static comparison with the locally supplied CS-01 overall circuit diagram.
This is not hardware calibration. Existing response tests validate numerical
behavior, not agreement with the instrument.

## Findings

- The schematic's `1/50` denotes 1 uF / 50 V, not 0.02 uF.
- The VCF input model uses a second-order 20 Hz high-pass. The previous comment
  citing 0.022 uF and 22 kohm does not establish an audio-input coupling network.
- The VCF output model adds an empirical first-order 8 Hz high-pass.
- The VCA input model adds a second-order 40 Hz high-pass and an additional
  second-order 20 Hz DC blocker. Neither cutoff is established by the schematic.
- The 82 kohm resistor near TP3 is in the signal path; it must not simply be
  treated as a shunt load for the 1 uF coupling capacitor. The IC input impedance
  and source impedance are needed to establish the effective time constant.
- Buffer and line-output coupling are modeled with empirical first-order poles.
  Their sample-rate scaling is tested, but their hardware values remain uncalibrated.

## Verification limits and next steps

Do not introduce assertions that these empirical cutoffs are hardware targets.
Keep the existing convergence, coefficient-precision and cascade consistency
tests. Establish source/load impedances or obtain measured line-output responses
before replacing the low-frequency network. A single isolated passive RC section
is first-order; a second-order replacement requires additional justification.

No audio coefficients were changed during this audit.