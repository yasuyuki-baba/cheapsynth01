# Regression tests

The active Google Test suite exercises the framework-independent C++ core.
Supported test environments are Windows and macOS; no audio device is required.

```sh
cmake -S . -B build-core -DBUILD_PLUGINS=OFF -DBUILD_TESTING=ON
cmake --build build-core --config Debug --parallel 2
ctest --test-dir build-core -C Debug --output-on-failure
```

`./run_tests.sh` performs these steps and writes a JUnit report.
Use `./run_tests.sh --group presets` (also `audio`, `dsp`, `core`, `numerical`,
`midi`, `integration`, `observation`) to select a CTest label. Native UI tests
remain pending; `--group ui` reports this explicitly rather than passing an empty
suite. Run `ctest --test-dir build-core -C Debug -L regression` for correctness
assertions and `-L observation` for characterization cases. Pass ordinary Google Test arguments to
select cases, e.g. `./run_tests.sh --gtest_filter='StateTest.*'`.

Coverage includes MIDI note priority, controller reset and panic, 14-bit CCs,
source switching during held/releasing notes, event offsets and block partition
independence, re-preparation, sample-rate/filter/waveform safety, allocation-free
core rendering, FIR delay/gain/rejection, XML factory/user/session state and
truncated XML rejection. Two independent filter model suites check numerical behavior.

Native editor interaction, DAW automation, plugin scanning, AU validation and
host latency compensation still need testing on Windows/macOS.

## Current coverage

The portable suite has 209 cases: 195 correctness regressions (including three
research equation cases) and 14 observational cases. Native product builds add
eight VST3 boundary cases and three parameter-presentation cases, for 220 total.
Use CTest discovery for the current count. See the
[native validation status](../../docs/iPlug2-migration-plan.md) for remaining host,
editor and device checks.

`unit/EnvelopeContinuityTest.cpp` exercises residual EG trajectories, live timing,
free-running source continuity and release/source switching. Preset assertions
cover same-name overwrite, failed replacement and missing-selected-file fallback.
`unit/PluginIdentityTest.cpp` guards plugin IDs and parameter index order.

Host-selection core assertions cover latest-valid requests, repeated selection,
cached user sound, malformed request chunks, pending-session identity, held-note
output equivalence, concurrent panel refresh and instance isolation. The request
and boundary-apply allocation probe runs 4096 pairs. This does not instrument
SDK callbacks or prove native host/UI thread safety. Whole-patch publication
now has concurrent regression assertions in `unit/PatchPublicationTest.cpp`.

Native product builds also run eight `vst3-boundary` tests against real SDK
interface types using simulated queues/streams. They do not load a plugin binary
or open an editor/device. Core-only builds remain independent of the SDK; use
`-DBUILD_VST3_BOUNDARY_TESTS=ON` to opt in with prepared SDK headers.


`unit/KeyboardInputTest.cpp` and the allocation probe cover UI/external note
ownership, per-instance PC input and cleanup. `unit/DspRegressionTest.cpp` adds
LFO/filter/noise/pitch regressions and independent empirical-stage arithmetic
references. Four NumericalRecoveryTest regressions cover VCA/filter nonfinite
input recovery, modulation fallback and usable smoothing after rejected controls.
MidiQueue cases guard stable clamping, future-event carry and empty
callback boundary processing. Native callback behavior requires verification in actual products.

Archive contract tests run independently, without loading a product:

```sh
python -m unittest discover -s CheapSynth01/tests/packaging -v
```

Fifteen independently constructed fixtures check file hashes, exact payload
coverage, required content, release identity, modes, safe ZIP/symlink paths and
opt-in signing command preparation.
Native compilation and source-archive offline rebuild run in CI. Actual plugin
loading, DAW/device and interactive UI verification are deferred.

`unit/MidiBehaviorTest.cpp` uses an independent
recording generator, a std::set oracle for all 128 keys and actual core audio for
velocity invariance across all waveforms. It covers every controller's production
range and fine/coarse pair, CC121 patch preservation, CC120/123 and held EG gates.
Additional EG continuity regressions retain live attack bounds, residual release
targets after edits, sustain retrigger behavior and lifecycle observation reset.
Native listener/thread/teardown behavior requires actual iPlug2 runtime checks.

`unit/VcoBehaviorTest.cpp` retains glissando timing against an independent pitch
schedule, PWM periods from carrier-cycle means, live held-wheel range changes,
measured bend/fine-pitch/LFO offsets and empirical decay-per-second constants.
Source tests check held and released tone/noise transitions and exact remaining
release time, including unmatched key release and new-note recovery.
Live waveform/Feet choices preserve held note identity and finite nonzero audio.
Upstream sources intentionally run continuously; engine/VCA-gate silence is
separate from source note-state inactivity.

`unit/PresetBehaviorTest.cpp` checks all 18 embedded factory controls against
resource XML after perturbation, independent manual-panel readings, all 39 audible
factory sounds, user-file/session exclusions and sorted catalogue identity.
Percussion checks cover held-note decay and the longer open-hi-hat tail; sequence
checks render an eight-step sixteenth-note pattern and verify retriggering and silence.
Breath Lead checks cover audible output at zero breath and response to a live CC2
change during a held note.
Selection and rename preserve edited sound without reloading the preset file.
Seven filter-model regressions check numerical behavior, with three separate observations.
Observations characterize this build and are not native-host performance gates.

## Signal-path and timeline checks

`unit/SignalPathTest.cpp` includes frozen model bit references, VCA analytical
transfer/precision, Original modulation independence, FIR folded-bin controls and
settled response observations. `unit/EngineTimelineTest.cpp` checks explicit
connected routes, source/release/glissando timelines, MIDI boundaries/overflow,
breath endpoints and concurrent choice publication. Core allocation probes now
count C++ allocations and frees on first/variable/dense queue+engine+UI-bridge
processing; they do not intercept arbitrary SDK malloc or locks.

`unit/ToneMeasurementTest.cpp` retains known-signal controls and production/strategy
spectrum/onset measurements. `unit/OnsetObservationTest.cpp` always renders and
checks repeated notes/stage transients. Set `CHEAPSYNTH_ONSET_OUTPUT` to an output
directory to export five mono PCM24 WAVs (48kHz) and five stage CSVs; filenames
start with `iplug2-`. For example:

```sh
CHEAPSYNTH_ONSET_OUTPUT=/tmp/cheapsynth-onsets ./run_tests.sh --gtest_filter='OnsetObservationTest.*'
python -m unittest discover -s CheapSynth01/tests/research -v
```

Research checks retain four conditional circuit equation cases; the three C++ RC
cases have the `research` CTest label. These hypothetical networks do not establish
production envelope calibration or operating-region validity.
