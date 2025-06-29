# CheapSynth01

[![Tests](https://github.com/username/cheapsynth01/actions/workflows/tests.yml/badge.svg)](https://github.com/username/cheapsynth01/actions/workflows/tests.yml)

CheapSynth01 is a software emulation of an early 80s compact monophonic synthesizer. Developed using the JUCE framework, it recreates classic analog sounds with precision.

## Features

- Faithful circuit emulation

## Supported Platforms

- Windows
- macOS
- Linux

## Supported Plugin Formats

- Standalone application
- VST3
- AU (macOS only)
- LV2 (macOS and Linux only)

## Build Instructions

### Requirements

- CMake 3.15 or higher
- C++20 compatible compiler
- JUCE (automatically fetched by CMake)

### Build Steps

```bash
# Create and configure build directory
cmake -B build

# Build the project
cmake --build build
```

### Running Tests

To run the tests locally:

```bash
./run_tests.sh
```

This will build and run all tests, generating a detailed test report in XML format.

## Usage

Load the plugin in your DAW or launch the standalone application.

## Development

### Continuous Integration

This project uses GitHub Actions for continuous integration. On each push and pull request to any branch, the following actions are performed:

- Building the project on multiple platforms (Windows, macOS, Linux)
- Running all tests
- Generating and publishing test reports

You can view the latest test results in the Actions tab of the GitHub repository.

## License

This project is released under the MIT License. See the LICENSE file for details.
