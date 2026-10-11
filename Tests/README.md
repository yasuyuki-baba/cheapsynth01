# Regression tests

The active Google Test suite exercises the framework-independent C++ core.
Supported test environments are Windows and macOS; no JUCE or audio device is required.

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
truncated XML rejection. The two independent filter model suites retain
their original numerical assertions.

`legacy-juce/` contains historical scenarios that are not part of this suite.
Native editor interaction, DAW automation, plugin scanning, AU validation and
host latency compensation still need testing on Windows/macOS.

## Completion plan

See the [migration completion plan](../docs/iPlug2-migration-plan.md) for the audit of newer main,
remaining behavior and validation gaps, iPlug2OOS alignment and validation gates.
The linked case inventory tracks the 272 Google Test definitions in the frozen
main baseline; their assertion coverage is tracked individually in that inventory.

The current suite has 135 active core cases: 132 correctness regressions and
three observational cases. `unit/EnvelopeContinuityTest.cpp` ports newer main's
residual EG trajectory, live timing and gate assertions and exercises free-running
source continuity and extended release/source switching. Preset assertions cover
same-name overwrite, failed replacement and missing-selected-file fallback.
`unit/PluginIdentityTest.cpp` guards retained plugin IDs and existing parameter
index order. JUCE project/automation/binary-state compatibility is out of scope;
its ID-mapping assertions have been removed.

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


The complete audited main Tests snapshot and source references are under
`legacy-juce` (272 historical definitions), excluded from active builds.
`unit/KeyboardInputTest.cpp` and the allocation probe cover UI/external note
ownership, per-instance PC input and cleanup. `unit/DspRegressionTest.cpp` adds
LFO/filter/noise/pitch regressions and independent empirical-stage arithmetic
references. Four NumericalRecoveryTest regressions cover VCA/filter nonfinite
input recovery, modulation fallback and usable smoothing after rejected controls.
MidiQueue cases guard stable clamping, future-event carry and empty
callback boundary processing. There are 143 active CTest cases with SDK-interface
tests enabled; this count does not claim historical assertion parity.

Archive contract tests run independently, without loading a product:

```sh
python -m unittest discover -s Tests/packaging -v
```

Eleven independently constructed fixtures check file hashes, exact payload
coverage, required content, release identity, modes and safe ZIP/symlink paths.
Native compilation and source-archive offline rebuild run in CI. Actual plugin
loading, DAW/device and interactive UI verification are deferred.

`unit/MidiBehaviorTest.cpp` replaces historical MIDI assertions with an independent
recording generator, a std::set oracle for all 128 keys and actual core audio for
velocity invariance across all waveforms. It covers every controller's production
range and fine/coarse pair, CC121 patch preservation, CC120/123 and held EG gates.
Additional EG continuity regressions retain live attack bounds, residual release
targets after edits, sustain retrigger behavior and lifecycle observation reset.
JUCE listener/thread/teardown assumptions do not establish iPlug2 UI correctness;
those native scenarios remain explicitly deferred in the assertion inventory.

`unit/VcoBehaviorTest.cpp` retains glissando timing against an independent pitch
schedule, PWM periods from carrier-cycle means, live held-wheel range changes,
measured bend/fine-pitch/LFO offsets and empirical decay-per-second constants.
Source tests check held and released tone/noise transitions and exact remaining
release time, including unmatched key release and new-note recovery.
Live waveform/Feet choices preserve held note identity and finite nonzero audio.
Upstream sources intentionally run continuously; engine/VCA-gate silence is
separate from source note-state inactivity and archived JUCE block APIs.

`unit/PresetBehaviorTest.cpp` checks all 18 embedded factory controls against
resource XML after perturbation, independent manual-panel readings, all 7 audible
factory sounds, user-file/session exclusions and sorted catalogue identity.
Selection and rename preserve edited sound without reloading the preset file.
Seven filter-model regressions and three separate observations were diffed
against the frozen main reference; counts and numerical tolerances are retained.
Observations characterize this build and are not native-host performance gates.
