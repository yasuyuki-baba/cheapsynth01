# CheapSynth01 Test Framework

This directory contains Google Test tests with a custom JUCE initialization environment.

## Test Structure

Tests are categorized as follows:

### Unit Tests (`unit/`)

Tests the functionality of individual components. Each class has a dedicated test class.

- **VCOProcessorTest** - Tests for the VCO processor functionality
- **ToneGeneratorTest** - Tests for sound generation functionality
- **OriginalVCFProcessorTest** - Tests for the CS-01 filter
- **ModernVCFProcessorTest** - Tests for the modern filter
- **VCAProcessorTest** - Tests for the VCA processor
- **EGProcessorTest** - Tests for the envelope generator
- **LFOProcessorTest** - Tests for the LFO processor
- **MidiProcessorTest** - Tests for MIDI processing
- **NoiseGeneratorTest** - Tests for the noise generator
- **CS01VCFCircuitTest** - Tests for the IG02610-inspired behavioral filter wrapper

### Integration Tests (`integration/`)

Tests the interaction between multiple components.

- **AudioGraphTest** - Tests for the complete audio graph functionality

### Mock Objects (`mocks/`)

Contains mock objects for testing.

- **MockOscillator** - Mock implementation of an oscillator
- **MockToneGenerator** - Mock implementation of sound generation
- **MockTimer** - Mock implementation of a timer

## How to Run Tests

The conditional EG decay algebra has a separate dependency-free Python check:

```bash
python3 Tests/eg_decay_equation_check.py -v
```

This is not invoked by the CMake runner. It checks the equations and an
invalid-operating-region example documented in `docs/EG-model-audit.md`, not
hardware fidelity or production DSP behavior.

To run the tests, execute the following command from the project's root directory:

```bash
./run_tests.sh
```

By default, the runner executes regression tests and excludes detailed DSP
observations named `Observation_*`. No tests are deleted or disabled.

```bash
./run_tests.sh --observations  # Detailed DSP observations only
./run_tests.sh --all           # Regression tests and observations
./run_tests.sh '--gtest_filter=EGTimingTest.*:EnvelopeRangeTest.*'
./run_tests.sh --group audio  # Core sound generation and DSP
./run_tests.sh --group midi  # MIDI and real-time control paths
./run_tests.sh --group ui  # Editor, panels, display, and parameter formatting
./run_tests.sh --group presets  # Preset and saved-state behavior
./run_tests.sh --group integration  # Whole-graph integration coverage
```

Functional groups are filters over the existing Google Test suites; they do
not move or duplicate test code. `audio` (also `audio-core`) covers oscillators,
filters, envelopes, amplifiers, modulation, and noise. `midi` (also `control`)
covers MIDI processing and graph-level MIDI controls. `ui` covers editor and
panel behavior, the audio display FIFO, and parameter formatting. `presets`
(also `state`) covers preset selection and persisted state. `integration`
covers the complete audio graph and graph-level routing, timing, and output
conversion. Groups may overlap where a suite crosses functional boundaries.
An explicit `--gtest_filter` passed with `--group` takes precedence.

The same options can be passed directly to the test executable. An explicit
`--gtest_filter` overrides the category default. `--gtest_output` is also respected.
Normal CI runs the regression category; run `--all` when changing DSP behavior or
when a complete characterization is needed. Observation tests still check basic
sanity (such as finite, non-silent output), but their printed spectral and timing
measurements are not hardware-calibrated regression thresholds. Nonlinear safety,
pitch, PWM timing, and graph consistency tests remain in the default category.

Detailed envelope duration checks remain sample-accurate in
`EGTimingTest.ProductionRangeStageDurations`. The graph integration check observes
the envelope in adaptive blocks of 1 to 64 host samples, preserving at least
16 observations across the shortest configured stage (subject to sample resolution).
Timing tolerances account for that observation interval.

The pitch/feet test retains all 180 combinations and its 0.1% frequency tolerance.
After 100 ms of settling, it stops once 64 complete periods have been measured,
with a four-second total limit to detect missing or incorrectly tuned output.

This script performs the following:

1. Creates a `build_tests` directory
2. Runs CMake to configure the project
3. Builds the tests
4. Runs the tests
5. Generates XML test results
6. Displays a test summary and results

### Continuous Integration

The unified `ci.yml` workflow runs on pushes to `main`, tags, pull requests,
and manual dispatches. After formatting passes, independent Linux, macOS, and
Windows jobs configure one build directory with all product formats and tests
enabled. Linux GUI tests run under Xvfb with Openbox; the runner waits for the
window manager to initialize before opening dialogs. A failed OS job does not
cancel the others.
Tag builds use Release and create a GitHub release only after all OS jobs pass.
The workflow:

1. Builds and runs tests on multiple platforms (Windows, macOS, Linux)
2. Generates XML test reports
3. Uploads test results as artifacts
4. Uploads product artifacts; successful tag builds also publish release packages

The current test status can be seen in the repository README badge or in the Actions tab on GitHub.

### Test Results Format

Test results are saved in JUnit compatible XML format to `build_tests/test_results.xml`. This format includes the following information:

- Test suite information
- Individual test cases
- Pass/fail status
- Error messages for failed tests
- Test execution time

#### XML output

`Tests/TestRunner.cpp` initializes Google Test and a JUCE GUI environment.
Google Test generates the XML report directly; there is no JUCE UnitTestRunner
conversion or custom report publisher. The default is `xml:test_results.xml`
in the current working directory. Override it with `--gtest_output=xml:PATH`.
CI runs from `build/` and uploads `build/test_results.xml` as an artifact.

## Test Implementation Guidelines

Follow these guidelines when adding new tests:

### 1. Test structure

Use Google Test's `TEST` for independent cases or `TEST_F` with a
`::testing::Test` fixture for shared setup and teardown. For example:

```cpp
#include <gtest/gtest.h>
#include "../../Source/CS01Synth/SynthConstants.h"

TEST(SynthConstantsTest, OversamplingFactorMatchesStages) {
    EXPECT_EQ(Constants::oversamplingFactor,
              std::size_t{1} << Constants::oversamplingStages);
}
```

Add each new test source to `Tests/CMakeLists.txt`. The shared runner already
initializes JUCE; do not add another `main()` or a static JUCE UnitTest instance.

### 2. Test Categories

Each test method should test a specific functional category:

- **Initialization Tests** - Verify that components are initialized correctly
- **Functionality Tests** - Verify that basic functionality works correctly
- **Edge Case Tests** - Verify that boundary values and exceptional cases are handled correctly
- **Performance Tests** - Verify that processing load and memory usage are within acceptable limits

### 3. Assertions

Use Google Test assertions:

- `EXPECT_TRUE` / `EXPECT_FALSE` for conditions
- `EXPECT_EQ` / `EXPECT_NE` for equality
- `EXPECT_LT` / `EXPECT_GT` for ordering
- `EXPECT_NEAR` for an absolute floating-point tolerance
- `ASSERT_*` when failure must stop the current test before continuing

### 4. Using Mock Objects

Tests may use mock objects instead of actual components. Mock objects are implemented in the `mocks/` directory.

```cpp
// Example of using a mock object in a test
testing::MockToneGenerator mockToneGenerator(apvts);
mockToneGenerator.prepare(spec);
mockToneGenerator.startNote(60, 1.0f, 8192);
float sample = mockToneGenerator.getNextSample();
```

## Test Coverage

Tests aim to cover the following areas:

1. **Core Components** - Classes central to sound generation
2. **Modulation-Related** - Classes responsible for timbre changes
3. **Input/Output Processing** - Classes responsible for handling input/output such as MIDI processing
4. **Integration Tests** - Tests to verify interaction between components

Tests for each component are implemented from the perspectives of initialization, basic functionality, edge cases, and performance.

## MIDI realtime regression coverage

`MidiProcessorTest` covers fixed-storage note tracking across all 128 keys,
duplicates, unmatched releases, priority/fallback, legato/velocity, reset commands,
every mapped controller and pitch bend. Timer tests check message-thread-only,
coalesced notification, current-state saving, edits during dispatch and safe
teardown. Expected notifications are awaited by pumping the message loop until
the expected count is reached, with a two-second monotonic timeout; this does
not assume a timer callback arrives within 100 ms on every CI platform.
`MidiRealtimeGraphTest` compares MIDI control changes with synchronous
parameter changes in both filter paths before any notification tick, and checks
session saving before dispatch. See [the control-flow design](../docs/MIDI-realtime-control.md).
