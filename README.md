# CheapSynth01

[![Build](https://github.com/yasuyuki-baba/cheapsynth01/actions/workflows/build.yml/badge.svg)](https://github.com/yasuyuki-baba/cheapsynth01/actions/workflows/build.yml)
[![Tests](https://github.com/yasuyuki-baba/cheapsynth01/actions/workflows/tests.yml/badge.svg)](https://github.com/yasuyuki-baba/cheapsynth01/actions/workflows/tests.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![JUCE](https://img.shields.io/badge/JUCE-Framework-orange.svg)](https://juce.com/)

CheapSynth01 is a software emulation of an early 80's compact monophonic synthesizer. Developed using the JUCE framework, it recreates classic analog sounds with precision.

## Features

- Faithful circuit emulation
- Two filter types with different resonance control modes:
  - Original VCF with toggle resonance (High/Low)
  - Modern VCF with continuous resonance control

## Signal Flow Architecture

CheapSynth01 uses JUCE's AudioProcessorGraph to implement a modular synthesis architecture. The signal flow changes dynamically based on parameter settings.

```mermaid
graph TD
    MIDI[MIDI Input] --> MidiProc[MIDI Processor]
    MidiProc --> |Triggers| EG[Envelope Generator]
    
    VCOProc[VCO Processor] --> |manages| GenSwitch{Generator Type}
    GenSwitch --> |Tone| ToneGen[Tone Generator]
    GenSwitch --> |Noise| NoiseGen[Noise Generator]
    ToneGen --> |implements| ISG[ISoundGenerator]
    NoiseGen --> |implements| ISG
    MidiProc --> |Uses| ISG
    
    VCOProc --> |Audio| FilterSwitch{Filter Type}
    
    FilterSwitch --> |Original| OriginalVCF[Original VCF]
    FilterSwitch --> |Modern| ModernVCF[Modern VCF]
    
    OriginalVCF --> |Toggle Resonance| VCA[VCA]
    ModernVCF --> |Continuous Resonance| VCA
    
    EG --> |Modulation| VCA
    EG --> |Modulation| OriginalVCF
    EG --> |Modulation| ModernVCF
    
    LFO[LFO] --> LFOTarget{LFO Target}
    LFOTarget --> |VCO| VCOProc
    LFOTarget --> |VCF| OriginalVCF
    LFOTarget --> |VCF| ModernVCF
    
    VCA --> Output[Audio Output]
```

### Key Signal Paths:

1. **Main Audio Path**:
   - Sound Generation: VCO Processor managing Tone Generator and Noise Generator
   - Filtering: Either Original VCF (vintage) or Modern VCF
   - Amplification: VCA controls final output level

2. **Control Paths**:
   - MIDI Processor: Interacts with VCO Processor via ISoundGenerator interface
   - Envelope Generator: Modulates both VCA (amplitude) and VCF (filter cutoff)
   - LFO: Can be routed to either VCO (pitch modulation) or VCF (filter modulation)
   
3. **Dynamic Routing**:
   - Filter Type parameter switches between Original (vintage) and Modern filter types
   - Each filter type has its own resonance control: Toggle mode for Original VCF and Continuous mode for Modern VCF
   - Feet parameter switches between Tone and Noise generators within the VCO Processor
   - LFO Target parameter determines modulation destination

4. **Architecture Pattern**:
   - Centralized communication via CS01AudioProcessor
   - Interface-based design with ISoundGenerator
   - Component decoupling using callback functions

The architecture supports real-time parameter changes, allowing for expressive performances and sound design.

## Supported Platforms

- Windows
- macOS
- Linux

## Supported Plugin Formats

- Standalone application
- VST3
- AU (macOS only)
- LV2 (macOS and Linux only)
- CLAP (CLever Audio Plugin)

## Build Instructions

For build instructions and development setup, please see [CONTRIBUTING.md](CONTRIBUTING.md).

## Usage

Load the plugin in your DAW or launch the standalone application.

## License

This project is released under the GNU General Public License v3. See the LICENSE file for details.
