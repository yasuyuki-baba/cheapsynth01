# Note onset continuity

## Finding and implementation

Previously the production VCO sent zeros while its generator was inactive.
Note-on resumed the waveform and injected a step into the continuously running
VCF and VCA input-coupling stages. The 25% pulse has a nonzero average, making
this gating particularly liable to excite coupling/filter transients. A sampled
110 Hz pulse example switched from zero to about 0.795 at note-on. This is a
software discontinuity, not evidence of the CS01 hardware's keying behavior.

The production graph now enables free-running tone/noise generation. Oscillator
phase, waveform state and upstream coupling/filter state continue while the VCA
is silent. The generators' MIDI held/release bookkeeping remains separate;
component users can still use the existing gated VCO mode.

To avoid output leakage at VCA EG depth below one, EGProcessor also produces a
sample-wise note gate in a preallocated buffer. The existing EG audio connection
orders EG before VCA, whose pointer to the EG remains valid for that graph's
lifetime. Blocks must fit the prepared internal capacity, as enforced by the
top-level segmented renderer. No audio-thread allocation is added.

VCA control is now `(1-depth)*noteGate + depth*EG`. The additional note gate
ramps up over 1 ms and releases linearly over the configured release duration.
Retrigger and release-time edits start from its current level; release edits
use the EG's remaining endpoint time so both gates finish together. This is an
implementation policy, not a reconstructed circuit. At full EG depth, the gate
does not affect gain. The subsequent [residual-state EG update](EG-stateful-model.md)
shortens retriggered attacks and preserves progress during time edits; fresh
stage curves retain their existing provisional shape. Below full depth,
note-off now fades the non-EG gain rather than
holding it until the oscillator's release deadline and abruptly cutting it.
All Sound Off and lifecycle reset clear the gate immediately.

Idle VCO processing now costs CPU and changes onset phase/PWM state. The
original EG depth, saturation, coupling and filter calibration remain provisional.
An immediate note at preparation can still encounter initial filter state;
this change does not claim every first-start transient is eliminated.

## Verification and listening material

`NoteOnsetTest` compares a released/retriggered free-running tone with a held
reference at three host rates and all five waveforms, checks both filters and
tone/noise sources for output silence at EG depths 0, 0.4 and 1, and checks
sample-wise gate consistency across block sizes, retrigger and release edits.
An additional noise test covers a release deadline inside a block, where the
gated rendering API still stops but continuous graph rendering must fill the
remaining samples. The current optimized modulated tone renderer and fixed
audio-owned routing are retained.
Existing envelope fixtures now respect their prepared block capacity: automation
prepares its largest block (20 ms), and the held-key MIDI test renders its
one-second waits in blocks of at most 256 samples.

Optional observation tests export whole-graph 48 kHz / 24-bit WAVs and
sampled 192 kHz internal VCO/EG/VCF/VCA trajectories:

```bash
CHEAPSYNTH_ONSET_OUTPUT=/absolute/output/directory \
  build/Tests/CheapSynth01Tests_artefacts/Debug/CheapSynth01Tests \
  --gtest_filter=NoteOnsetTest.Observation_* \
  --gtest_output=xml:/absolute/output/directory/results.xml
```

The comparison material uses note 45 (110 Hz), attack 1 ms, decay 37 ms,
sustain zero, release 199 ms, cutoff 962 Hz, high resonance, VCF EG depth 0.25,
VCA EG depth one, volume 0.7, and no breath or external LFO modulation. It is a
controlled diagnostic patch, not an exact factory preset or matched video patch.
Wave indices are triangle=0, saw=1, square=2, pulse=3, PWM=4.

`artifacts/dsp/note-onset/comparison-wave-3.wav` and `comparison-wave-4.wav`
contain four pre-change notes, 400 ms silence, then four changed notes, with no
independent normalization. These archived captures describe the initial
implementation on baseline `4f2e59d`, before integration with newer `main`
changes; they do not capture the subsequent integrated implementation. The optional
capture tests can export audio and trajectories for the current implementation.
`onset-stages.png` shows the pulse's upstream step
and the changed continuously running signal. `boundary-measurements.json`
records the samples around note-on. The run also wrote raw WAV/CSV captures to
ignored `build/note-onset/before` and `build/note-onset/after` directories; those
files are not tracked and are not required to read the archived results.

The measured upstream gate discontinuity is removed. The shortest EG attack
still creates an amplitude transient; overall onset high-frequency energy is
phase/timbre dependent and did not decrease for every waveform in these captures.
Whether the user's particular audible click is resolved has not been
established by listening with the affected patch. Initial listening was inconclusive
and did not establish a consistent audible improvement. No matched hardware
measurement or direct audio analysis of the linked YouTube videos is claimed.

Before integration with newer `main`, all 223 registered cases were covered
successfully across the initial passing
prefix, the continuation after the fixture correction, and the separately
executed capture tests. Standalone compilation, self-contained header checks,
pinned clang-format 21.1.7, `git diff --check`, and the four independent EG decay
algebra checks passed. `artifacts/dsp/note-onset/verification.json` records this
coverage and the outstanding listening/hardware limitations.

After rebasing onto `main` at `5f4053a`, all 265 cases passed in a single run,
including both capture tests. The Standalone build, header checks, pinned
formatter and four EG algebra checks also passed. The tested source commit
is `b914171`; subsequent documentation records the inconclusive listening result.

After the residual-state EG update (`cb5d4a6`), all 270 cases passed, including
both optional capture tests. That run wrote diagnostic captures to the ignored
`build/note-onset/residual-eg-final` directory; those files are not tracked in
the repository. Archived comparison WAVs above
still describe the initial implementation; no listening result for the EG update is recorded.

macOS CI exposed a rounding regression in the fully open VCA path: adding the
gate product changed floating-point contraction and broke the existing exact
pre-refactor comparison. The fully open gate now uses the original expression;
moving gates apply a correction only to the non-EG gain. Keeping the original
EG expression outside the gate branch also prevents optimized builds from
hoisting its product and changing contraction. A standalone comparison with
pre-change source under GCC `-O2`/`-O3 -mfma -ffp-contract=fast` found zero bit
differences across 707,707 fully open samples. The moving-gate equation check
stayed bounded, muted exactly at EG/gate zero, and differed from its independent
double-precision solution by at most 4.31e-8 across another 707,707 cases.
The bitwise regression keeps its original strict comparison.
All 17 focused VCA/structure and onset regressions
passed locally after this correction; Standalone/header builds and formatting
also passed. CI retains full macOS output as an artifact and prints bounded
failure diagnostics to avoid per-sample assertion floods.
