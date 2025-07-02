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

```bash
# Create and configure build directory
cmake -B build -DSTANDALONE_ONLY=ON

# Build the project
cmake --build build
```

For development, we recommend using the `-DSTANDALONE_ONLY=ON` option to reduce build times.

### Running Tests

To run the tests:

```bash
./run_tests.sh
```

## Coding Guidelines

### Code Style

- Use "CS01" prefix for class names (e.g., CS01VCOProcessor)
- Use camelCase for variable names
- Use UPPER_SNAKE_CASE for constants
- Begin function names with verbs in camelCase
- Write all comments in English

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

- Prioritize accurate emulation of the original sound
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

This project uses GitHub Actions for continuous integration. On each push and pull request to any branch, the following actions are performed:

- Building the project on multiple platforms (Windows, macOS, Linux)
- Running all tests
- Generating and publishing test reports

You can view the latest test results in the Actions tab of the GitHub repository.

Thank you for contributing to CheapSynth01!
