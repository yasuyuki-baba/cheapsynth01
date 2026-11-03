# CheapSynth01 0.5.0

CheapSynth01 0.5.0 expands the factory sound library, adds selectable
oversampling and sustain-pedal support, and revises both filter models.

## Before updating

- **Existing sounds will change.** Both Original and Modern filter models have
  been replaced. Existing presets and saved DAW sessions can sound different at
  the same parameter values, including changes in level, timbre and resonance.
  There is no legacy filter mode or automatic gain/cutoff compensation. Keep
  a render of important projects before updating. These remain provisional
  behavioral models, not calibrated reproductions of the original hardware.
- **Direct MIDI CC assignments have changed.** The former panel mappings for
  CC9, 14, 15, 16, 70, 71, 72, 73, 74, 75, 76, 79, 82, 85, 88, 89, 90 and 91
  have been removed. Older MIDI clips using these assignments no longer change
  those sound controls. Use host automation or DAW-side controller mapping.
  Existing parameter IDs and host automation mappings are unchanged. See the
  [MIDI implementation](MIDI-Implementation.md) for supported messages.

## New and improved

- **39 factory presets:** six manual-based sounds, 32 additional sounds and an
  initialization patch. The expanded collection includes basses, leads, pads,
  percussion, effects and patches for repeated-note sequences. The original
  seven factory program indices are retained.
- **Selectable oversampling:** OFF (1x), 2x, 4x, 8x or 16x, with 4x as the
  default. Playback and export use the same selection. The setting is saved
  with the DAW session and stays unchanged when loading sound presets. All
  factors retain 16 host samples of reported latency; higher factors use more CPU.
- **Sustain pedal:** CC64 retains released notes while preserving highest-note
  priority. Pedal release returns to a physically held note or starts the normal
  envelope release. All Notes Off and Reset All Controllers respect the updated
  sustain lifecycle.
- **Breath and aftertouch:** MIDI breath control and channel pressure retain
  independent values and combine using the higher value. Releasing one source
  no longer clears the other.
- **Revised filters:** Original uses a two-pole model with non-resonant Low and
  moderately resonant High settings. Modern uses a four-integrator,
  multi-feedback model with continuous resonance and associated bass reduction.
- **Envelope and sound generation refinements:** revised decay timing, oscillator
  and noise behavior, plus DSP and preset-loading performance improvements.
- **Preset management:** improved user-preset saving and legacy XML loading;
  stable preset IDs and optional author, category, description and tags are
  preserved in XML and session state. Factory presets include categories and tags.
- **Documentation:** expanded user guide, factory-preset reference, MIDI chart,
  Windows/macOS installation instructions and circuit-model evidence notes.

## Fixes

- Improved synchronization between preset selection, host program state, panel
  values and the sound engine, including editor reopening and host automation.
- Fixed MIDI handling while bypassed, noise note-gate behavior, VST3 state-chunk
  handling and UI-keyboard note ownership.

## Supported platforms

Supported products remain Windows APP/VST3/CLAP and macOS APP/VST3/AUv2/CLAP
(macOS 10.15 or later).

See [INSTALL.md](../INSTALL.md) for installation instructions.
