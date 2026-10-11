# CheapSynth01 0.4.0

CheapSynth01 0.4.0 moves the instrument from JUCE to iPlug2 and updates the synth
panel, envelope timing and plugin integration.

## Changes since 0.3.0

- Migrate the standalone application and plugins to iPlug2. Supported products
  are Windows APP/VST3/CLAP and macOS APP/VST3/AUv2/CLAP (macOS 10.15 or later).
- Add a CS01-inspired panel, output level meter, display scaling and editor help.
  Improve preset selection, readable parameter units and momentary button feedback.
- Map Attack, Decay and Release to RC time constants using an assumed A taper.
  Retain factory preset panel positions and improve Clavinet and Synth Bass tails.
- Improve MIDI, note lifecycle, live envelope edits, preset/state handling and
  audio-boundary parameter publication, with expanded regression coverage.
- Use a 4x FIR output converter with a reported latency of 16 host samples.
- Verify complete source and native release archives against their file hashes,
  version and source commit; include dependency notices and SHA256SUMS.txt.

## Compatibility

This release changes compatibility and sound compared with 0.3.0.

- JUCE-era DAW projects, automation and binary state are not supported. Plugin
  identities remain unchanged, so save a copy of existing projects and retain the
  previous plugin build before upgrading.
- XML preset import/export remains supported. Envelope values now represent RC
  time constants in seconds; existing values can produce different durations and
  shapes, and values outside the new range are clamped. Normalized envelope
  automation also changes with the new taper; there is no automatic conversion.
- The FIR converter differs from the former JUCE IIR converter in high-frequency
  response and phase. Identical audio output is not promised.

The envelope taper and hardware models remain approximations, not calibration
against an original CS01.

## Validation status

Native plugin loading, DAW automation/state restoration, editor/device interaction
and installation checks remain pending in the recorded migration status. Core
tests, SDK-interface simulations and archive checks do not establish those results.
Signed/notarized distribution is also pending. See the
[native validation status](iPlug2-migration-plan.md) and
[release preparation steps](iPlug2-release-packaging.md#preparing-040).
