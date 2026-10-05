# Modern VCF / CS01II model

## Local primary sources

- `CS01 II Sythesizer.pdf`, page 6, VCF schematic.
- `../Yamaha-CS-01-Overall-Circuit-Diagram.pdf`, VCF schematic.

The CS01 drawing identifies IC2 as IG02610; the CS01II drawing identifies
IC2 as IG05630. The CS01 drawing exposes C1/C2
capacitor connections and takes the signal from C3 (pin 14). The CS01II
drawing exposes C1/C2/C3/C4 capacitor connections. This supports changing
Modern's order rather than replacing Original's processing or controls.

## DSP approximation, not a transistor-level reconstruction

Modern uses two self-implemented TPT lowpass sections. At zero resonance their Q values
are 0.5411961 and 1.3065630: a fourth-order Butterworth approximation with
unity DC gain and nominal -3 dB at cutoff. Original remains unchanged.

The IC's internal topology and exact transfer function are not established
by these external schematics. Butterworth alignment is a modelling choice,
not a measured or schematic-proven hardware response. Resonance scales the
first section's Q by 1..4; this is empirical and does not reproduce global
feedback or self-oscillation. No hardware-calibrated saturation is claimed.
Gentle output-only tanh coloration blends 8..30% with the linear output;
drive increases smoothly with resonance and a sample-rate-adjusted input
envelope. Small-signal gain remains unity. This is an empirical voicing choice
inspired by Original, not evidence of common internal IC circuitry. There is
no duplicated per-stage distortion or added asymmetric DC bias.
Existing semitone control depths are also empirical.

Further calibration requires internal IC documentation or hardware response
measurements, particularly for resonance and pole alignment.
## Code boundaries

`ModernVCFProcessor` handles buses, parameters and modulation;
`CS01IIVCFCircuit` owns the circuit boundary and cutoff safety limits;
`IG05630` owns the two TPT sections and empirical resonance mapping.
This mirrors Original's processor/circuit/IC-model separation without changing
Original. External coupling remains unity to preserve the existing Modern sound.
The IC name identifies the hardware component and the future update boundary,
not the fidelity of the model. Both `IG02610` (Original) and `IG05630` (Modern)
are provisional IC models; their approximations are documented separately.

`IG05630` has no JUCE dependency: coefficients and trapezoidal integrator
states are implemented locally. JUCE filters remain only as a test reference
for the previous Modern response, not as production filter components.
