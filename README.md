# CheapSynth01

[![CI](https://github.com/yasuyuki-baba/cheapsynth01/actions/workflows/ci.yml/badge.svg)](https://github.com/yasuyuki-baba/cheapsynth01/actions/workflows/ci.yml)

CheapSynth01 is an iPlug2-based, monophonic synthesizer inspired by an early
1980s compact synth. Circuit-informed models retain documented approximations;
software regression tests do not establish calibration against the original hardware.

- Five tonal waveforms, white noise, discrete glissando, PWM and LFO modulation
- Original VCF with Low/High resonance and Modern VCF with continuous resonance
- Stateful envelope, asymmetric pitch bend and breath control
- Factory sounds transcribed from the owner's manual, user XML presets and old
  XML preset import
- On-screen keyboard, PC-keyboard MIDI input and output scope
- Four-times internal processing with a linear-phase FIR output converter

Desktop products: Windows/macOS Standalone, VST3 and CLAP, plus AUv2 on macOS.
Supported development and test environments are Windows and macOS.

See [development/build instructions](CONTRIBUTING.md), [test guide](Tests/README.md),
[migration and compatibility notes](docs/iPlug2-migration.md),
[factory presets](docs/Factory-presets.md), [MIDI implementation](docs/MIDI-Implementation.md),
and [model limitations](docs/Circuit-model-unknowns.md).

## Version 2 compatibility

The iPlug2 version retains the JUCE version's AU, CLAP and VST3 plugin identifiers
and macOS bundle ID. The state reader accepts legacy JUCE binary XML payloads.
Old automation parameter IDs are not yet translated to iPlug2 parameter indices,
so retaining the plugin identity does not establish full DAW-project compatibility.
Exported XML presets can also be imported in the new version.

The output converter has 16 host samples of latency, which is reported to the
host. Its high-frequency response and phase differ from the previous JUCE IIR
converter, so identical audio output is not promised.

## License

The project remains GPLv3; see [LICENSE](LICENSE). Removing JUCE does not change
the license of existing project contributions. iPlug2 is zlib-like, TinyXML2 is
zlib, and the bundled Roboto font is Apache-2.0. See
[third-party notices](THIRD_PARTY_NOTICES.md).
