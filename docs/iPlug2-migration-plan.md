# iPlug2 migration completion status

Draft [PR #40](https://github.com/yasuyuki-baba/cheapsynth01/pull/40).
The non-runtime implementation/audit work is complete. Real plugin, native editor,
DAW and device verification is deferred by the user's instruction; the PR remains
a draft until those gates pass. Compilation and package checks do not instantiate
products and continue in CI.

## Frozen references

| Reference | Commit | Purpose |
| --- | --- | --- |
| Last common JUCE baseline | `1cbbdd7f39ec9f698d34363409da6dd3063f7f24` | Migration starting point, 213 definitions |
| Audited main | `32b91c8344a0581232afd7e0a79c5d25da22f42a` | Reconciled behavior, 272 frozen definitions |
| Original migration audit | `dd95b2057466f224a71ed86dd739f50c8f07c1ac` | Historical 38-case gap assessment |
| iPlug2OOS structural reference | `9b214c1121a0da1e8db14b86a565c942a3bfea7e` | Root SDK/project-local wrapper/CMake organization |
| Current iPlug2 SDK | `d54f69050f517e43b941d88c2a170f0a840b9ee4` | Pin retained; no combined SDK upgrade |

The archived audit counts/results describe those revisions, not current native
runtime coverage. Audited main is an ancestor of the migration branch. See
[the commit ledger](iPlug2-main-reconciliation.md) for each main change's destination.

## Product decisions

- Windows/macOS APP, VST3 and CLAP; macOS AUv2. No Linux/LV2, AAX/VST2, mobile or web targets.
- Retain CheapSynth01 branding, existing plugin identities and preset locations.
- Use native iPlug2 parameter/state/host APIs. JUCE project, automation and binary-state compatibility is excluded; XML presets remain supported.
- Retain standard iPlug2 interactions and the CS01-inspired panel, upright outlined CheapSynth01 logo with striped Synth, shared RES label and aligned controls.
- Retain repository GPLv3 licensing, SDK/dependency notices and hardware research provenance.

## Completed work

| Area | Implemented outcome | Verification surface |
| --- | --- | --- |
| OOS structure | Thin root CMake, root pinned iPlug2, project-local CheapSynth01 wrapper/config/resources, separate Source core; CMake-generated VS/Xcode presets, standalone-only option; macOS 10.15 minimum | Windows/macOS compilation, permanent plugin identity/index assertions |
| Main/assertion reconciliation | All 272 definitions reviewed; retained/adapted/observational/retired/deferred distinctions and reasons recorded; full historical Tests/Source/release sources archived | CI inventory provenance/coverage/evidence-path checker; no unresolved rows |
| DSP | Continuous upstream tone/noise, residual EG branch targets/retrigger/timing, independent VCA gate, empirical stages, fixed explicit routing, numerical recovery and restored double resonance-endpoint subtraction | Frozen filter bit references, independent gain/coupling/VCA transfer and timing equations, all-wave/Feet pitch, PWM/glissando, breath, source/release/event timelines |
| Converter/MIDI | 129-tap 4x FIR with 16-frame latency; stable negative-clamped/empty-boundary queue, future carry/overflow panic, highest note priority, all CC pairs and panic/reset semantics | Impulse/gain/rejection/known-bin controls, MIDI sample boundaries, independent held-set/generator oracles and real-core audio |
| State/host programs | Immutable per-instance prepared host catalogue, latest-valid boundary requests, stable file/type identity, pending sound serialization, coherent bounded patch/note-reset publication, VST3 queue and XML/bypass stream handling | Core concurrency/allocation/state assertions and eight real-SDK-interface simulations; no plugin instantiation |
| Presets | Seven factory sounds/manual settings, user XML import/export, overwrite/safe replacement, Unicode/portable names, protected factory/files, identity-preserving rename/delete/refresh and failures | XML/resource/manual oracles, audible factory renders, immediate post-MIDI save, complete current-state round trips |
| Keyboard/editor safety | Per-instance QWERTY state, PC/mouse ownership, UI/external note separation, desired-state coalescing, hide/focus/text/close cleanup and weak async session guards | Portable input/lifetime/concurrent-reset assertions; actual native focus/dialog/repaint behavior deferred |
| CI/package/release preparation | Core Debug/Release and native Windows/macOS jobs, JUnit/full logs with bounded failure summaries, complete source/native ZIP hash/mode/source/version/tag verification, offline source rebuild; opt-in signing plan and notarization instructions | 15 Python archive/signing fixtures; native compile/ZIP checks; no release/credentials created |
| Documentation/research | Current MIDI/host/build/preset/release docs; old software paths/measurements explicitly historical; portable onset WAV/CSV export, retained RC and conditional hardware equations | Four Python equation checks and three C++ research cases; no hardware calibration claim |

The active suite is 200 portable cases (186 correctness regressions, including
three research cases, and 14 observations), plus eight simulated SDK-interface
cases and three real iPlug2 parameter-presentation cases: 211 total. Counts are
not one-to-one historical assertion parity. The
inventory separates 39 deferred native cases and reasoned retirement of removed
JUCE interfaces, test doubles, old compatibility and Linux probes.

## Intentional changes and assertion boundaries

The old JUCE polyphase IIR, auxiliary IIR/oscillator/random primitives and batched
APVTS renderers are removed. Exact whole-JUCE-graph/wrapper output equality and
old optimization/lock timing probes are not current contracts. Frozen behavioral
models and empirical/gain expressions retain independent exact references;
converter/filter bandwidth, gain, folded bins, timing and convergence retain
explicit bounded numerical checks. Direct synthesis has no interpolated audio
input path. Events at/beyond a callback's end carry instead of old beyond-block
clamping. Free-running upstream sources are gated downstream, not by source
rendering silence. These differences are recorded per historical case.

Core allocation/free probes exercise prepared requests and first/variable/dense
queue+engine+UI-bridge rendering. They do not intercept arbitrary SDK malloc or
locks. No static drawing, empty UI suite, archived run or core mock is counted
as native interaction/runtime evidence. Hardware equation tests are conditional
research models, not production calibration.

## Deferred runtime gates

1. Load actual Windows/macOS APP/VST3/CLAP/AUv2 products, scan with native hosts and
   run applicable pluginval/VST3/CLAP/auval validators. Verify format metadata,
   channels, host preset lists, automation, current XML state, zero callbacks,
   concurrent program/state edits, instance isolation and 16-frame latency compensation.
2. Exercise actual IGraphics editor/control lifecycle: repeated Original/Modern
   RES switching and repaint captures, aligned tracks/labels, host/state updates,
   SDK value editing/format/default reset/fine adjustment and balanced gestures;
   dialog confirm/cancel/close/rebuild and keyboard drawer/scaling recreation.
3. Verify native focus/text/mouse/QWERTY ownership, external MIDI visual state and
   notes through focus loss/hide/close; instrument actual SDK callback/thread
   allocation/locking and UI notification paths.
4. Test standalone preferences/output channels/MIDI routing, persistence, device
   reconnect, sample-rate/buffer changes and playable output on Windows/macOS.
5. Before an eventual published release, perform native runtime/installation
   verification and signing/notarization with provisioned credentials. Hooks are
   prepared; actual signing/publication is not a migration-completion claim.

No additional portable migration work is hidden behind these gates. The exact
final revision's local tests and native build/offline-package CI evidence are
recorded in PR #40. Keep the PR draft while the requested runtime checks are deferred.
