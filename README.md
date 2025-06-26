# CheapSynth01

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
# Clone the repository
git clone https://github.com/yasuyukibaba/cheapsynth01.git
cd cheapsynth01

# Create and configure build directory
cmake -B build

# Build the project
cmake --build build
```

## Usage

Load the plugin in your DAW or launch the standalone application.

## License

This project is released under the MIT License. See the LICENSE file for details.

## Developer

- Yasuyuki Baba
