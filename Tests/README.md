# Regression tests

The active Google Test suite exercises the framework-independent C++ core.
Supported test environments are Windows and macOS; no JUCE or audio device is required.

```sh
cmake -S . -B build-core -DBUILD_PLUGINS=OFF -DBUILD_TESTING=ON
cmake --build build-core --config Debug --parallel 2
ctest --test-dir build-core -C Debug --output-on-failure
```

`./run_tests.sh` performs these steps. Pass ordinary Google Test arguments to
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
main baseline; they are not all covered by the active 38-test suite.
