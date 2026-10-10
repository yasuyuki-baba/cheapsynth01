# Stability audit and implementation results (2026-10-09 JST)

Each section records measurements at its stated date and source revision; not
all sections describe the current implementation. Current architecture is in
[DSP responsibility boundaries](DSP-responsibility-boundaries.md),
[the EG model](EG-stateful-model.md), and
[note onset continuity](Note-onset-continuity.md).

The latest GitHub main at the start of the audit was
`4f2e59d722ca2c761ce7a72d2f6ee719bcc9e900`, identical to the audit target.
A stale local origin/main or an existing build was not used as the baseline.

The initial measurements and tests below used source
`ec12e588f57b78fc199a001418a632463f2aa626`.
The main implementation was a280bb7, the cache-restoration follow-up was
9db9a2c, and test parameter types were corrected in ec12e58.
The initial reporting/packaging commit did not change production DSP.
Subsequent corrections are recorded in separate sections below.
Machine-readable conditions, counts and log hashes are in
[stability-results.json](../../artifacts/audit/stability-results.json).
XML timestamps are UTC; dates in this report are JST.

## Reproduction and corrections

| Finding | Diagnosis on the audited main baseline | Correction and direct verification |
|---|---|---|
| 1 UI notifications | Choice notifications outside the message thread directly modified the UI. The baseline reproduction test also failed. | Notifications only set an atomic dirty flag. A message-thread Timer reads current values at 60 Hz (the existing Modulation timer uses 120 Hz). GUI clicks apply immediately on that thread. Worker notifications, latest values and notifications after destruction were tested. |
| 2 Save/Overwrite | Raw this/manager captures were confirmed in production code. The earlier audit's abnormal termination had no identified cause and was not treated as proof of a UAF. | Standardized on panel-owned AlertWindow and SafePointer. Editor closure was tested at 4 stages: during entry, pending entry completion, during confirmation and pending confirmation completion. |
| 3 Release lifetime | At 48 kHz, Note Off at 50 ms followed 20 ms later by a change to 1 second stopped the source before the envelope. Baseline reproduction failed. | Pass the EG's remaining internal sample count to the source. Extension, shortening, repeated edits and source switching were tested with Tone/Noise, depths 0/1 and partitions 1/7/64, checking agreement between EG and source activity. |
| 4 MIDI/Noise boundaries | Reproduced lost events at position N and in 0-sample blocks, and Noise rendering past a block boundary. | Normal positions are 0..N-1. Out-of-range positions clamp to 0 or N; N events apply after rendering. Empty blocks apply only host events without advancing DSP. Tested -1/N-1/N/N+1, same-position On/Off/CC120 and ordering with mixed SysEx. Noise generates only the remaining active samples. |
| 5 Presets | Confirmed paths that returned no save result and committed selection before loading. | Portable single-filename validation, bool success results, temporary-file replacement and selection only after success. Corrupt XML, deleted files and a nonempty directory at the save destination preserve existing data/selection. |
| 6 Realtime processing | Confirmed segment/panel MidiBuffer storage, long-message copies, collector/keyboard locks and notification posting. Additional probes found initial allocations in JUCE graph scratch storage and IIR processing. | Non-owning host-event traversal, reusable storage, a fixed MPSC queue of 2048 short events and a reverse keyboard-display queue. Full/contended queues request panic to prevent stuck notes. Re-preparation discards old generations; concurrent send/receive/reprepare tests were added. |
| 7 Programs/routing | JUCE VST3 ProgramChangeParameter and LV2 state paths call setCurrentProgram, so message-thread-only use could not be assumed. Graph changes were already deferred to the message thread, not performed directly on audio. | Apply values on the next callback from an immutable Factory/User catalogue parsed outside realtime processing. Notify/reclaim on the message thread. Preserve selection by filename/type. Tested concurrent updates, 7 Factory programs, User caches and invalid XML updates. Synchronize stale APVTS adapter caches before UI/session restoration. |
| 8 Numerical recovery | NaN/Inf into Original input coupling prevented recovery on subsequent finite input. The baseline test failed; NaN generation during normal playing was not established. | Reset input coupling, model and output coupling together. VCA also resets internal state on invalid audio/EG and prevents nonfinite depth from contaminating smoothing. Compare recovery with a fresh instance and check nonzero finite output. |

All **7 of 7 freshly compiled baseline cases failed as expected**.
They reproduced boundary MIDI, same-position order, Release, Noise boundaries,
Original recovery, VCA recovery and UI notifications.
No test attempted to crash the baseline save GUI. The initial abnormal
termination from a mixed stale build was excluded as an invalid reproduction.

## Initial correction validation

| Configuration | Count | Result and limits |
|---|---:|---|
| Debug --all | 238 | failures/errors/disabled = 0 |
| Release --all | 238 | failures/errors/disabled = 0 |
| project ASan + UBSan + LeakSanitizer --all | 236 | All passed, no detections in logs. Instrumented 52 production/test cpp files. JUCE/GoogleTest module objects were not instrumented. Excluded 2 ELF allocation/lock probes. |
| Independent Release RT/whole-graph measurements | 4 | All passed, exit 0. Other builds/tests were stopped during measurement. |
| Python distribution validation | 8 | Synthetic ZIP cases including valid assets, missing CLAP, missing notices and version/commit mismatches. |
| EG equation checks | 4 | Passed; not production DSP or hardware measurements. |
| header / format | Formatting of 102 C++ files, check_headers | Passed with clang-format 21.1.7; diff whitespace checks also passed. |

Intermediate failures were not removed or weakened. The first two full-suite
failures (initial allocation and concurrent source switching) passed after fixes.
An additional User-cache-to-Factory test exposed adapter-cache inconsistency;
this was corrected for session restoration as well.
The initial sanitizer run stopped after 65 passing cases at an invalid Bool-to-Float
downcast in an existing Original VCF test. The VCO glissando fixture also used
Bool as a substitute, so both fixtures were aligned with production Float types.
Cases and acceptance criteria were retained. Earlier High-setting observations
from that VCF test are not treated as valid production measurements.
Windows/macOS, real DAWs/audio devices, full JUCE instrumentation and
ThreadSanitizer were not run.

Reproduction commands (dependencies are documented in CONTRIBUTING.md):

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DSTANDALONE_ONLY=ON -DCOPY_PLUGIN_AFTER_BUILD=OFF
cmake --build build --target CheapSynth01Tests check_headers --parallel 2
xvfb-run -a bash scripts/run-linux-gui-tests.sh build/Tests/CheapSynth01Tests_artefacts/Debug/CheapSynth01Tests --all
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DSTANDALONE_ONLY=OFF -DCOPY_PLUGIN_AFTER_BUILD=OFF -DCMAKE_SKIP_RPATH=ON
cmake --build build-release --parallel 2
xvfb-run -a bash scripts/run-linux-gui-tests.sh build-release/Tests/CheapSynth01Tests_artefacts/Release/CheapSynth01Tests --all
python3 Tests/tools/build-project-sanitized.py --build build --output build-sanitized
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 xvfb-run -a bash scripts/run-linux-gui-tests.sh build-sanitized/audit-sanitized --all
python3 Tests/release_validation_test.py -v
python3 Tests/eg_decay_equation_check.py -v
```

The environment was Debian 13, GCC 14.2.0, CMake 3.31.6, JUCE 9.0.3,
Linux 6.18.44, x86_64. AMD EPYC 9V74 exposed 3 logical CPUs with a cgroup
allocation of 2 CPUs. GUI tests used Xvfb/Openbox.
The same pinned JUCE/GoogleTest sources already fetched in another checkout
were supplied through FetchContent source overrides.
JUCE originals were not edited; a CMake recipe with verified context generated
a graph-scratch-reuse override inside the build. Upgrades require re-auditing.

## Realtime and switching measurements and remaining limits

At 48 kHz with the whole graph at 4x, Note On/CC/Off occurred at each position,
programs changed every 8 blocks, and GUI operations ran concurrently.
Each block size used 1000 callbacks in a single observation on a normal-priority
std::thread.

| Block size | p95 µs | p99 µs | Maximum µs | Program-change p99/maximum µs | Period µs |
|---:|---:|---:|---:|---:|---:|
| 1 | 4.447 | 6.179 | 95.405 | 5.649 / 11.277 | 20.833 |
| 16 | 123.096 | 224.560 | 1806.780 | 175.305 / 280.215 | 333.333 |
| 64 | 373.144 | 530.893 | 1152.950 | 469.410 / 537.763 | 1333.333 |

Maximum times exceeded the period for 1/16-sample blocks. Average CPU cost
is not a deadline guarantee.
Dense MIDI across 100×64 callbacks and probes for first-use, variable-length
and mixed SysEx processing detected 0 allocations/frees within probe coverage.
Mutex acquisitions totaled 44,900 (44,800 graph + 100 program thread checks).
This does not cover all shared-library allocations, aligned allocation or the
complete host wrapper's realtime behavior.
JUCE 9.0.3 MessageManager thread checks also use a mutex. Substituting an
unprotected getter was rejected because it would introduce a race.
Host notifications through standard JUCE attachments still posted AsyncUpdater messages.

Filter/LFO routing used Timer-driven graph rebuilding on the message thread;
it was not sample-accurate switching.
Held-note switching blocks had maximum adjacent-sample difference 0.170279
and overall peak 0.317756, without a calibrated audibility threshold.
Adding nonzero input to both filters in the current graph increased the
64-sample median from 114.574 to 117.789 µs (about 2.8%).
This was not a completed fixed-graph-plus-selector/crossfade comparison.
At this audit stage, switching state and additional-node CPU/compatibility
were unverified, so the graph was retained.
The VCO then used a single-sample API and repeated parameter updates;
post-change measurements are recorded later below.

## Compatibility, distribution and documentation

Parameter IDs/version hints, XML structure/units, Factory resources and
live-control exclusions were retained. DSP structural regressions checked
bitwise agreement for normal finite inputs. Source-lifetime extension/shortening
and boundary corrections intentionally changed sound behavior.
Programs requested outside the message thread applied on the next callback;
GUI notifications ran at 60 Hz. External User-file edits reached the cache on
list refresh. New saves/renames reject Windows-reserved names and similar invalid
names, but existing files are not excluded from loading by these naming rules.
The Original VCF multichannel API shares one state. The production mono contract
was documented; independent polyphony/multichannel state was not added.

Windows CLAP packaging was added, with checks for tag/product version, all
assets, notices and source-commit consistency.
Actual Linux products were Standalone/VST3/LV2/CLAP, ELF x86_64, with local
dynamic dependencies resolved. Symbol requirements were GLIBC 2.38,
GLIBCXX 3.4.32 and CXXABI 1.3.15. Older Linux compatibility was not tested.
Workspace-absolute RPATHs were removed. Windows/macOS CPU/OS minima and
signing/notarization were unverified. Corresponding-source, licence, file-hash
and runner-information packaging is described in [Distribution.md](Distribution.md).

Existing CSVs were already distinguished as ideal references or historical
experiments in [dsp-baselines.md](dsp-baselines.md). That classification was
retained; saved-content hashes and historical commits were added to
[provenance.json](../../artifacts/dsp/provenance.json).
Historical commits do not prove the source revision used for measurement;
unrecorded measurement conditions/toolchains remained null.
New production-graph measurements were saved separately with this audit's
commit, environment and commands. The earlier VCO-only oversampling explanation
was updated to the current whole-graph 4x policy.

LICENSE remained GPLv3. The JUCE 9 AGPLv3/commercial licensing route requires
the author's decision: combined AGPL distribution under GPLv3 §13 or an
applicable JUCE commercial licence. Commercial licence ownership was not
assumed. Notices and corresponding-source packaging alone were not treated
as confirmation of legal compliance.

Hardware fidelity was not claimed. Calibration requires an identified CS-01
unit/revision, supply/temperature/component condition, signal level, input/output
loading, breath control voltage, waveform/feet/EG settings, recording bandwidth,
sample rate/uncertainty and correspondence between raw recordings and measurement
points. Tolerances, unit variation and control-curve references remain unresolved
without hardware evidence.

## Follow-up correction: host program path (2026-10-09 JST)

The follow-up source was `904da992198ef7bd93e8e050faea34a286fcb540`.
Initial values and measurements remain records of the earlier commit.
Follow-up results are in
[program-rt-followup.json](../../artifacts/audit/program-rt-followup.json).

Removed the MessageManager mutex acquired by thread checks in `setCurrentProgram`;
all host requests reserve entries in the existing immutable cache.
GUI selection and loading after save use ProgramManager's explicit non-realtime
path, preserving actual-file validation and immediate loading.
JUCE VST3 ProgramChangeParameter skips requests for the current program, so the
reserved selection is returned immediately. Audio values apply on the next
callback, including empty blocks. Tests covered reservation cancellation back
to the original program, latest valid request, invalid indices and session save
before application. Saving snapshots reserved values and identity together
without changing the format. Successful UI/session loads cancel prior reservations.
Audio-side CAS protects new requests arriving during application.

Final Debug **242/242**, Release **242/242** and focused project
ASan/UBSan/LeakSanitizer **29/29** passed. The full sanitizer suite was not
rerun in this follow-up. Uninstrumented JUCE and other dependencies, and the
initial full 236-case sanitizer validation, belong to the earlier commit.
The existing User-cache test's pre-application metadata expectation was aligned
with reserved selection; checks that cutoff and source mode remain unchanged
before application were added instead. Cases and audio assertions were not
removed. An initial Xvfb startup failure was recorded as an environment-startup
failure resolved by rerunning.

Across both threads, 204 standalone host requests performed 0 allocations,
frees or mutex acquisitions. The same dense 100×64 callback test reduced
mutex acquisitions from 44,900 to 44,800. The remaining acquisitions belong
to JUCE's graph; this is not full realtime safety or a deadline guarantee.
This follow-up did not change parameter IDs/version hints, XML format, DSP
timbre or control curves.

## Follow-up correction: package integrity validation (2026-10-09 JST)

Validation source was `0ec2ab1`. Details are in
[package-integrity-followup.json](../../artifacts/audit/package-integrity-followup.json).
The previous validator accepted substituted contents with valid CRCs.
A baseline case expecting rejection failed; the same case passed after correction.

Product and corresponding-source manifests now list SHA-256 for all files,
checked against actual contents. Coverage must be complete except for the
manifest itself; missing/extra files, invalid hashes, duplicate paths and ambiguous
manifests are rejected. Hash paths use `/` on Windows too. The final tag job
passes its checked-out commit, rejecting a complete older asset set whose version
and internal commits agree with each other. Python regressions were added to CI lint.

Python **22/22** passed. All hashes were verified across **5 ZIP** archives:
4 actual Linux product formats plus corresponding source. A negative case
requiring another source commit rejected the actual ZIP set as expected.
Synthetic Windows/macOS fixtures were not treated as successful real-OS products;
GitHub Actions itself was not run. Production C++ and DSP tests were unchanged
from 904da99, so additional C++ regression runs were not performed.
Hash agreement establishes package consistency, not signing or reproducible builds.
LICENSE, production DSP and preset format were unchanged; nothing was pushed
or published during that audit phase.

## Follow-up correction: VCO optimization after measurement (2026-10-09 JST)

Production/test source was `386eded41921fbe94a76d4d2b09b7b060602e0cb`.
The comparison baseline was `f42b3a35c8161cf323b9aa42cb874dc8b584ac83`.
The old Release executable for whole-graph comparison was built from 904da99;
production C++ was unchanged through f42b3a3.
Conditions, counts, hashes and three runs of measurements are in
[vco-optimization.json](../../artifacts/audit/vco-optimization.json);
measurement logs are in [vco-performance.log](../../artifacts/audit/vco-performance.log).

Repeated per-sample parameter-name searches and RTTI were replaced with
parameter-object references bound at construction. Values are read at the same
points as before; automation values are not frozen per block.
Single-sample Tone API calls were combined into continuous rendering, retaining
the order of LFO/PWM/glissando advancement, active/tail decisions, addition and
signed-zero handling. Parameter IDs, version hints, XML format and control
curves were unchanged.

Comparison with the frozen old Tone renderer established **bitwise agreement
across 480 conditions and 3,932,160 float values**. Coverage included
44.1/48/96 kHz, internal/external oversampling, 5 waveforms, 4 feet settings,
1/7/64/255-sample partitions, continuous LFO, live pitch/bend/PWM/wave/glissando/
Release edits, Note Off/retrigger and graph-owned tails.
After ValueTree replacement by 7 Factory programs and session restoration,
bound objects were verified to supply current values. The frozen reference
shares YM10150/WaveformStrategies; it is not an independent oracle for those models.

| Configuration | Count | Result and limits |
|---|---:|---|
| Debug --all | 245 | failures/errors/disabled = 0 |
| Release --all | 245 | failures/errors/disabled = 0 |
| project ASan/UBSan/LeakSanitizer --all | 242 | All passed, no detections. Instrumented 54 project/test cpp files. JUCE/GoogleTest/system were not instrumented; excluded 3 ELF allocation/mutex probes. |
| Independent Release observations | 2 old + 6 new cases × 3 runs | All passed. Old then new ran sequentially, with other project builds/tests stopped. |
| header / format | Debug/Release, 105 C++ files | Passed with clang-format 21.1.7; diff whitespace checks passed. |

The first comparison ran alongside a Debug build. Audio-equivalence checks
remained valid, but those timings were excluded from performance conclusions.
The ranges below cover three repeated measurements on shared Linux virtual CPUs,
normal priority, with a cgroup allocation of 2 CPUs.
Settings were 48 kHz host / 192 kHz processing, PWM and LFO depth 0.73.
Each trial used 32 warm-up + 1000 measured blocks, alternating old/new order.
VCO-only medians decreased by approximately 74–77% in each condition.
p99/max include scheduling and other effects; they are not deadline guarantees.

| Glissando | Host block | Old median µs | New median µs | New p99 µs | New maximum µs |
|---|---:|---:|---:|---:|---:|
| Fixed pitch | 1 | 1.563–1.573 | 0.370–0.390 | 0.551–0.581 | 16.966–286.803 |
| Fixed pitch | 16 | 20.511–20.592 | 5.268–5.278 | 18.378–44.838 | 80.541–2790.180 |
| Fixed pitch | 64 | 81.172–81.423 | 20.932–20.991 | 79.580–96.105 | 248.736–2554.170 |
| In progress | 1 | 1.642–1.653 | 0.381–0.381 | 0.471–0.591 | 0.570–16.145 |
| In progress | 16 | 21.683–21.883 | 5.368–5.398 | 42.474–68.814 | 178.980–353.033 |
| In progress | 64 | 86.059–86.410 | 21.282–21.312 | 76.164–151.329 | 344.269–1499.360 |

Existing whole-graph spectrum/processing tests were reused.
The following ranges are milliseconds to process one audio second, not callback
deadline measurements. Folded-bin values in the 6 recorded conditions were
identical for old/new; this does not establish hardware timbre fidelity.

| Host Hz | Waveform | Old ms/audio-second | New ms/audio-second |
|---:|---:|---:|---:|
| 44100 | 1 | 88.221–115.760 | 41.035–42.529 |
| 44100 | 2 | 83.345–107.028 | 38.134–40.379 |
| 48000 | 1 | 97.203–115.417 | 43.537–46.121 |
| 48000 | 2 | 94.793–114.520 | 40.950–45.569 |
| 96000 | 1 | 196.687–212.316 | 84.484–102.480 |
| 96000 | 2 | 190.612–228.507 | 80.462–107.000 |

New implementation results from the existing whole-graph test with dense MIDI,
concurrent GUI operations and program changes (1000 callbacks per block size):

| Host block | p95 µs | p99 µs | Maximum µs | Program-change p99 µs | Program-change maximum µs |
|---:|---:|---:|---:|---:|---:|
| 1 | 3.445–3.716 | 14.382–36.014 | 210.662–318.700 | 18.317–20.511 | 54.082–210.662 |
| 16 | 88.934–117.627 | 154.573–602.970 | 351.145–1777.180 | 127.532–229.196 | 179.330–426.333 |
| 64 | 251.830–408.206 | 384.190–1015.980 | 516.900–18019.600 | 279.461–619.495 | 338.140–877.714 |

At 48 kHz, deadlines for blocks 1/16/64 are 20.833/333.333/1333.333 µs.
Each block size exceeded its deadline in some runs; no realtime deadline
guarantee was achieved. GUI edit counts depend on wall-clock time and vary
between runs, so GUI load was not strictly paired.
The 100×64 callback probe recorded 0 allocations/frees and 44,800 mutex acquisitions.
JUCE graph mutexes, APVTS attachment notification posting and message-loop-dependent
routing remained unresolved at this stage.
Windows/macOS, real DAWs/devices, full JUCE instrumentation, TSan and hardware
calibration were again not run.

To reproduce, use the built Release test executable, stop other builds and
execute the following three times sequentially:

```sh
xvfb-run -a bash scripts/run-linux-gui-tests.sh \
  build-release/Tests/CheapSynth01Tests_artefacts/Release/CheapSynth01Tests \
  --all --gtest_filter='VcoOptimizationTest.*:VcoOptimizationObservationTest.*:WholeGraphObservationTest.*:AuditRealtimeTest.*'
```

The old whole-graph comparison uses the same WholeGraphObservationTest filter
on a Release executable built from baseline source. The paired VCO comparison
within one process uses the frozen reference and can run with the latest
executable alone.

## Follow-up correction: remove UI posting from host notifications (2026-10-09 JST)

The baseline was `57e9b300617696efab8246029702e2fbc885b874`;
production/test commit was `5ac834fa4163c8f7e9e7905f6ce257a902deead9`.
Details are in [polling-notifications.json](../../artifacts/audit/polling-notifications.json),
with measurements in [polling-notifications.log](../../artifacts/audit/polling-notifications.log).

JUCE 9.0.3 ParameterAttachment calls `isThisTheMessageThread()` and
`triggerAsyncUpdate()` outside the message thread.
Running the same worker regression with standard JUCE Slider/Button/ComboBox
attachments produced **1 allocation, 0 frees and 6,003 mutex acquisitions** for
3,000 notifications. One case requiring 0 allocations failed as expected.
This was an independently built probe with bindings replaced by standard JUCE
types, not a rebuild of the entire old main or an old whole-graph comparison.

Production Slider/Button/ComboBox bindings were replaced with message-thread-owned
polling. They register no parameter listener, read initial values at construction
and then read current parameter values at 60 Hz. Audio-side message posting or
thread checks were not added.
Ranges, custom skew/snapping, text conversion and double-click defaults were
compared with JUCE 9.0.3. GUI operations immediately notify the host, preserving
drag gestures, complete Button/ComboBox gestures, APVTS UndoManager and gesture
completion when destroyed during a drag. External changes appear at approximately
16.7 ms intervals, or later when the message loop is busy; DSP does not wait.

After a GUI write, the observation cache is invalidated so the latest value is
shown even if the host returns to the previously observed value before the next
tick. Silent MIDI values are also observed directly.
Tests directly covered worker-notification coalescing, no GUI access from workers,
destruction during concurrent automation and destruction before a tick.

An intermediate implementation's first host-notification probe also detected
3 allocations with the GUI closed. APVTS feet/filterType/lfoTarget ListenerList
iterator vectors grew on first notification. Since feet was already checked on
audio, redundant VCO/processor APVTS registrations were removed.
Routing changes used the processor's existing message timer to read current
filterType/lfoTarget. The graph was not made fixed at this stage; the existing
message-loop-dependent switching contract was retained.

| Check | Count | Result and limits |
|---|---:|---|
| Debug --all | 252 | failures/errors/disabled = 0 |
| Release --all | 252 | failures/errors/disabled = 0 |
| project ASan/UBSan/LeakSanitizer --all | 247 | No detections. Instrumented 55 project/test cpp files. JUCE/GoogleTest/system were not instrumented; excluded 5 ELF probes. |
| Focused GUI/format/RT regressions | 17 | All passed, including 5 new GUI and 2 RT cases. |
| Independent Release RT/whole-graph observations | 7 | All passed after other project builds/full regressions completed. |
| header / format | Debug/Release, 107 C++ files | Passed with clang-format 21.1.7; diff whitespace checks passed. |

The new binding alone recorded **0 allocations, 0 frees and 3,000 mutex
acquisitions** for 3,000 worker notifications. The remaining 3,000 are JUCE
AudioProcessorParameter notification locks.
The processor probe changing every parameter 100 times also recorded
**0 allocations/frees, including the first notification**.
Its 4,530 mutex acquisitions were identical with GUI open/closed; the editor
added 0 acquisitions. This probe does not establish a lock-free audio callback.

Observations from the existing whole-graph dense-MIDI/GUI-edit/program-change
test (1000 callbacks per block size):

| Block | p95 µs | p99 µs | Maximum µs | Program-change p99 µs | Program-change maximum µs | Deadline µs |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 2.994 | 4.788 | 78.318 | 4.487 | 19.36 | 20.8333 |
| 16 | 59.07 | 116.826 | 267.795 | 102.755 | 176.287 | 333.333 |
| 64 | 238.931 | 1106.57 | 2229.82 | 349.168 | 2210.77 | 1333.33 |

Maximum times for blocks 1/64 exceeded their deadlines. This was one run,
without claims of deadline guarantees or reliable percentile estimates.
Wall-clock-dependent GUI edit counts did not exactly match earlier measurements,
so no CPU improvement percentage was inferred from the notification change.
The 100×64 callback probe retained 0 allocations/frees and 44,800 mutex acquisitions.
JUCE graph locks and host-notification locks remain.
Folded-bin values for the 6 recorded spectrum conditions matched the previous
run; this does not establish hardware fidelity.

Two of the 16 intermediate focused cases failed. An incorrect oracle compared
Slider proportions directly with snapped parameter values; it was corrected to
compare against a standard JUCE Slider, and float-precision expectations were
aligned with actual values. The other first-use APVTS allocation was fixed by
removing registrations as described above. Cases/assertions were not removed.
The initial Debug full-regression launch overlapped with linking and returned
permission denied (exit 126). It was excluded as a pre-test launch failure;
all 252 cases were rerun after the build completed.
Intermediate compile/format issues were also corrected; successes from stale
executables were not added to the final results.
Windows/macOS, real DAWs/devices, full JUCE instrumentation, TSan and hardware
calibration were not run.

The reproduction filter is `PollingAttachmentTest.*:PollingAttachmentRealtimeTest.*`.
Full regressions use the earlier Xvfb commands with `--all`.
This follow-up also retained parameter IDs/version hints, saved XML, DSP equations
and control curves. Notification design and lifetime contracts are in
[MIDI-realtime-control.md](MIDI-realtime-control.md).

## Follow-up validation: audio-side filter/LFO routing selection (2026-10-10 JST)

The baseline at this stage was `0de5c948297bf0ff15eadc9185c0fb5cb30aaca7`.
The original audit target/main at startup remained
`4f2e59d722ca2c761ce7a72d2f6ee719bcc9e900`.
Baseline routing changes were delayed graph changes from the message timer,
not direct audio-side changes. A reproduction that changed choices on a worker
and rendered without servicing the message loop failed 1 baseline case
(covering 2 actual choice values), while 2 existing observations passed.

The existing graph was retained with fixed connections. At the start of each
host callback, after applying a pending program, current filterType/lfoTarget
values select audio-owned input/output/LFO masks. This also applies to
zero-sample callbacks. The GUI's 60 Hz display timer does not determine sound
routing. The two choice reads are not one transaction and do not guarantee
switching at arbitrary samples within a callback.
Inactive filters advance with zero input and the existing EG sidechain, then
clear their output contribution to VCA, preserving the old disconnected-input
history. Both models continue processing. The whole graph was not removed;
no new nodes or crossfade were added.

The production change commit was `360b547c158e03a7b4f63c47285fa04286b52d55`.
Final test source was `3d72d88a0ef53bd09328e5784b4c6a3e6ce0d6f3`.
A test-only macro compared prior dynamic connections and a variant feeding
nonzero input to both filters. Each mode in 12 conditions (48 kHz host,
192 kHz internal, blocks 1/16/64, 2 filters × 2 targets) used 32 warm-up
callbacks followed by 1000 measurements, rotating trial order.
All 10 focused prototype cases passed. Normal CPU medians were comparable
with the old routing; medians increased for the dual-nonzero-input variant,
so fixed connections with inactive zero input were selected.
The old routing is not an additional product mode or parameter.

Output comparisons at 44.1/48/96 kHz, 2 filters × 2 targets and blocks 1/7/64
covered 36 conditions and 147,528 float values with exact numerical agreement.
This does not claim bitwise equality including signed zero.
Switches at matched callback times also gave difference 0 across 64,000 samples.
Observed peak=0.305222 and switch sample step=0.159228 preserve the old hard-switch
transient. Click removal, listening quality and hardware timbre fidelity were
not established.
Whole-graph checks covered Release extension/shortening, Tone/Noise switching,
repeated filter/LFO switching and block partitioning.
Parameter IDs/version hints, saved XML, product version, DSP equations/control
curves and LICENSE were unchanged.

| Check | Count | Result and limits |
|---|---:|---|
| Debug --all | 259 | failures/errors/disabled = 0 |
| Release --all | 259 | failures/errors/disabled = 0 |
| project ASan/UBSan/LeakSanitizer --all | 252 | No detections. Instrumented 56 project/test cpp files. JUCE/GoogleTest/system were not instrumented; excluded 7 ELF probes. |
| Independent Release observations | 9 × 3 runs | All passed, each run executed sequentially after all builds/regressions completed. |
| header / format | Debug/Release, 108 C++ files | Passed with clang-format 21.1.7; diff whitespace checks passed. |

An active-note probe switching every callback (blocks 0/1/16/64,
1000 callbacks) recorded **0 allocations, 0 frees, 5,250 mutex acquisitions,
0 observed contention and 0 ns wait** in each of three runs.
Host choice notifications were outside the callback probe.
Deliberate-contention controls detected 1 contention per run, with waits
of 501/922/281 ns. Acquisition counts and blocking waits are distinguished.
Existing dense-MIDI/GUI-queue callback probes also recorded 0 allocations/frees
and 44,800 mutex acquisitions in all three runs.

Representative paired comparisons (target=VCO, ranges of run medians over
three runs, µs):

| Block / filter | Old dynamic routing | Fixed, inactive zero input | Fixed, both inputs nonzero |
|---|---:|---:|---:|
| 64 / Original | 53.211–56.836 | 52.720–53.140 | 55.794–55.994 |
| 64 / Modern | 44.838–44.958 | 44.247–44.397 | 55.885–55.924 |

Median/p99/max for all 12 conditions × 3 modes × 3 runs are preserved in JSON/logs.
Medians alone are not converted to CPU improvement percentages or deadline
guarantees. Scheduler variation and high maxima remain.

Existing whole-graph dense-MIDI/GUI-edit/program-change measurements used
1000 callbacks per run/block. Each column gives minimum–maximum over three
runs, in µs:

| Block | p95 | p99 | Maximum | Program-change p99 | Program-change maximum | Deadline |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 3.034–3.735 | 4.537–51.808 | 107.673–293.934 | 6.230–24.367 | 32.409–77.427 | 20.8333 |
| 16 | 50.466–58.228 | 87.382–160.813 | 230.078–310.820 | 74.422–84.378 | 142.295–235.856 | 333.333 |
| 64 | 231.230–337.079 | 315.156–538.534 | 1172.280–2365.860 | 280.284–390.651 | 317.751–2365.860 | 1333.33 |

Maximum times exceeded deadlines in all three runs for block 1 and two runs
for block 64. GUI edit counts depend on wall-clock time and do not exactly
match the old measurements. Upper percentiles or means from short trials do
not establish realtime deadline guarantees. The 6 whole-graph spectrum
conditions were also recorded separately from hardware calibration.

One SessionGraphTest failed in each intermediate Debug/Release full run of
259 cases. The old expectation that inactive filters were physically disconnected
was replaced with a fixed-connection check. Assertions were added for restored
callback choice values and an output mask enabled only on the selected filter.
The case/assertions that held notes are not restored and sound begins with the
next Note On were retained and rerun. A missing OriginalVCFProcessor include
in the prototype build was also fixed before validation.
Failing cases or audio assertions were not removed, and stale-executable passes
were not treated as final validation.

While enabled, the Linux callback probe counts pthread trylock EBUSY as contention
and measures the wait for the subsequent actual blocking lock. Return-value
contracts such as EOWNERDEAD are retained. Ordinary CPU observations do not
enable this interposition. A control with a deliberately held mutex also detected
1 contention and a positive wait. Observed zero contention does not guarantee
that locks never wait in a host.
JUCE graph callback mutexes and host parameter-notification mutexes remain.
Graph connection editing was removed from automation, but host bus-layout
lifecycle updates remain.
Windows/macOS, real DAWs/devices, full JUCE/GoogleTest/system instrumentation,
TSan and hardware calibration were not run. JUCE commercial-licence ownership
and the distribution licensing route still require the author's decision.

Records and reproduction conditions are in `artifacts/audit/routing-selection.json`
and `.log`. Full regressions use the earlier Xvfb commands with `--all`;
focused tests use `RoutingSelectionTest.*:RoutingSelectionRealtimeTest.*`.
Performance observations ran independently three times after builds/full
regressions completed. Means/medians are not treated as realtime deadline
guarantees; p99/max are reported alongside them.
