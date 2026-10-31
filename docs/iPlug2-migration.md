# iPlug2 architecture

## Architecture

`CheapSynth01Core` uses standard C++20 and TinyXML2. `SynthEngine` owns the
oscillator, envelope, MIDI state, both filters, VCA and output converter.
Audio follows explicit LFO/EG → VCO → selected VCF → VCA
processing at four times the host rate. MIDI is applied before the corresponding
host frame. No runtime graph mutation or GUI notification is required for
routing/source changes. Core parameters use lock-free atomics; MIDI parameter changes are published atomically; the iPlug2 idle callback refreshes
controls and the VST3 controller cache outside audio rendering.

`CheapSynth01` is an iPlug2 plugin; the editor uses IGraphics. The panel
uses a 1240×400 layout: BREATH/VOLUME, CONTROL, LFO, VCO, VCF, VCA
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
maps them to its modeled resonance values 0.2/0.7. The header
contains the MEMORY preset selector, Save, Save As, Rename, Delete and XML import.
Save As creates and selects a new user patch without changing the sound; existing
names receive a numbered suffix such as `Lead (1)`. Save overwrites the selected
user patch without asking for a name; it is disabled for factory sounds.
The MEMORY header identifies factory sounds as read only and user patches as
editable. A `*` after the name indicates sound settings differ from the loaded or
saved patch; restoring the settings or saving clears it. Volume and live
performance inputs do not count as patch edits. Naming
uses inline text entry in the preset display; factory rename/delete are disabled.
Saved patch and session XML includes `schema="1"` for the file format and
`pluginVersion` for the CheapSynth01 version that wrote it. The version comes
from the CMake project version. XML without these attributes also loads.
Save also updates the sound cached for that user patch's existing DAW program
number. Host names/count/order remain fixed for the plugin instance; additions,
renames and deletions appear in the host list after recreating the instance.
The optional 32-key keyboard and scope expand the panel to 1240×640. Corner
resizing scales the panel. Breath input is a MIDI/host parameter rather
than a panel control; there are 24 host parameters.

## Audio processing

The EG tracks residual branch targets/progress independently of the VCA note gate.
The selected tone/noise source runs continuously upstream; the non-EG VCA
contribution follows the note gate. Live release timing is owned by the EG even
across source switches.
Auxiliary filters, oscillators, smoothing and random generation use independent
primitives. Noise uses a deterministic xorshift generator reset during prepare.

The default output converter is a 129-tap Blackman-windowed linear-phase FIR
at 4x, with cutoff at 90% of host Nyquist. Oversampling is selectable from
OFF (1x), 2x, 4x, 8x and 16x, shared by playback and export. At 2x/8x/16x the
FIR has 65/257/513 taps; OFF uses a plain 16-sample delay. All settings retain
16 host samples of latency, reported by the plugin. Quality is session state
and is excluded from sound presets; sessions without this setting restore at 4x.
No fixed envelope offset is added: all stages share the internal clock.

## State and identifiers

- Attack, Decay and Release store RC time
  constants using the assumed A-taper mapping described in [EG RC timing](EG-RC-timing.md).
- Factory XML files are embedded at build time.
- User XML files are read from these folders: macOS
  `~/Library/Application Support/CheapSynth01/UserPresets`, Windows
  `%APPDATA%/CheapSynth01/UserPresets`.
- Preset loading preserves volume, pitch bend, modulation and breath input.
- Session loading restores volume and patch settings while excluding transient
  performance controls. Notes are not serialized.
- The state reader accepts current XML payloads. It validates
  the entire XML before committing parameter updates.
- Plugin identifiers are: AU `aumu/CS01/BABA`, CLAP
  `com.yasuyukibaba.cheapsynth01`, and macOS bundle ID
  `org.github.yasuyukibaba.cheapsynth01`. VST3 uses component
  UID `ABCDEF019182FAEB4241424143533031` and controller UID
  `ABCDEF011234ABCD4241424143533031` (SDK byte order follows the platform).
  Parameter IDs use iPlug2's native scheme.
- Products, development and CI testing target Windows/macOS. Linux is excluded
  from the supported environments and there is no LV2 target.

## Verification and limits

Current verification covers the portable DSP/state core, SDK interface boundaries,
XML/host-program publication, keyboard ownership and source/native ZIP contracts.
There are 206 portable CTest cases, eight VST3 boundary cases and three iPlug2
parameter-presentation cases. Fifteen Python packaging/signing tests and four
conditional hardware equation tests run separately. Counts describe current
coverage and do not establish native runtime readiness.

Exact model/wrapper references, independent VCA transfer equations,
live EG/source/route timelines, full waveform/pitch/Feet ranges, breath controls,
MIDI boundaries and no-heap portable paths are checked. The active regression
references test current model arithmetic.

Native compilation and ZIP verification do not load a plugin.

Real plugin/DAW/editor/device checks, validators that instantiate products,
host latency compensation and standalone preferences/device persistence remain
pending. Whole SDK callback allocation/locking and native control
thread/gesture/lifecycle behavior are part of that runtime validation. Optional
signing/notarization hooks are prepared; no credentials, signed release or tags
are produced by compilation or archive verification.

See [native validation status](iPlug2-migration-plan.md),
[DSP design](DSP-design.md), [host integration](iPlug2-host-integration.md)
and [release preparation](iPlug2-release-packaging.md).

The repository remains GPLv3. iPlug2's permissive license does not authorize
relicensing existing contributions.
