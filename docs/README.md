# Documentation

CheapSynth01 uses iPlug2 on Windows/macOS. Start with the user guide for playing
and presets, or the development guide for building and testing.

Product documentation lives directly in `docs/`; hardware evidence and technical
background live in `hardware/`. Earlier implementations, migration ledgers and
superseded documents are available in Git history.

## Playing and Presets

- [User guide](User-guide.md): device settings, keyboard input, controls/defaults,
  factory and user presets, session behavior and troubleshooting
- [Factory presets](Factory-presets.md): owner's-manual panel readings and their
  conversion to plugin values
- [MIDI implementation](MIDI-Implementation.md): supported channels, controllers,
  note priority and panic messages

## Development and Current Architecture

- [Build instructions](../CONTRIBUTING.md): Windows/macOS prerequisites, CMake
  commands and presets, standalone-only builds and project layout
- [DSP design and modeling limits](DSP-design.md): stage ownership, envelope,
  filter hypotheses and verification
- [Signal flow](../README.md#signal-flow-architecture): sound generation, filtering,
  modulation, note gating and the output converter
- [MIDI realtime control flow](MIDI-realtime-control.md): parameter publication,
  audio/UI ownership and persistence
- [Host presets and state](iPlug2-host-integration.md): native format integration,
  block-boundary patch application and XML state
- [Active tests](../CheapSynth01/tests/README.md): regressions, observations, research equations
  and SDK-interface tests
- [Release archives](iPlug2-release-packaging.md): distribution files, offline
  source rebuild, signing preparation and notarization

## Compatibility and Native Validation

- [Migration notes](iPlug2-migration.md)
- [Native validation status](iPlug2-migration-plan.md)

Actual plugin/DAW, native editor and device verification remains pending.
Core tests, SDK-interface simulations, compilation and archive checks do not
replace those runtime checks.

## Hardware Evidence and Modeling Limits

- [Hardware documentation index](hardware/README.md): circuit audits, source
  provenance, unresolved details and device background
- [Circuit-model limitations](hardware/Circuit-model-unknowns.md)
- [Circuit evidence status](hardware/Circuit-evidence-status.md)
- [Hardware source catalog](hardware/Source-catalog.md)

Hardware analysis records circuit evidence, conditional equations and unverified
interpretations separately. These records retain their original provenance;
software regression tests do not establish calibration against original hardware.
