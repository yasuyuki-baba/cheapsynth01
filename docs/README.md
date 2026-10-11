# Documentation

CheapSynth01 uses iPlug2 on Windows/macOS. Start with the user guide for playing
and presets, or the development guide for building and testing.

Maintained product documentation lives directly in `docs/`. Hardware evidence
and technical background live in `hardware/`; frozen JUCE software records live
in `legacy-juce/`. Superseded copies are indexed under
[`legacy-juce/earlier-revisions/`](legacy-juce/earlier-revisions/README.md).

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
- [Signal flow](../README.md#signal-flow-architecture): sound generation, filtering,
  modulation, note gating and the output converter
- [MIDI realtime control flow](MIDI-realtime-control.md): parameter publication,
  audio/UI ownership and persistence
- [Host presets and state](iPlug2-host-integration.md): native format integration,
  block-boundary patch application and XML state
- [Active tests](../Tests/README.md): regressions, observations, research equations
  and SDK-interface tests
- [Release archives](iPlug2-release-packaging.md): distribution files, offline
  source rebuild, signing preparation and notarization

## Migration Status

- [Migration notes](iPlug2-migration.md)
- [Completion status and deferred runtime checks](iPlug2-migration-plan.md)
- [Main reconciliation ledger](iPlug2-main-reconciliation.md)
- [Historical test inventory](iPlug2-test-inventory.csv)

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

## Historical JUCE Documentation

The [historical documentation index](legacy-juce/README.md) preserves the previous
software design, audits and measurement results. In particular:

- [DSP responsibility boundaries](legacy-juce/software/DSP-responsibility-boundaries.md)
- [EG stateful model](legacy-juce/software/EG-stateful-model.md)
- [Note onset continuity](legacy-juce/software/Note-onset-continuity.md)
- [Waveform generation](legacy-juce/software/Waveform-generation.md)

These describe the frozen JUCE revision. Use the current architecture and migration
references above for the iPlug2 implementation and verification status.

Older versions formerly stored under `docs/` and `docs/tech/` are preserved in
[the earlier-version index](legacy-juce/earlier-revisions/README.md). They are
historical references; use the maintained links above when updating documents.
