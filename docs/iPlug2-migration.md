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
buttons; Original VCF exposes LOW/HIGH tabs, while Modern VCF exposes a continuous
fader. The LOW/HIGH tabs write normalized endpoints 0/1; the original filter
continues mapping them to its modeled resonance values 0.2/0.7. The header
contains the MEMORY preset selector, save/rename/delete and XML import. Naming
uses inline text entry in the preset display; factory rename/delete are disabled.
The optional 32-key keyboard and scope expand the panel to 1240×640. Corner
resizing scales the panel. Breath input remains a MIDI/host parameter rather
than a panel control, matching the former layout; all 24 host parameters remain.

## Audio differences

The waveform strategies, nonlinear filter equations, EG stage model, VCA model,
modulation spans, glissando and note-priority rules are retained. JUCE's auxiliary
filters, oscillators, smoothing and random generator are replaced by independent
primitives. Noise uses a deterministic xorshift generator reset during prepare.

The output converter is a 129-tap Blackman-windowed linear-phase FIR at 4x, with
cutoff at 90% of host Nyquist. Its delay is `(129-1)/2/4 = 16` host samples,
reported by the plugin. Its magnitude response and phase differ from JUCE's
polyphase IIR; bit-identical audio and equal high-frequency response are not
claimed. No fixed envelope offset is added: all stages share the internal clock.

## Compatibility

- The same parameter XML IDs, ranges, defaults and skew mappings are used.
- All seven existing factory XML files are embedded at build time.
- User XML files are read from the former folders: macOS
  `~/Library/Application Support/CheapSynth01/UserPresets`, Windows
  `%APPDATA%/CheapSynth01/UserPresets`.
- Preset loading preserves volume, pitch bend, modulation and breath input.
- Session loading restores volume and patch settings while excluding transient
  performance controls. Notes are not serialized.
- The state reader accepts raw XML and legacy JUCE binary XML chunks. It validates
  the entire XML before committing parameter updates.
- Plugin identifiers are retained: AU `aumu/CS01/BABA`, CLAP
  `com.yasuyukibaba.cheapsynth01`, and macOS bundle ID
  `org.github.yasuyukibaba.cheapsynth01`. VST3 explicitly retains JUCE's component
  UID `ABCDEF019182FAEB4241424143533031` and controller UID
  `ABCDEF011234ABCD4241424143533031` (SDK byte order follows the platform).
  Hosts can identify the new version as the same plugin. Old JUCE automation
  IDs are not yet translated to iPlug2 parameter indices; full DAW-project
  restoration still requires native validation. XML preset import remains available.
- Products, development and CI testing target Windows/macOS. Linux is excluded
  from the supported environments and there is no LV2 target.

## Verification and limits

The portable suite retains the two independent filter-model test suites and
adds engine, MIDI, FIR, parameter and preset/state regression tests. Historical
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
the updated Windows/macOS workflow has not been run from this workspace.

The repository GPLv3 license is retained. iPlug2's permissive license removes the
JUCE dependency but does not authorize relicensing third-party contributions.
