# DSP responsibility boundaries

These production classes are provisional behavioral models, not verified internal
IC reconstructions. This naming/stage refactor preserves algorithms, arithmetic
order, constants, parameter mappings, routing, smoothing, and serialized state.

## Signal paths

```text
OriginalVCFProcessor (controls and modulation at the graph processing rate)
  -> CS01VCFCircuit (external signal-path wrapper)
     -> EmpiricalVCFInputCoupling (20 Hz second-order high-pass)
     -> IG02610BehavioralModel (nonlinear TPT state-variable filter + safety)
     -> EmpiricalVCFOutputCoupling (8 Hz first-order DC blocker)

ModernVCFProcessor (controls and modulation at the graph processing rate)
  -> CS01IIVCFCircuit (external boundary + cutoff safety)
     -> IG05630BehavioralModel (nonlinear four-pole behavioral filter + safety)
     -> unity external coupling (no additional stage)

VCAProcessor (controls, EG-depth smoothing, panel volume mapping)
  -> empiricalInputCoupling (40 Hz second-order high-pass)
  -> safetyDcBlocker (additional 20 Hz second-order high-pass)
  -> IG02600BehavioralModel (EG/breath gain composition + empirical saturation)
  -> EmpiricalVCACoupling (buffer-input coupling)
  -> Tr7EmpiricalBuffer (asymmetric gain + difference-based treble emphasis)
  -> EmpiricalVCACoupling (output coupling)
  -> safetyHighFreqRolloff (second-order low-pass)
```

Shared oversampling and output conversion belong to `CS01AudioProcessor`, which
prepares the graph at the internal rate; individual VCF processors do not own
oversampling stages. Original retains its empirical High/Low values 0.7f/0.2f
and the 0.5f selection threshold. Both processors retain empirical modulation
spans of 36 semitones for EG and 24 for LFO/breath.

The wrapper names `CS01VCFCircuit` and `CS01IIVCFCircuit` remain. Their names
identify the external circuit boundary, not a claim that the IC internals or
surrounding component transfers have been reconstructed. No Modern coupling or
new safety stage is invented where the existing path is unity.

## Evidence and terminology

- **Circuit-informed:** external audio/control connections and stage order guide
  the wrappers and processors. Capacitor labels such as `1/50` mean 1 uF / 50 V;
  they do not establish effective loading or a calibrated cutoff.
- **BehavioralModel:** `IG02610BehavioralModel` owns the two-state nonlinear TPT
  behavior. `IG05630BehavioralModel` owns the two-section four-pole TPT cascade
  with one-sample resonant feedback; its internal topology is a software choice.
  `IG02600BehavioralModel` owns the provisional normalized EG/breath gain and
  saturation. These names do not assert internal IC topology knowledge.
- **Empirical:** uncalibrated damping/Q, drive, resonance curves, coupling
  frequencies/poles, Tr7 gains/treble emphasis, and VCA saturation constants are
  explicit in the corresponding stages or `EmpiricalParameters`. VCA coupling
  poles retain 0.997f and 0.9995f at 44.1 kHz with the original sample-rate
  conversion. Tr7 retains gains 0.95f/0.92f and difference factors 0.998f/2.0f,
  including their existing rate dependence. The volume exponent remains 2.5f.
- **Safety:** `SafetyParameters::maximumOutput` is 1.5f in both VCF models.
  Finite-input/output handling, Modern state recovery, cutoff/coefficient bounds,
  and the VCA's additional DC blocker and rolloff are implementation protection,
  not verified hardware transfer characteristics. The VCA rolloff remains
  min(15000 Hz, sampleRate * 0.45f). These stages can affect sound; labeling them
  as safety does not justify removing or changing them.

Safety that depends on model states remains next to the state update. In
particular, Modern resets its integrators on nonfinite output and keeps the
unlimited output in its feedback state before limiting the returned sample.
Original retains its existing finite-output substitution without resetting its
integrators. Moving those guards into a generic wrapper would change semantics.

## Preservation checks

`DspStructureRegressionTest` compares the production Original and Modern circuit
paths and VCA processor against frozen pre-refactor production implementations in
`Tests/reference/DspBeforeRefactor.*`. These files are test-only oracles; keep
their arithmetic unchanged rather than updating them along with production DSP.

Comparisons use exact float bits (including signed zero), with no ULP tolerance.
Coverage includes 44.1/48/96/192 kHz, impulse, DC, low and driven input, silence,
cutoff/resonance modulation, both Original block entry points, VCF reset, VCA
release/reprepare, live controls and block sizes 1/7/64/256. The same compiler and
build settings compile both implementations; this is a same-build refactor
check, not a promise of binary identity across compilers or platforms.

The audio group includes these checks and the renamed behavioral-model suites.
Existing processor, graph, state and observation tests continue to cover their
original responsibilities.

## Refactor validation result

Validated on 2026-10-07 with the Debug GCC build. The test target rebuilt
successfully. Group filters were taken from `run_tests.sh` and run against the
rebuilt executable through the existing Xvfb/Openbox GUI harness.

| Check | Passed | Failures |
| --- | ---: | ---: |
| Exact pre-refactor DSP comparisons | 4 | 0 |
| Audio group | 133 | 0 |
| Integration group | 25 | 0 |
| Full regression suite | 201 | 0 |
| Observations | 16 | 0 |

The groups overlap; their counts are not additive. Reports are saved locally as
`build/dsp-refactor-{structural,audio,integration,regression,observations}.xml`.
Changed C++ files pass clang-format 21.1.7 checks, and `git diff --check` passes.

Audio samples in the exact checks were bit-identical to the pre-refactor
implementations (395,264 float comparisons; zero tolerance). No audio change was
introduced or observed in this refactor. Parameter IDs, mappings, routing and
serialized state formats were retained; the existing state and graph regressions
also pass. Observation timings remain software measurements, not hardware
calibration or real-time performance guarantees.
