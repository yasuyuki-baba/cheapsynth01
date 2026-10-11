# Main reconciliation ledger

Baseline: `1cbbdd7f39ec9f698d34363409da6dd3063f7f24` → `32b91c8344a0581232afd7e0a79c5d25da22f42a`.

The audited main is an ancestor of the migration branch. Every listed change and
all 272 historical test definitions have reviewed destinations/dispositions.
Portable work is complete; native runtime/editor/host/device verification remains
deferred by user instruction. Windows/macOS are the supported environments.

| Commit | Change | Destination | Status |
| --- | --- | --- | --- |
| `30dcd72` | Refactor DSP behavioral model names and stage boundaries | Core DSP behavior / numerical reference assertions | implemented: behavioral-model names and empirical VCF/VCA boundaries ported; independent former-expression/reset regressions added |
| `f51c64c` | Sync on-screen modulation wheel with MIDI input | Native IGraphics controls, polling and lifecycle assertions | implemented idle refresh/atomic MIDI parameter publication; native wheel repaint/interaction deferred |
| `064dfa3` | Release v0.3.0 and unify internal version metadata | Version metadata / source and binary package verification | adapted: migration remains 2.0.0; archive creation verifies CMake/string/hex agreement, release gate requires matching tag/version/source |
| `4f2e59d` | Wait for asynchronous mod wheel updates in UI regression test | Native IGraphics controls, polling and lifecycle assertions | retired JUCE message-loop waiting; actual SDK idle repaint assertions deferred |
| `66a9812` | Fix audited UI, MIDI, envelope, preset and distribution stability | Preset file safety, MIDI, envelope, keyboard lifecycle and packaging | implemented preset safety, coherent state, EG/MIDI/source and per-instance keyboard lifecycle; native interactions deferred |
| `7d0caec` | Preserve preset restoration after cached host programs and verify user cache transitions | Host program/state identity and real-time wrapper tests | implemented immutable host catalogue/current identity, prepared boundary requests and core plus simulated SDK tests; native hosts deferred |
| `4c0cbf4` | Match VCF and glissando test parameter types to production for sanitizer checks | Test organization, bounded numerical checks and diagnostics | adapted production float timing/model ranges; core numerical/sanitizer checks; frozen oracles retained |
| `056f839` | docs: record audit verification and measurement provenance | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `5235bcd` | Keep host program requests independent of message-thread checks and UI loading | Host program/state identity and real-time wrapper tests | implemented thread-independent prepared requests and latest-valid boundary application; native callback instrumentation deferred |
| `8d5c125` | Preserve immediate host selection and reserved program state before audio application | Host program/state identity and real-time wrapper tests | implemented requested identity before audio apply, reselection/cancellation and pending-save tests |
| `182d215` | Verify pending user program identity while retaining the unapplied sound | Host program/state identity and real-time wrapper tests | implemented prepared user identity/cached sound and pending-session restoration assertions |
| `43055ed` | docs: record host program follow-up verification and remaining realtime limits | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `124bb5e` | Verify complete file hashes for binary and source release packages in CI | Version metadata / source and binary package verification | implemented: complete per-file hashes/modes, exact payload coverage and release source/version verification; independent Python contract tests |
| `507e504` | Require tag release packages to match the checked-out source revision | Version metadata / source and binary package verification | implemented: complete per-file hashes/modes, exact payload coverage and release source/version verification; independent Python contract tests |
| `f66f4ba` | docs: record package integrity regression and actual Linux archive checks | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `52f9d77` | Reuse VCO parameter bindings and batch modulated rendering without changing sample progression | Core DSP behavior / numerical reference assertions | retired JUCE APVTS binding/batched renderer optimization; scalar core and independent pitch/PWM/glissando/event references retained |
| `7fb612f` | docs: record VCO sample equivalence and repeated complete-graph measurements | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `d9aaa73` | Poll GUI parameter bindings without audio-thread message posts or cold APVTS listeners | Native IGraphics controls, polling and lifecycle assertions | retired custom JUCE attachments; implemented atomic publication/SDK idle refresh; native control/thread/gesture verification deferred |
| `8091162` | docs: record notification posting reproduction, complete regressions and remaining RT locks | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `630fd42` | Select filter and LFO routes on audio within the existing fixed graph | Explicit engine routing / held-note and real-time assertions | implemented explicit audio-owned routing; independent connected chain and live-release/event partition assertions |
| `47e4fe2` | Exercise active synthesis while switching every audio callback | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `e5588a7` | Distinguish contended graph locks from acquisitions in the Linux RT probe | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `70fe914` | Preserve pthread error semantics in the contention probe | Reference probes / test evidence; audit applicability | retired Linux-only pthread interception probe; preserved as historical reference |
| `f727b47` | Record graph lock contention and controlled wait observations | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `04598f9` | Document audio-owned routing and legacy comparison probes | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `250590c` | Verify restored routing masks and fixed connections without weakening note lifecycle checks | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `5f4053a` | Record final routing regressions, contention scope and three quiescent comparisons | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `35fd6a2` | Keep upstream synthesis continuous across note boundaries | Core DSP behavior / numerical reference assertions | implemented continuous upstream sources and VCA gate; all-wave/in-block noise deadline and release silence assertions |
| `7e5af51` | Cover noise continuity across an in-block release deadline | Core DSP behavior / numerical reference assertions | implemented portable 512-sample noise deadline/RMS scenario at three host rates with 4x internal clock |
| `d6df36a` | Record rebased validation and inconclusive listening results | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `1795193` | Retain envelope branch targets and progress across retriggers and time edits | Core DSP behavior / numerical reference assertions | implemented residual branch targets/retrigger/live timing; independent trajectory, clock, slope and partition assertions |
| `39999ec` | Record validation of the residual-state envelope update | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `00af2f2` | Keep macOS test failure diagnostics bounded | Test organization, bounded numerical checks and diagnostics | implemented bounded CI failure summaries with full JUnit/log artifacts |
| `46e2e33` | Preserve original VCA arithmetic with the note gate fully open | Core DSP behavior / numerical reference assertions | implemented frozen open-gate gain expression and exact empirical stage regressions; native compiler/ARM builds in CI, runtime deferred |
| `babe156` | Keep gate corrections separate from the original EG expression | Core DSP behavior / numerical reference assertions | implemented independent note gate preserving EG gain expression; gain/depth/release and analytical transfer regressions |
| `186c362` | Fix exclusive GUI choice switches and parameter synchronization | Native IGraphics controls, polling and lifecycle assertions | implemented exclusive IGraphics choices and shared RES controls; native repaint/host-gesture validation deferred |
| `2c6cf66` | Organize documentation and keep references entirely local | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `ed94aa4` | Update architecture and circuit documentation for current DSP behavior | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `8e4f4ad` | Describe current behavior and hardware evidence without future work plans | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `826b5d8` | Separate software documentation from hardware analysis | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `30b400b` | Correct documentation claims against current implementation and recorded evidence | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `a76cc90` | Make documentation readable without external source PDFs | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `8eef56b` | Improve documentation coverage, current specification navigation and evidence scope | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `851677f` | Document waveform generation from shared square and phase source | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `2044325` | Document filtered noise generation and live waveform/source switching | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `e9c0049` | Translate stability audit documentation into English | Validation evidence / current documentation; audit applicability | reviewed historical measurement/documentation provenance; archived JUCE software evidence and retained hardware analysis, current iPlug2 claims documented separately |
| `129b9e9` | Shorten DSP regressions and split CI test stages | Test organization, bounded numerical checks and diagnostics | adapted bounded measurement windows and CTest correctness/observation groups; JUnit and bounded logs in CI |
| `32b91c8` | Limit test optimization to bounded period measurements | Test organization, bounded numerical checks and diagnostics | retained bounded 65-crossing/4-second period measurements; Debug stays unoptimized and Release uses normal configuration |


## Assertion reconciliation outcome

The [inventory](iPlug2-test-inventory.csv) records 113 adapted core contracts,
62 retained/replaced core assertions, 16 portable observations and six previously
replaced cases. 27 JUCE internal cases, six mock-only tone tests, two old binary
compatibility cases and one Linux probe are retired with explicit reasons.
27 native-runtime and 12 native-UI cases remain deferred. There are no unreviewed
or partially classified rows. `scripts/check-test-inventory.py` enforces frozen
provenance/coverage and evidence paths in CI; it is not semantic equivalence proof.

The complete main Tests/Source/release references remain under Tests/legacy-juce;
historical software/audit documentation is under docs/legacy-juce and hardware
analysis under docs/hardware. Existing project licensing and SDK/plugin identities
are retained. No old project/automation/binary-state adapters are restored.

The active 197-case portable suite contains independent frozen filter arithmetic,
empirical coupling/gain expressions, analytical VCA transfer and precision bounds,
all waveform/Feet pitch measurements, PWM/glissando timing schedules, residual EG
trajectories, MIDI mappings/offsets/panic, state/preset identity and real-time
publication/ownership probes. Eight simulated SDK-interface cases add native
queue/stream boundary coverage without instantiating a product.

The frozen filter reference exposed float subtraction of damping endpoints in
the port. Double subtraction restores the former formula's precision; four-rate
bit comparisons verify the behavioral models. Original/VCA whole-wrapper JUCE
bit equality is intentionally replaced: the independent Biquad/oscillator/random
primitives and 129-tap output FIR differ. Independent transfer/response and timing
limits, not old complete-graph samples, are the relevant current contracts.

Direct synthesis is always at four times host rate. Removed JUCE interpolation,
fixed/legacy/dual-filter graph timing and cached block-renderer optimization
experiments remain archival; they are not current native performance guarantees.
Future MIDI events carry to the next callback instead of old beyond-block clamping.
Continuous upstream sources are not expected to be silent when inactive; the
engine/VCA gate determines output silence.

Optional onset WAV/CSV export and separate research equations are active. Fifteen
Python archive/signing fixtures cover package integrity, source identity and safe
opt-in command preparation; four conditional hardware equation checks retain the
old Python algebra work. Source ZIPs are rebuilt offline in CI. Real SDK callback
locks/allocations, native gestures/repaints/dialogs, DAW loading/latency and standalone
device/preferences interactions remain runtime gates; no core mock substitutes.
