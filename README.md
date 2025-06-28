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
# Create and configure build directory
cmake -B build

# Build the project
cmake --build build
```

### Running Tests

Configure the build with the plugin disabled and JUCE's headless mode enabled:

```bash
cmake -B build -DBUILD_PLUGIN=OFF -DBUILD_TESTS=ON -DJUCE_HEADLESS_PLUGIN_CLIENT=ON
cmake --build build --target CheapSynth01Tests
./build/CheapSynth01Tests
```


## Usage

Load the plugin in your DAW or launch the standalone application.

## License

This project is released under the MIT License. See the LICENSE file for details.
