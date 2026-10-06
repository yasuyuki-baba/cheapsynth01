# Modern VCF / CS01II model

## Local primary sources

- `CS01 II Sythesizer.pdf`, page 6, VCF schematic.
- `../Yamaha-CS-01-Overall-Circuit-Diagram.pdf`, VCF schematic.

The CS01 drawing identifies IC2 as IG02610; the CS01II drawing identifies
IC2 as IG05630. The CS01 drawing exposes C1/C2
capacitor connections and takes the signal from C3 (pin 14). The CS01II
drawing exposes C1/C2/C3/C4 capacitor connections. This supports changing
Modern's order rather than replacing Original's processing or controls.

## Existing model and hardware evidence

The external service schematics identify the IC and show four external timing
capacitor connections on CS01II, while the product's VCF signal path uses the
low-pass output. They do not establish the IC's internal OTA arrangement,
internal BP/HP pins, nonlinear transfer, or the exact resonance law. The
user-supplied report proposes a cascade of two second-order SVFs, exponential
cutoff control, and nonlinear resonant feedback; this is now the design basis
for an opt-in experimental model, with each unsupported detail labeled as a
behavioral hypothesis.

`IG05630` remains the legacy model and default. It uses two self-implemented
TPT lowpass sections. At zero resonance their Q values are 0.5411961 and
1.3065630: a fourth-order Butterworth approximation with unity DC gain and
nominal -3 dB at cutoff. Its resonance map and output-only coloration remain
empirical.

## Experimental report-based model

Select `ModernVCFProcessor::Model::Experimental` for code-level A/B comparison,
or use the temporary Modern toggle beside the keyboard/monitor toggle in the
editor. The default remains `Legacy`; this UI choice is not saved in presets
or plugin state. The Original VCF has an independent toggle in the same place.
`ExperimentalIG05630` cascades two TPT second-order
SVF low-pass sections using the Butterworth Q pair, then feeds the final output
back to the input summing point. A one-sample feedback state makes the loop
deterministic and stable under modulation, but it is not a zero-delay solution
or a claim about the IC's internal loop. The resonance control maps linearly
to a provisional maximum feedback gain of 3.0, chosen to allow bounded
self-sustained oscillation in the behavioral model.

Smooth tanh shaping is used at the input summing point and in resonant
feedback. The named input/feedback drives and maximum output clamp are
provisional. This reflects the report's nonlinear-feedback proposal and
resonance-drop behavior without asserting that the IC has an internal VCA or
these exact nonlinear cells. The cutoff input is recalculated every sample;
the processor's exponential semitone modulation is an empirical frequency
mapping, not a calibrated IG05630 CV law. No BP/HP outputs are exposed because
the reviewed CS01II product schematic only establishes use of the LP path.

## DSP approximation, not a transistor-level reconstruction

The IC's internal topology and exact transfer function are not established by
the external schematics. Butterworth alignment and all experimental damping,
feedback, drive, and safety values are behavioral choices, not measured
hardware characteristics. No asymmetric DC bias is introduced because the
report provides no evidence for its direction or amount. Existing semitone
control depths are also empirical.

Further calibration requires internal IC documentation or hardware response
measurements, particularly for resonance and pole alignment.

## Experimental validation and recommendation

The targeted Debug and Release runs passed 18 tests across the Modern,
experimental IG05630, circuit wrapper, and legacy IG05630 suites. Coverage
includes four-pole response, finite/bounded output during fast cutoff and
resonance changes at 44.1/48/96 kHz host rates (4x internal rate), deterministic
block partitioning, bounded self-sustained resonance after an impulse, and
selection of the new model while preserving Legacy as the default. These are
software properties, not hardware validation.

Compiled A/B response, harmonic, and CPU observations are saved in
`artifacts/dsp/ig05630_cpp_response.csv`,
`artifacts/dsp/ig05630_cpp_harmonics.csv`, and
`artifacts/dsp/ig05630_cpp_benchmark.csv`. At 48 kHz host rate / 192 kHz
internal rate, the experimental model shows a strong resonance peak and
low-frequency resonance drop, consistent with the supplied report's
behavioral target. At 1 kHz cutoff and maximum resonance, the projected gain
was about +15.5 dB at cutoff and -16.4 dB at 500 Hz; the legacy model measured
about +9.0 dB and +3.6 dB at those points. This is a model-to-model comparison,
not evidence that the CS01II has those exact values.

At 4x internal rate with static/modulated cutoff, the median core cost on this
machine was 205.9 ns/sample Debug and 60.7 ns/sample Release for Experimental,
versus 177.6 and 51.4 ns/sample for Legacy. The increase is about 16% Debug
and 18% Release. This moderate cost is the trade-off for explicit nonlinear
resonant feedback and self-oscillation behavior. Timings exclude the rest of
the plugin graph and are machine-specific.

Keep Experimental available for A/B work, but keep Legacy as the default until
listening comparisons and, if possible, CS01II hardware captures support a
change. No circuit-accuracy or sound-match claim is made.
## Code boundaries

`ModernVCFProcessor` handles buses, parameters, modulation, and model A/B
selection; `CS01IIVCFCircuit` owns cutoff safety limits and dispatches between
models; `IG05630` holds the legacy approximation;
`ExperimentalIG05630` owns the report-based TPT cascade, feedback, nonlinear
behavior, and named empirical parameters.
This mirrors Original's processor/circuit/IC-model separation without changing
Original. External coupling remains unity to preserve the existing Modern sound.
The IC name identifies the hardware component and the future update boundary,
not the fidelity of either model. No behavior in this experimental model has
been validated against a physical CS01II or IG05630.

Both model cores have no JUCE dependency: coefficients and trapezoidal
integrator states are implemented locally. JUCE filters remain only as a test
reference for the previous Modern response, not as production filter
components.
