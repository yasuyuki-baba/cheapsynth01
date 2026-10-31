# CheapSynth01 0.4.0

CheapSynth01 0.4.0 uses iPlug2 for the standalone application and plugins.

## Features

- Supported products
  are Windows APP/VST3/CLAP and macOS APP/VST3/AUv2/CLAP (macOS 10.15 or later).
- A CS01-inspired panel, output level meter, display scaling and editor help,
  with preset selection, readable parameter units and momentary button feedback.
- Attack, Decay and Release use RC time constants in seconds with an assumed
  A-taper mapping shared by panel controls and normalized automation.
  XML values outside the parameter range are clamped.
- MIDI, note lifecycle, live envelope edits, preset/state handling and
  audio-boundary parameter publication have regression coverage.
- A 4x FIR output converter reports a latency of 16 host samples.
- XML preset import/export is supported.
- Complete source and native release archives are verified against their file
  hashes, version and source commit, with dependency notices and SHA256SUMS.txt.

The envelope taper and hardware models remain approximations, not calibration
against an original CS01.

## Validation status

Native plugin loading, DAW automation/state restoration, editor/device interaction
and installation checks remain pending in the recorded native validation status. Core
tests, SDK-interface simulations and archive checks do not establish those results.
Signed/notarized distribution is also pending. See the
[native validation status](iPlug2-migration-plan.md) and
[release preparation steps](iPlug2-release-packaging.md#preparing-040).
