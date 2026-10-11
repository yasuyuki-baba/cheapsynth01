# JUCE to iPlug2 migration

## Architecture

`CheapSynth01Core` uses standard C++20 and TinyXML2. `SynthEngine` owns the
oscillator, envelope, MIDI state, both filters, VCA and output converter. The
former JUCE graph is replaced by explicit LFO/EG → VCO → selected VCF → VCA
processing at four times the host rate. MIDI is applied before the corresponding
host frame. No runtime graph mutation or GUI notification is required for
routing/source changes. Core parameters use lock-free atomics; MIDI parameter changes are published atomically; the iPlug2 idle callback refreshes
controls and the VST3 controller cache outside audio rendering.

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

- Parameter XML IDs remain stable. Attack, Decay and Release now store RC time
  constants using the assumed A-taper mapping described in [EG RC timing](EG-RC-timing.md).
  Old timing values are not automatically converted.
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

Current verification covers the portable DSP/state core, SDK interface boundaries,
XML/host-program publication, keyboard ownership and source/native ZIP contracts.
There are 206 portable CTest cases, eight VST3 boundary cases and three iPlug2
parameter-presentation cases. Fifteen Python packaging/signing tests and four
conditional hardware equation tests run separately. Counts describe current
coverage and do not establish equivalent sound or old-project compatibility.

Frozen model arithmetic exposed a resonance precision difference: the original
model now subtracts damping endpoints as double, retaining the former formula's
precision. Exact model/wrapper references, independent VCA transfer equations,
live EG/source/route timelines, full waveform/pitch/Feet ranges, breath controls,
MIDI boundaries and no-heap portable paths are checked. The active regression
references test current model arithmetic; they do not maintain the removed JUCE
graph or IIR converter.

Historical migration verification results are recorded in
[PR #40](https://github.com/yasuyuki-baba/cheapsynth01/pull/40); those results apply
to its tested revision. Native compilation and
ZIP verification do not load a plugin. Fifteen Python archive/signing contract
checks and four conditional hardware equation checks run separately.

Real plugin/DAW/editor/device checks, validators that instantiate products,
host latency compensation and standalone preferences/device persistence remain
pending. Whole SDK callback allocation/locking and native control
thread/gesture/lifecycle behavior are part of that runtime validation. Optional
signing/notarization hooks are prepared; no credentials, signed release or tags
are produced by compilation or archive verification.

See [native validation status](iPlug2-migration-plan.md),
[DSP design](DSP-design.md), [host integration](iPlug2-host-integration.md)
and [release preparation](iPlug2-release-packaging.md).

The repository remains GPLv3. iPlug2's permissive license removes JUCE as a
dependency; it does not authorize relicensing existing contributions.
