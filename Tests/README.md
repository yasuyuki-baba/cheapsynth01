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
legacy JUCE XML chunk decoding. The two independent filter model suites retain
their original numerical assertions.

`legacy-juce/` contains historical scenarios that are not part of this suite.
Native editor interaction, DAW automation, plugin scanning, AU validation and
host latency compensation still need testing on Windows/macOS.

## Completion plan

See the [migration completion plan](../docs/iPlug2-migration-plan.md) for the audit of newer main,
remaining behavior/compatibility gaps, iPlug2OOS alignment and validation gates.
The linked case inventory tracks the 272 Google Test definitions in the frozen
main baseline; their assertion coverage is tracked individually in that inventory.

The current suite has 72 active core cases: 69 correctness regressions and
three observational cases. `unit/EnvelopeContinuityTest.cpp` ports newer main's
residual EG trajectory, live timing and gate assertions and exercises free-running
source continuity and extended release/source switching. Preset assertions cover
same-name overwrite, failed replacement and missing-selected-file fallback.
`unit/PluginIdentityTest.cpp` guards retained plugin IDs and existing parameter
index order and the VST3 legacy-ID mapping contract; native old-project
compatibility still requires host fixtures.

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
