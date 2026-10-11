# JUCE to iPlug2 migration

## Architecture

`CheapSynth01Core` uses standard C++20 and TinyXML2. `SynthEngine` owns the
oscillator, envelope, MIDI state, both filters, VCA and output converter. The
former JUCE graph is replaced by explicit LFO/EG → VCO → selected VCF → VCA
processing at four times the host rate. MIDI is applied before the corresponding
host frame. No runtime graph mutation or GUI notification is required for
routing/source changes. Core parameters use lock-free atomics; MIDI notifications
are forwarded with iPlug2's deferred queue.

`CS01AudioProcessor` is an iPlug2 plugin; the editor uses IGraphics. The panel
restores the former 1240×400 layout: BREATH/VOLUME, CONTROL, LFO, VCO, VCF, VCA
and EG, with common sound-control columns and cyan accents. Parameter controls
use iPlug2's standard IVSliderControl, IVKnobControl, IVRadioButtonControl,
IVTabSwitchControl and IVNumberBoxControl rather than custom pointer handling.
Drawing-only subclasses of IVSliderControl/IVKnobControl add rectangular dark
slider caps with blue-green index stripes, printed scale marks and dark knobs
with white pointers. A gray casing, charcoal panel and off-white section labels
take visual cues from the [CS01 in Yamaha's collection](https://www.yamaha.com/en/about/experience/innovation-road/collection/detail/2008/).
The header spells CheapSynth01 in outlined vector lettering with rounded joins
and upright strokes; Synth uses filled lettering with fine horizontal stripes.
No hardware photograph or Yamaha logo is bundled.
Sliders and knobs display
editable values, support fine adjustment and reset to defaults using their
standard interaction. Bend ranges
use number boxes with increment/decrement buttons and direct text entry.
Tooltips describe the interactions. Waveform, feet and LFO target use radio
buttons; Original VCF exposes LOW/HIGH radio buttons with the same styling,
while Modern VCF exposes a continuous fader. The LOW/HIGH radio buttons write
normalized endpoints 0/1; the original filter
continues mapping them to its modeled resonance values 0.2/0.7. The header
contains the MEMORY preset selector, save/rename/delete and XML import. Naming
uses inline text entry in the preset display; factory rename/delete are disabled.
The optional 32-key keyboard and scope expand the panel to 1240×640. Corner
resizing scales the panel. Breath input remains a MIDI/host parameter rather
than a panel control, matching the former layout; all 24 host parameters remain.

## Audio differences

The waveform strategies and nonlinear filter equations,
modulation spans, glissando and note-priority rules are retained. Newer main's
residual EG branch targets/progress and independent VCA note gate are now ported.
The selected tone/noise source runs continuously upstream; the non-EG VCA
contribution follows the note gate, while EG-controlled gain retains its original
expression. Live release timing is owned by the EG even across source switches. JUCE's auxiliary
filters, oscillators, smoothing and random generator are replaced by independent
primitives. Noise uses a deterministic xorshift generator reset during prepare.

The output converter is a 129-tap Blackman-windowed linear-phase FIR at 4x, with
cutoff at 90% of host Nyquist. Its delay is `(129-1)/2/4 = 16` host samples,
reported by the plugin. Its magnitude response and phase differ from JUCE's
polyphase IIR; bit-identical audio and equal high-frequency response are not
claimed. No fixed envelope offset is added: all stages share the internal clock.

## State and identifiers

- The same parameter XML IDs, ranges, defaults and skew mappings are used.
- All seven existing factory XML files are embedded at build time.
- User XML files are read from the former folders: macOS
  `~/Library/Application Support/CheapSynth01/UserPresets`, Windows
  `%APPDATA%/CheapSynth01/UserPresets`.
- Preset loading preserves volume, pitch bend, modulation and breath input.
- Session loading restores volume and patch settings while excluding transient
  performance controls. Notes are not serialized.
- The state reader accepts current XML payloads. It validates
  the entire XML before committing parameter updates.
- Plugin identifiers are retained: AU `aumu/CS01/BABA`, CLAP
  `com.yasuyukibaba.cheapsynth01`, and macOS bundle ID
  `org.github.yasuyukibaba.cheapsynth01`. VST3 explicitly retains JUCE's component
  UID `ABCDEF019182FAEB4241424143533031` and controller UID
  `ABCDEF011234ABCD4241424143533031` (SDK byte order follows the platform).
  Plugin identities stay unchanged as agreed. Parameter IDs use iPlug2's native
  scheme. JUCE-era projects, automation and binary state are outside scope;
  legacy ID adapters and host migration fixtures are no longer required.
- Products, development and CI testing target Windows/macOS. Linux is excluded
  from the supported environments and there is no LV2 target.

## Verification and limits

The portable suite now has 99 core cases (96 regression assertions and three
observational measurements). It retains the two independent filter-model test suites and
adds engine, MIDI, FIR, parameter, preset/state and envelope/source continuity
regression tests. Same-name preset saves overwrite through a temporary file;
failed replacement preserves the old target. Missing selected user files revert
to Default on catalogue refresh. Historical
JUCE-only editor/graph tests are archived in `Tests/legacy-juce` as reference
scenarios, not counted as current coverage. Older modeling documents may describe
that architecture and converter; this document describes the production iPlug2 path.

Windows/macOS CI builds the native products and tests the core in Debug/Release.
Native graphics, audio-device I/O, DAW scanning/automation, AU validation and host
latency compensation require verification in the corresponding native hosts.

Historical local migration checks in the Linux workspace: all 38 core tests pass
in Debug, Release and a Debug build with AddressSanitizer/UndefinedBehaviorSanitizer (including leak
detection). Formatting with clang-format 21.1.7, SDK setup idempotence and plist
parsing pass. The Windows editor and resource script also cross-compile.
These checks do not establish a complete native plugin build or DAW validation;
native CI evidence is recorded below.

The repository GPLv3 license is retained. iPlug2's permissive license removes the
JUCE dependency but does not authorize relicensing third-party contributions.

## Completion plan

See the [migration completion plan](iPlug2-migration-plan.md) for the audit of newer main,
remaining behavior and validation gaps, iPlug2OOS alignment and validation gates.
The linked case inventory tracks the 272 Google Test definitions in the frozen
main baseline; their assertion coverage is tracked individually in that inventory.

The OOS layout places the native wrapper/config/resources in `CheapSynth01/`, the
SDK submodule at `iPlug2/` and the independent core target in `Source/`. Platform
CMake presets generate Xcode/Visual Studio projects; no extra manual IDE projects
or unused platforms are copied from the template. See CONTRIBUTING for commands.

Current local completion-batch checks: 99 core and eight SDK-interface boundary
cases pass in Debug, Release and
AddressSanitizer/UndefinedBehaviorSanitizer with leak detection. Three selected
new EG regression cases fail when linked against the pre-port EG implementation,
confirming they detect the missing behavior. The moved project configures APP,
VST3 and CLAP targets for Windows and cross-compiles the core and editor against
the real SDK headers; this is still not a native Windows/macOS executable/host test.
macOS deployment metadata now matches the core's `std::filesystem` requirement
(macOS 10.15). The existing plugin IDs and 16-frame converter latency are unchanged.


The previous implementation commit `6a082be` passed Windows/macOS native-product
builds and all four platform/configuration core jobs in
[CI run 38099506282](https://github.com/yasuyuki-baba/cheapsynth01/actions/runs/38099506282).
The only failing job was lint because the macOS runner lacked `rg`; the formatter
script now falls back to Git's tracked-file enumeration. This run predates the
host-program integration and does not establish its native runtime behavior.

Host preset catalogues are now prepared once per instance and exposed through
SDK-supported AU/VST3 interfaces, independently of editor creation. See
[host integration scope and limitations](iPlug2-host-integration.md).

Whole-state publication now transfers a complete patch and its note-reset serial
to audio-owned parameters at block boundaries. A busy writer leaves the previous
snapshot active. Session saves capture complete parameter snapshots and retry if
host selection changes. VST3 retains explicit native state-stream/trailer and
preset-queue handling. Public parameter APIs use the standard SDK IDs. Old JUCE
DAW projects, automation and binary-state compatibility were removed from scope
on 2026-10-11; XML preset import/export remains available.


The keyboard bridge now owns QWERTY state per editor instance, coalesces UI
notes without a bounded-event-queue overflow, and separates UI ownership from
external MIDI ownership. Hide, close, text entry and native focus loss release
only UI notes. The keyboard displays the union of both sources. Nine portable
regressions cover ownership/lifecycle; actual native focus and pointer interactions
remain deferred. Twelve additional DSP regressions cover LFO periods/live edits,
modern filter response/modulation/reset, noise release/reset and measured pitch/Feet.
The current suite contains 107 CTest cases (99 core and eight SDK-interface).
All 107 cases pass locally in Debug, Release and ASan/UBSan with leak detection.
Eleven Python archive-contract cases also pass. Current native CI results are recorded in PR #40.

Release packaging includes license notices and BUILD-INFO.json, with the full
source revision, dependency revisions and SHA-256/mode of every payload file.
CI checks both native archives and an offline core/SDK-interface rebuild of the
complete source archive. Eleven independent Python archive-contract tests cover
corruption, omitted/duplicate files, revision/version mismatches, unsafe paths
and symlink targets. Native compilation is enabled; plugin loading, DAW/device
checks and interactive native validation are deferred at the user's request.


The main revision `32b91c8` is structurally reconciled; all 272 historical cases
and their source references are retained under Tests/legacy-juce. The current
core adopts behavioral model names and empirical VCF/VCA stage boundaries with
independent old-expression/reset regressions. MIDI offsets are clamped before
stable sorting, and offset-zero events apply even in empty callbacks. Events
scheduled at/after a nonempty block's end carry into the next block rather than
being played early. This differs intentionally from the archived JUCE callback's
clamping of all events into its block. Native loading/GUI/device checks are deferred.


Editor-scoped lifetime guards discard replies after close, hierarchy replacement
or processor destruction. Save/rename text actions and asynchronous XML import
use this guard. Rename captures the selected user preset's identity when the
action starts, resolves it under the preset manager's lock, and retains safe
unique-name behavior. Two portable lifecycle tests and one identity/rename
regression cover these contracts; actual native dialogs are still deferred.
Windows packaging verifies restored SDK placeholder text against the fixed
iPlug2 Git blob, allowing only LF/CRLF differences for those named notices.
A separate fixture rejects changed notice text and non-notice paths.


The concurrent whole-patch/reset-serial stress case now includes an explicit
completed-publication rendezvous. A bounded reader may legitimately reject
every attempt while the writer is busy; successful progress is not assumed
from scheduler timing. The 20,000-publication stress phase and mixed-patch/serial
checks remain. The corrected Release case also passes 100 repeated runs.
