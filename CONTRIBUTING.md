# Contributing to CheapSynth01

Thank you for your interest in contributing to CheapSynth01! This document provides guidelines and instructions for contributing to this project.

## Code of Conduct

Please help us keep CheapSynth01 open and inclusive. Be respectful to others, welcome newcomers, and maintain a positive environment for collaboration.

## How to Contribute

There are many ways to contribute to CheapSynth01:

### Reporting Bugs

If you find a bug, please submit an issue using our bug report template. Include as much information as possible:
- Clear description of the issue
- Steps to reproduce
- Expected vs. actual behavior
- Environment details (OS, DAW, plugin version)
- Screenshots or audio samples if applicable

### Suggesting Features

Feature requests are welcome! Use our feature request template to submit your ideas.

### Sound Comparison Testing

**We particularly need help with sound comparison testing against real hardware.** If you own an original synthesizer, your contribution is especially valuable. Please use our dedicated sound comparison template when submitting feedback.

### Code Contributions

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes following our guidelines
4. Push to your branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Development Setup

### Requirements

- CMake 3.15 or higher
- C++20 compatible compiler
- JUCE (automatically fetched by CMake)

### Build Instructions

#### First Time Setup

If you've cloned the repository, initialize the submodules:

```bash
git submodule update --init --recursive
```

#### Development Build (Fast)

For development, we recommend using the `-DSTANDALONE_ONLY=ON` option to reduce build times:

```bash
# Create and configure build directory (standalone only)
cmake -B build -DSTANDALONE_ONLY=ON

# Build the project
cmake --build build
```

This builds only the standalone application, which is sufficient for most development and testing.

#### Release Build (All Formats)

To build all plugin formats including CLAP:

```bash
# Create and configure build directory (all formats)
cmake -B build -DSTANDALONE_ONLY=OFF -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build build
```

For multi-configuration generators, build with `cmake --build build --config Release`.

This builds all supported plugin formats:
- Standalone application
- VST3
- AU (macOS only)
- LV2 (macOS and Linux only)
- CLAP (CLever Audio Plugin)

### Running Tests

To run the tests:

```bash
./run_tests.sh
```

The test script assumes Bash, Unix Makefiles, and a Debug executable. For other
generators, configure with `-DBUILD_TESTING=ON` and build using
`cmake --build build --target CheapSynth01Tests --config Debug`.
Run `build/Tests/CheapSynth01Tests_artefacts/Debug/CheapSynth01Tests`
(append `.exe` on Windows). See [Tests/README.md](Tests/README.md) for details.
Tests are enabled by default; use `-DBUILD_TESTING=OFF` for product-only builds.

## Coding Guidelines

### Code Style

- Use "CS01" prefix for class names (e.g., CS01VCOProcessor)
- Use camelCase for variable names
- Use UPPER_SNAKE_CASE for constants
- Begin function names with verbs in camelCase
- Write all comments in English

### Formatting checks

CI checks all tracked `.cpp` and `.h` files under `Source` and `Tests` using
clang-format **21.1.7** and the repository's `.clang-format`. Dependencies and
generated build files are excluded. Formatting violations fail the CI job.

Install the pinned formatter in a virtual environment:

```bash
python3 -m venv /tmp/cheapsynth01-format
/tmp/cheapsynth01-format/bin/python -m pip install clang-format==21.1.7
export CLANG_FORMAT=/tmp/cheapsynth01-format/bin/clang-format
```

Run the same check as CI, or explicitly apply formatting fixes:

```bash
bash scripts/check-format.sh
bash scripts/check-format.sh --fix
```

If the matching formatter is already installed, set `CLANG_FORMAT` to its
executable path instead. Keep formatting-only changes separate from functional
changes. The unified CI workflow runs on pushes to `main`, tags, all pull requests,
and manual dispatches. Formatting runs once before the OS build/test matrix;
it does not run clang-tidy static analysis.

### JUCE Best Practices

- Follow JUCE design patterns
- Respect JUCE component lifecycles
- Use AudioProcessorValueTreeState for parameter management
- Consider cross-platform compatibility

### Audio Processing

- Avoid dynamic memory allocation in audio threads
- Perform heavy operations on the main thread
- Target zero latency
- Prefer float over double for floating point precision

### Sound Design

- Prioritize accurate emulation of the original hardware
- Implement accurate waveforms, filter characteristics, and envelope responses
- Include special features like breath control

## Pull Request Process

1. Ensure all tests pass
2. Update documentation if necessary
3. Title your PR clearly and reference any related issues
4. Wait for code review
5. Address any requested changes
6. Once approved, a maintainer will merge your PR

## Testing New Features

When implementing a new feature:

1. Write unit tests for the new functionality
2. Follow the test structure outlined in `Tests/README.md`
3. Ensure all existing tests still pass
4. If applicable, add an integration test

## Continuous Integration

This project uses GitHub Actions for continuous integration. Pushes to `main`,
tag pushes, all pull requests, and manual dispatches run the workflow:

- Building the project on multiple platforms (Windows, macOS, Linux)
- Running all tests
- Checking C++ formatting before platform builds
- Generating XML test reports and uploading them as artifacts

You can view the latest test results in the Actions tab of the GitHub repository.

Thank you for contributing to CheapSynth01!
