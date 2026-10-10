# iPlug2 migration completion plan

Status: planning baseline for draft [PR #40](https://github.com/yasuyuki-baba/cheapsynth01/pull/40).
This document records the audit and proposed implementation order; it does not
claim the missing behavior or native validation has been completed.

## Frozen comparison points

| Source | Commit | Purpose |
| --- | --- | --- |
| Last common JUCE baseline | `1cbbdd7f39ec9f698d34363409da6dd3063f7f24` | Migration started with 213 Google Test definitions |
| Audited main | `32b91c8344a0581232afd7e0a79c5d25da22f42a` | Current behavior and 272 Google Test definitions across 25 files |
| Audited migration | `dd95b2057466f224a71ed86dd739f50c8f07c1ac` | 38 active core tests; historical JUCE tests are inactive |
| [iPlug2OOS](https://github.com/iPlug2/iPlug2OOS) | `9b214c1121a0da1e8db14b86a565c942a3bfea7e` | Target project organization and native build/validation examples |
| Current iPlug2 submodule | `d54f69050f517e43b941d88c2a170f0a840b9ee4` | Preserve this pin during structural migration |
| OOS example's iPlug2 pin | `b64192fe18afd9bc9a1fe324db5aceb48f4a0eee` | A separate SDK upgrade, if needed, requires separate validation |

Counts describe source test definitions, not equivalent coverage. The active
suite combines broad engine checks with two retained numerical model suites.
Debug/Release core runs passed 38/38 in the prior local verification; this is not
native Windows/macOS product evidence. No workflow run for the audited migration
HEAD was found during this audit. The PR also needs reconciliation with newer main.

## Scope and invariants

- Windows/macOS standalone, VST3 and CLAP; macOS AUv2. Linux remains excluded.
- Retain CheapSynth01 naming, current plugin identities, preset paths and legacy
  state import. Do not generate fresh IDs while duplicating an OOS template.
- Preserve the chosen standard iPlug2 interactions and CS01-inspired panel,
  including upright CheapSynth01 lettering and striped Synth lettering.
- Keep the independent core and its device-free test target.
- Do not automatically add AAX, VST2, AUv3, mobile, web or OOS publishing workflows.
- Retain repository licensing and third-party notices. Adopting the template does
  not change the license of existing code or contributions.

## Audit findings

| Priority | Area / evidence in audited migration | Gap and required outcome |
| --- | --- | --- |
| P0 | Branch divergence; `Source/CS01Synth`, inactive `Tests/legacy-juce` | Main added DSP, real-time, preset and UI fixes after the branch point. Port their behavior and regression assertions; do not merge JUCE implementation mechanically. |
| P0 | `Source/CS01AudioProcessor.cpp`, `Source/Parameters.h` | Plugin IDs are retained, but legacy host automation IDs are not mapped. XML ID compatibility alone does not restore DAW automation. Establish a format-specific compatibility manifest and old-session fixtures. |
| P0 | Processor construction uses one default host preset | The UI has factory/user presets, but the former host program catalogue/current-program behavior is absent. Restore supported host program interfaces and stable selection identity. |
| P0 | `Source/ProgramManager.cpp`, processor state callbacks | Review callback threads, coherent patch publication and UI dispatch. Current state synchronization can call `SendParameterValueFromDelegate`, documented by iPlug2 as unsuitable for the audio thread. Validate wrapper callbacks before deciding the implementation; this is a risk identified by inspection, not an observed crash. |
| P1 | Tone/noise, EG and VCA implementations | Newer main's continuous upstream synthesis, retrigger/time-edit branch handling and gate/arithmetic corrections are not incorporated. Bring these behaviors and their tests across. |
| P1 | User preset writes in `Source/ProgramManager.cpp` | Duplicate names currently create suffixed files; former saves overwrite the named preset. Newer main also protects existing files through temporary-file replacement. Specify and restore save/overwrite behavior and failure preservation. |
| P1 | IGraphics QWERTY helper and editor lifecycle | Keyboard handler remains installed when the drawer is hidden. SDK helper holds static key state. Restore visibility/text-focus gating and per-instance ownership; release UI-held notes on focus loss, hide and close without releasing external MIDI notes. |
| P1 | `Tests/CoreTest.cpp`, `Tests/CMakeLists.txt` | Detailed DSP, wrapper, editor, preset panel and real-time audit assertions were not migrated. Main's later tests and reference oracles are not all in the archive either. Track individual assertions, not just test counts. |
| P1 | `.github/workflows/ci.yml` | Native builds are configured, but actual full native/host evidence is missing. Add validators, executable GUI coverage and preserved test reports. |
| P1 | CI packaging and `scripts/` | Main's source packaging, notices and release hash/source/tag validation are absent. Adapt these to the new dependencies and products. |
| P2 | Root CMake and `libs/iPlug2` | Current project organization is not OOS-shaped. Adopt root SDK/common settings, project-local configuration/resources and supported presets without changing identities. |
| P2 | `run_tests.sh`, historical modeling documents | Old test grouping/reporting and documentation links need updating. Clearly distinguish historical JUCE measurements, current production DSP and idealized characterization scripts. |

## Implementation order and completion gates

### 0. Reconcile the behavior baseline

1. Keep the refs above as reproducible baselines. Record subsequent main changes
   separately so the target does not move silently.
2. Enumerate main commits since the common baseline and classify each as behavior,
   test, release tooling, documentation or obsolete JUCE implementation.
3. Resolve PR conflicts by carrying applicable behavior across. Preserve explicit
   product decisions made during this migration.
4. Capture old parameter IDs/order/flags/groups, program identities and representative
   state/DAW projects before changing the wrapper further.

Gate: every applicable main change has an implementation/test destination or a
specific exclusion reason; native baseline build results and known failures are recorded.

### 1. Adopt the OOS project structure

Use the official template as the structural reference, adapting its examples:

```text
CMakeLists.txt             # thin root entry; include iPlug2.cmake, add targets
CMakePresets.json          # supported macOS/Windows configurations
common-mac.xcconfig
common-win.props
iPlug2/                   # pinned SDK submodule
CheapSynth01/
  CMakeLists.txt           # iplug_add_plugin; explicit supported formats
  config.h
  config/                 # project-specific platform settings
  projects/               # maintained native projects where needed
  resources/
  scripts/                # adapted packaging helpers
Source/                   # framework-independent core, or equivalent core target
Tests/
```

Move existing settings/resources deliberately; preserve VST3 component/controller
UIDs, AU `aumu/CS01/BABA`, CLAP ID and bundle ID. Compare OOS common settings with
the pinned SDK. Do not combine the directory reorganization with an SDK upgrade.
Retain TinyXML2, factory resource generation and standalone preferences behavior.
Avoid copying example licenses, unused platforms or unrelated automation.

Gate: clean checkout/submodule setup builds core Debug/Release and every supported
native product on Windows/macOS, with identity assertions and documented commands.
Any subsequent SDK upgrade is its own change with the same checks.

### 2. Restore DSP regression depth and newer main behavior

Start with [the case inventory](iPlug2-test-inventory.csv). Preserve meaningful
numerical and behavioral assertions while replacing JUCE fixtures with core fixtures.
Prioritize EG retrigger/live timing, continuous VCO/noise state, VCA gating, MIDI
priority/controllers, routing and both filters. Bring across frozen reference
implementations when they establish equivalence independently of the new code.

Cover waveform/pitch/feet/PWM/glissando and live changes; filter transfer,
saturation/recovery/non-finite inputs; EG residual level and timing; VCA depth and
arithmetic; breath modulation end to end. Verify variable/zero block sizes, MIDI
boundary ordering/carry/overflow and block partition independence. Extend allocation
and real-time checks to the wrapper callback and parameter/program/state paths,
not only `SynthEngine::render`.

Treat the intentional converter change separately: JUCE polyphase IIR becomes a
129-tap, 4x FIR with 16 host-frame latency. Preserve common DSP assertions; use
explicit tolerances for converter bandwidth, gain, alias rejection and impulse
response rather than demanding whole-plugin bit equality. Record performance
measurements separately from bounded correctness tests.

Gate: every inventory case has a reviewed assertion mapping, replacement or
reasoned retirement. Core Debug/Release and appropriate sanitizer checks pass;
converter differences have documented thresholds and no unexplained sound changes.

### 3. Restore host, preset and real-time integration

- Map legacy host parameter identifiers per format, including order, flags,
  groups and normalization. Verify old projects retain sound and automation.
  Keep SDK changes out of the submodule working tree; if an adapter/patch is
  necessary, record it reproducibly and test each affected format.
- Publish prepared preset/state snapshots at audio block boundaries. Keep disk I/O,
  parsing, locks and allocation outside real-time program selection. Defer UI
  updates through supported iPlug2 mechanisms after determining callback threads.
- Restore host program catalogues where supported, independently of editor creation.
  Track factory/user selections by stable identity, including rename/delete,
  missing files and host-cached catalogues.
- Restore deliberate overwrite behavior using safe replacement. Test malformed
  state, failed writes/imports, Unicode paths and Windows filename restrictions;
  preserve both existing files and selection on failure.
- Exercise all parameters, legacy raw/binary XML, automation without an editor,
  concurrent preset/state changes and close/reopen with multiple instances.

Gate: native old-session fixtures pass for VST3/AU/CLAP as applicable, including
program selection, state round trips, automation and reported latency compensation.
Thread/real-time checks cover the actual wrapper paths.

### 4. Add native editor and standalone checks

Create a harness using actual iPlug2 controls/editor lifecycle. Core mocks or
static drawings cannot establish interaction or repaint correctness.

- Check Original/Modern VCF switching by mouse, host automation and state restore:
  shared RES label, aligned slider tracks/label baselines, correct visibility and
  repeated toggling without stale pixels. Use deterministic repaint captures.
- Check standard value editing, units/fine adjustment/default reset; preset text
  entry/dialog errors; drawer size/scaling and editor recreation.
- Gate PC-key input on drawer visibility and text focus. Verify per-instance note
  ownership and note release on focus loss/hide/close. Verify external MIDI display
  and notes independently so UI cleanup cannot silence external performance.
- Verify standalone audio output/MIDI preferences, persistence, device reconnect,
  sample-rate changes and playable output on Windows/macOS.

Gate: automated native assertions plus recorded manual device/DAW checks pass.
OOS's pluginval examples use `--skip-gui-tests`; those runs do not satisfy this gate.

### 5. Complete CI, distribution and documentation

Adapt OOS native validation examples: pluginval/VST3 validator, clap-validator and
macOS auval where applicable. Keep supported product builds and core Debug/Release
jobs, upload test results and useful failure logs. Restore useful test groups via
CTest labels/developer commands, distinguishing core, numerical, host, UI and
observational measurements.

Adapt main's notices/source packaging and release validation to the pinned SDK and
new formats. Produce matching binary/source archives with hashes, source commit/tag
and dependency/license provenance. Verify the artifact being released is the one
that passed checks. Consolidate version metadata; do not copy OOS example versions
or silently replace the branch's version with main's. Prepare platform packaging
and signing/notarization hooks for the eventual release workflow.

Update developer/user instructions, test links and current DSP claims. Label old
IIR/zero-latency documents as historical; `tools/dsp_characterize.py` models idealized
probes and is not production-plugin coverage. Retain hardware/model research history.

Gate: clean-checkout native CI, validators, separate GUI checks and package validation
pass; supported release artifacts and corresponding source are reproducible and
documented. The draft PR becomes ready only after these evidence gates are met.

## Tracking rules

The CSV contains all 272 definitions from the frozen main reference, including
source line and whether the same suite/case existed at the migration baseline.
`pending-case-review` means audit work remains; it does not assert that no current
test covers any part of that case. `candidate_active_family` is a starting point
for comparison, not an equivalence claim. Compare assertions and inputs before
setting `disposition` to `ported`, `replaced` or `retired` and recording evidence.

JUCE graph construction or framework listener implementation checks may become
native wrapper/routing tests or be retired with a reason. Their externally visible
behavior must remain covered. Main's Python release-validation and equation checks,
reference fixtures, sanitizer tooling and manual evidence are additional work;
the 272-row Google Test inventory is not the entire verification surface.

Implement phases as small reviewable commits/PRs. Compatibility investigation can
begin with phase 0; finish structural changes before investing in native UI harness
paths. Do not mark a phase complete from archived tests, test count alone, cross
compilation alone or validators that skipped the relevant UI paths.
