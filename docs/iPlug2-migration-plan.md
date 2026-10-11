# Native validation status

The current products target Windows/macOS: APP, VST3 and CLAP on both platforms,
plus AUv2 on macOS. Portable tests, SDK-interface simulations, compilation and
package verification cover their respective surfaces. They do not instantiate
products or replace real host, editor and device validation.

## Current automated checks

- Portable DSP/state core: numerical references, routing, MIDI, envelope/source
  continuity, patch publication, preset behavior and allocation probes
- Native SDK interfaces: VST3 queues/state streams and iPlug2 parameter presentation
- Python tests: release archives, signing preparation and conditional hardware equations
- CI: formatting, Windows/macOS core/native builds and offline source rebuild

See [the test guide](../Tests/README.md), [DSP design](DSP-design.md),
[host integration](iPlug2-host-integration.md) and
[release preparation](iPlug2-release-packaging.md) for scope and commands.

## Remaining runtime checks

These checks remain pending; no automated mock is counted as their completion.

- Plugin scanning/loading, real DAW automation, program selection and state restoration
- AU validation and host compensation for the reported 16-sample FIR latency
- Editor controls, gestures, keyboard focus, resize, asynchronous dialogs and teardown
- Standalone audio/MIDI preferences, device switching and persistence after restart
- Complete SDK callback allocation/locking and native control thread behavior
- Signed distribution and macOS notarization when preparing an actual release

Record platform, product format, host/device versions, source revision and results
when performing these checks. Compilation and ZIP checks alone do not establish
runtime readiness.

## Compatibility scope

Plugin identities, parameter index order, preset locations and XML preset support
are retained. Loading old JUCE DAW projects, automation or binary state is outside
scope. Hardware research provenance remains documented under `docs/hardware/`.
Earlier implementation archives and migration ledgers are available in Git history.
