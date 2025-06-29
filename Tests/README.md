# CheapSynth01 Test Framework

This directory contains the test code for the YAMAHA CS-01 emulation project. Tests are implemented using JUCE's built-in unit testing framework.

## Test Structure

Tests are categorized as follows:

### Unit Tests (`unit/`)

Tests the functionality of individual components. Each class has a dedicated test class.

- **VCOProcessorTest** - Tests for the VCO processor functionality
- **ToneGeneratorTest** - Tests for sound generation functionality
- **CS01VCFProcessorTest** - Tests for the CS-01 filter
- **ModernVCFProcessorTest** - Tests for the modern filter
- **VCAProcessorTest** - Tests for the VCA processor
- **EGProcessorTest** - Tests for the envelope generator
- **LFOProcessorTest** - Tests for the LFO processor
- **MidiProcessorTest** - Tests for MIDI processing
- **NoiseProcessorTest** - Tests for the noise generator
- **IG02610LPFTest** - Tests for the IG02610 filter

### Integration Tests (`integration/`)

Tests the interaction between multiple components.

- **AudioGraphTest** - Tests for the complete audio graph functionality

### Mock Objects (`mocks/`)

Contains mock objects for testing.

- **MockOscillator** - Mock implementation of an oscillator
- **MockToneGenerator** - Mock implementation of sound generation
- **MockTimer** - Mock implementation of a timer

## How to Run Tests

To run the tests, execute the following command from the project's root directory:

```bash
./run_tests.sh
```

This script performs the following:

1. Creates a `build_tests` directory
2. Runs CMake to configure the project
3. Builds the tests
4. Runs the tests
5. Generates XML test results
6. Displays a test summary and results

### Continuous Integration

Tests are automatically run via GitHub Actions whenever code is pushed to any branch in the repository or a pull request is created. The workflow:

1. Builds and runs tests on multiple platforms (Windows, macOS, Linux)
2. Generates XML test reports
3. Uploads test results as artifacts
4. Publishes test results to the GitHub Actions interface

The current test status can be seen in the repository README badge or in the Actions tab on GitHub.

### Test Results Format

Test results are saved in JUnit-compatible XML format in the `build_tests/test_results.xml` file. This format includes:

- Test suite information
- Individual test cases
- Pass/fail status
- Error messages for failed tests
- Execution time for tests

## Test Implementation Guidelines

Follow these guidelines when adding new tests:

### 1. Test Class Structure

```cpp
class MyComponentTest : public juce::UnitTest
{
public:
    MyComponentTest() : juce::UnitTest("MyComponent Tests") {}
    
    void runTest() override
    {
        testInitialization();
        testFeature1();
        testFeature2();
        // Other test methods
    }
    
private:
    void testInitialization()
    {
        beginTest("Initialization Test");
        
        // Test code
        
        expect(condition, "Error message");
    }
    
    // Other test methods
};

static MyComponentTest myComponentTest;
```

### 2. Test Categories

Each test method should test a specific functional category:

- **Initialization Tests** - Verify that components are initialized correctly
- **Functionality Tests** - Verify that basic functionality works correctly
- **Edge Case Tests** - Verify that boundary values and exceptional cases are handled correctly
- **Performance Tests** - Verify that processing load and memory usage are within acceptable limits

### 3. Assertions

JUCE's unit testing framework provides the following assertions:

- `expect(condition, message)` - Verify that a condition is true
- `expectEquals(a, b, message)` - Verify that two values are equal
- `expectNotEquals(a, b, message)` - Verify that two values are not equal
- `expectLessThan(a, b, message)` - Verify that a is less than b
- `expectGreaterThan(a, b, message)` - Verify that a is greater than b
- `expectWithinAbsoluteError(a, b, error, message)` - Verify that the difference between a and b is less than or equal to error

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
