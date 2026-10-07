# CS-01 Circuit Model: Unresolved Details

This document identifies details not yet established from circuit evidence,
current approximations, and replacement targets. It does not claim accurate
reproduction of unknown circuits. The scope is the CS-01 model, not the Modern
filter or MIDI extensions.

## 1. Details that depend on custom IC internals

| Target | Known external configuration | Unresolved internals | Current approximation / impact | Main implementation |
| --- | --- | --- | --- | --- |
| YM10150: waveform generation | Waveform/feet selection and sound-generator connections | Generation method, amplitude, harmonics, output impedance | BLEP, integration, custom waveform corrections, and tanh shaping affect waveform and timbre | `Source/CS01Synth/ToneGenerator.cpp`, `Source/CS01Synth/WaveformStrategies.h` |
| YM10150: keyboard and gate | Keyboard inputs and EG gate path | Key priority, retrigger conditions, transition state | Highest-note priority and legato gate retention; consistency tests do not prove hardware behavior | `Source/CS01Synth/MidiProcessor.cpp` |
| YM10150: glissando | GLS connections to 22 kohm, A1M, and 0.022 uF components transcribed | Oscillation thresholds, division, resistance-to-semitone timing, phase during adjustment | Maximum 208 ms per semitone with a provisional control curve; changes preserve fractional step progress | `Source/CS01Synth/ToneGenerator.cpp` |
| IG02610: VCF | External control/audio paths and High/Low selection | Internal topology, control-to-cutoff relationship, loading, resonance, distortion | TPT state-variable behavioral core with nonlinear feedback; damping/drive values and modulation depth are provisional | `Source/CS01Synth/ExperimentalOriginalVCF.h`, `Source/CS01Synth/CS01VCFCircuit.cpp`, `Source/CS01Synth/OriginalVCFProcessor.cpp` and `.h` |
| IG02600: VCA | External audio, EG, and breath paths | Control-voltage-to-gain relationship, input combination, saturation, loading | Multiplied EG/breath gains and custom saturation; intermediate curves remain unresolved | `Source/CS01Synth/VCAProcessor.cpp` |

Implementation paths are relative to the repository root. Pinouts alone do not
establish internal transfer characteristics.

## 2. Unresolved details beyond custom ICs

| Target | Unresolved details / current treatment | Detailed record |
| --- | --- | --- |
| EG stage switching | Local TC7476BP truth table and asynchronous reset/feedback wiring mapped; conditional attack/decay/release sequence identified. Actual input thresholds, loaded logic levels and transient behavior remain unresolved | [EG audit](EG-model-audit.md), [TC7476BP investigation](TC7476BP-online-investigation.md) |
| EG charging/discharging | 2.2 uF and resistor branches transcribed; conduction states, effective loading, terminal voltages, buffer transfer unresolved; production uses a regression-tested stateful exponential envelope with provisional, uncalibrated curvature | [EG audit](EG-model-audit.md) |
| EG timing/control curves | 1 ms–2 s is provisional; manual S–L markings do not specify seconds; generic RC tests do not prove hardware agreement | [Timing validation](EG-time-range-validation.md) |
| Coupling/low-frequency response | IC input impedance and effective loads unresolved; DC-removal/coupling cutoffs uncalibrated | VCF/VCA implementation comments |
| Buffers/distortion | Transistor/FET operating points, asymmetry, saturation uncalibrated; custom corrections are not device models | `Source/CS01Synth/VCAProcessor.cpp`, `Source/CS01Synth/WaveformStrategies.h` |
| Sliders | A-taper labels do not establish exact position-to-resistance curves; software skew is provisional | [Glissando audit](Glissando-circuit-audit.md), [Timing validation](EG-time-range-validation.md) |

## 3. Distinguish hardware unknowns from software properties

- Sample-rate handling, numerical precision, block partitioning, MIDI event
  positions, and release-state preservation can be verified as software properties.
- Global 4x oversampling mitigates digital aliasing; it does not model custom IC internals.
- Regression success does not guarantee that uncalibrated values match hardware.
- Missing internal specifications do not invalidate established external wiring,
  component values, or manufacturer operating instructions. Retain that evidence.

## 4. Update sequence when evidence becomes available

1. Record exact part number, source, and page; map the evidence to external wiring.
2. Identify which unresolved item it resolves.
3. Define control laws, units, and timing first; test independent expected values.
4. Replace approximations and check presets, performance state, and timbral effects.

See [Requested resources](Requested-circuit-resources.md) for evidence needs.
Creating this inventory did not change production audio processing.

## 5. External-circuit audit closeout

See [external-circuit closeout](External-circuit-closeout.md) for completed
analysis, reproducible verification and the remaining evidence required before
production changes. Audit completion is not hardware-model completion.
