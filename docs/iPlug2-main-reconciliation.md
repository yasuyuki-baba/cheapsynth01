# Main reconciliation ledger

Baseline: `1cbbdd7f39ec9f698d34363409da6dd3063f7f24` → `32b91c8344a0581232afd7e0a79c5d25da22f42a`.

Port behavior and assertions, not JUCE runtime code. Main is merged structurally; this ledger tracks behavior/assertion reconciliation, which is not complete. Linux tooling is reference evidence only; supported products remain Windows/macOS.

| Commit | Change | Destination | Status |
| --- | --- | --- | --- |
| `30dcd72` | Refactor DSP behavioral model names and stage boundaries | Core DSP behavior / numerical reference assertions | implemented: behavioral-model names and empirical VCF/VCA boundaries ported; independent former-expression/reset regressions added |
| `f51c64c` | Sync on-screen modulation wheel with MIDI input | Native IGraphics controls, polling and lifecycle assertions | pending |
| `064dfa3` | Release v0.3.0 and unify internal version metadata | Version metadata / source and binary package verification | adapted: migration remains 2.0.0; archive creation verifies CMake/string/hex agreement, release gate requires matching tag/version/source |
| `4f2e59d` | Wait for asynchronous mod wheel updates in UI regression test | Native IGraphics controls, polling and lifecycle assertions | pending |
| `66a9812` | Fix audited UI, MIDI, envelope, preset and distribution stability | Preset file safety, MIDI, envelope, keyboard lifecycle and packaging | partial: preset overwrite/file protection/name validation and missing-file fallback restored; keyboard visibility/text/focus/close lifecycle and UI/external MIDI ownership restored; native interactions remain deferred |
| `7d0caec` | Preserve preset restoration after cached host programs and verify user cache transitions | Host program/state identity and real-time wrapper tests | partial: immutable per-instance prepared host catalogue, boundary application and pending-state identity restored; concurrent whole-patch publication and serialization covered in core; eight simulated real-SDK interface boundary cases pass; real plugin/host validation deferred |
| `4c0cbf4` | Match VCF and glissando test parameter types to production for sanitizer checks | Test organization, bounded numerical checks and diagnostics | pending |
| `056f839` | docs: record audit verification and measurement provenance | Validation evidence / current documentation; audit applicability | pending |
| `5235bcd` | Keep host program requests independent of message-thread checks and UI loading | Host program/state identity and real-time wrapper tests | pending |
| `8d5c125` | Preserve immediate host selection and reserved program state before audio application | Host program/state identity and real-time wrapper tests | pending |
| `182d215` | Verify pending user program identity while retaining the unapplied sound | Host program/state identity and real-time wrapper tests | pending |
| `43055ed` | docs: record host program follow-up verification and remaining realtime limits | Validation evidence / current documentation; audit applicability | pending |
| `124bb5e` | Verify complete file hashes for binary and source release packages in CI | Version metadata / source and binary package verification | implemented: complete per-file hashes/modes, exact payload coverage and release source/version verification; independent Python contract tests |
| `507e504` | Require tag release packages to match the checked-out source revision | Version metadata / source and binary package verification | implemented: complete per-file hashes/modes, exact payload coverage and release source/version verification; independent Python contract tests |
| `f66f4ba` | docs: record package integrity regression and actual Linux archive checks | Validation evidence / current documentation; audit applicability | pending |
| `52f9d77` | Reuse VCO parameter bindings and batch modulated rendering without changing sample progression | Core DSP behavior / numerical reference assertions | pending |
| `7fb612f` | docs: record VCO sample equivalence and repeated complete-graph measurements | Validation evidence / current documentation; audit applicability | pending |
| `d9aaa73` | Poll GUI parameter bindings without audio-thread message posts or cold APVTS listeners | Native IGraphics controls, polling and lifecycle assertions | pending |
| `8091162` | docs: record notification posting reproduction, complete regressions and remaining RT locks | Validation evidence / current documentation; audit applicability | pending |
| `630fd42` | Select filter and LFO routes on audio within the existing fixed graph | Explicit engine routing / held-note and real-time assertions | compare existing core coverage |
| `47e4fe2` | Exercise active synthesis while switching every audio callback | Validation evidence / current documentation; audit applicability | pending |
| `e5588a7` | Distinguish contended graph locks from acquisitions in the Linux RT probe | Validation evidence / current documentation; audit applicability | pending |
| `70fe914` | Preserve pthread error semantics in the contention probe | Reference probes / test evidence; audit applicability | pending |
| `f727b47` | Record graph lock contention and controlled wait observations | Validation evidence / current documentation; audit applicability | pending |
| `04598f9` | Document audio-owned routing and legacy comparison probes | Validation evidence / current documentation; audit applicability | pending |
| `250590c` | Verify restored routing masks and fixed connections without weakening note lifecycle checks | Validation evidence / current documentation; audit applicability | pending |
| `5f4053a` | Record final routing regressions, contention scope and three quiescent comparisons | Validation evidence / current documentation; audit applicability | pending |
| `35fd6a2` | Keep upstream synthesis continuous across note boundaries | Core DSP behavior / numerical reference assertions | continuous upstream rendering and VCA gate ported; source lifecycle assertions added |
| `7e5af51` | Cover noise continuity across an in-block release deadline | Core DSP behavior / numerical reference assertions | per-sample continuous noise covers deadline; original block scenario still pending |
| `d6df36a` | Record rebased validation and inconclusive listening results | Validation evidence / current documentation; audit applicability | pending |
| `1795193` | Retain envelope branch targets and progress across retriggers and time edits | Core DSP behavior / numerical reference assertions | core trajectory/gate behavior ported; five independent EG assertion replacements; remaining cases pending |
| `39999ec` | Record validation of the residual-state envelope update | Validation evidence / current documentation; audit applicability | pending |
| `00af2f2` | Keep macOS test failure diagnostics bounded | Test organization, bounded numerical checks and diagnostics | pending |
| `46e2e33` | Preserve original VCA arithmetic with the note gate fully open | Core DSP behavior / numerical reference assertions | open-gate expression ported; arithmetic regression added; native ARM evidence pending |
| `babe156` | Keep gate corrections separate from the original EG expression | Core DSP behavior / numerical reference assertions | non-EG gate correction ported; gain regression added; full numerical suite pending |
| `186c362` | Fix exclusive GUI choice switches and parameter synchronization | Native IGraphics controls, polling and lifecycle assertions | pending |
| `2c6cf66` | Organize documentation and keep references entirely local | Validation evidence / current documentation; audit applicability | pending |
| `ed94aa4` | Update architecture and circuit documentation for current DSP behavior | Validation evidence / current documentation; audit applicability | pending |
| `8e4f4ad` | Describe current behavior and hardware evidence without future work plans | Validation evidence / current documentation; audit applicability | pending |
| `826b5d8` | Separate software documentation from hardware analysis | Validation evidence / current documentation; audit applicability | pending |
| `30b400b` | Correct documentation claims against current implementation and recorded evidence | Validation evidence / current documentation; audit applicability | pending |
| `a76cc90` | Make documentation readable without external source PDFs | Validation evidence / current documentation; audit applicability | pending |
| `8eef56b` | Improve documentation coverage, current specification navigation and evidence scope | Validation evidence / current documentation; audit applicability | pending |
| `851677f` | Document waveform generation from shared square and phase source | Validation evidence / current documentation; audit applicability | pending |
| `2044325` | Document filtered noise generation and live waveform/source switching | Validation evidence / current documentation; audit applicability | pending |
| `e9c0049` | Translate stability audit documentation into English | Validation evidence / current documentation; audit applicability | pending |
| `129b9e9` | Shorten DSP regressions and split CI test stages | Test organization, bounded numerical checks and diagnostics | pending |
| `32b91c8` | Limit test optimization to bounded period measurements | Test organization, bounded numerical checks and diagnostics | pending |

## Merge scope

The audited main revision is an ancestor after this reconciliation commit. The
iPlug2/OOS CMake/CI targets, independent core and native parameter/state design
remain authoritative. JUCE processor/UI/graph internals are not restored.
Main's behavioral model naming and empirical stage split are ported with
independent arithmetic/reset assertions. Earlier envelope/gate/source continuity
ports remain active. This does not claim all 272 historical assertions are replaced.

The complete main Tests snapshot (272 definitions), Source reference and
release tools are archived under Tests/legacy-juce; original software/audit
documentation is under docs/legacy-juce with historical-scope banners. Hardware
analysis is retained under docs/hardware. New current docs are indexed by
docs/README.md. Old session and automation compatibility remains explicitly out
of scope. Framework-specific optimization/locking/listener probes remain historical
evidence, not current native product guarantees. Real plugin/DAW/device checks
are deferred; native compilation and archive verification continue in CI.
