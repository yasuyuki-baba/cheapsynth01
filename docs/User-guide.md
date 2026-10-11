# Using CheapSynth01

CheapSynth01 plays one note at a time. Launch the standalone application or load
VST3/CLAP (Windows/macOS) or AUv2 (macOS) on an instrument track in your DAW.
For building the current products, see [CONTRIBUTING.md](../CONTRIBUTING.md).
Native host, editor and device verification for the iPlug2 migration is still
pending; see [migration status](iPlug2-migration-plan.md).

## Audio and MIDI Setup

Plugins use the DAW's audio and MIDI configuration. In standalone, open
`File > Preferences…` (`Ctrl+,`) on Windows or `CheapSynth01 > Preferences…`
(`Cmd+,`) on macOS. The iPlug2 Preferences dialog selects the output device and
channels, sample rate, buffer size and MIDI input device/channel. Settings are
saved by the standalone host for the next launch.

All MIDI channels feed the same voice. When several notes are held, the highest
note sounds; overlapping notes retain the envelope gate. Releasing the highest
note returns to the highest remaining note. Velocity does not change volume,
and the sustain pedal is unsupported. Stopping DAW transport alone does not stop
a held note. See [MIDI assignments and panic messages](MIDI-Implementation.md).

## Keyboard and Controls

Enable **KEYBOARD + MONITOR** to display the keyboard and output scope. Click the
keys or play with the PC keys `A W S E D F T G Y H U J K O L`, starting at MIDI note
48. `Z` lowers the PC-keyboard octave and `X` raises it. PC playing requires the
editor to have focus and pauses during text entry. Hiding the keyboard, changing
its octave or leaving the editor releases the relevant UI notes while retaining
externally held MIDI notes.

Sliders and knobs use standard iPlug2 interactions: drag or scroll to adjust,
Shift/Ctrl for fine adjustment, click the displayed value to type, and double-click
the control to reset. Pitch bend range number boxes support typed editing.
Filter tabs **I** and **II** select Original and Modern; both use the label **RES**.

The defaults below are parameter initialization values. **Default** restores the
initial sound controls; factory sounds and saved sessions can set other values.
Depths and levels use normalized values from 0 to 1.

| Control | Range or choices | Default | Effect |
| --- | --- | --- | --- |
| Wave Type | Triangle, Sawtooth, Square, Pulse, PWM | Sawtooth | Selects the tonal waveform; PWM varies pulse width at its own rate. |
| Feet | 32', 16', 8', 4', WN | 8' | Lower feet values raise the octave; WN selects white noise instead of a tonal waveform. |
| PWM Speed | 0.6–12 Hz | 2 Hz | Pulse-width modulation rate for the PWM waveform; independent of LFO Speed. |
| Pitch | -1 to +1 semitone | 0 | Fine tuning around the played note. |
| Glissando | 0–208 ms per semitone | 0 | Steps between overlapping notes in semitones. Zero is immediate; editing the time affects an active slide. |
| Filter Type | Original, Modern | Original | Selects the two-pole or four-pole behavioral low-pass model. |
| Cutoff | 20–20,000 Hz | 20,000 Hz | Base cutoff before EG, LFO and breath modulation. |
| Resonance | 0–1 | 0.2 | Original uses Low/High (threshold 0.5); Modern uses continuous resonance. |
| VCF EG Depth | 0–1 | 0 | Amount of envelope-driven cutoff increase. |
| VCA EG Depth | 0–1 | 1 | Blends note-gated level at 0 with envelope-controlled level at 1. Zero still becomes silent after note release. |
| Attack | 1 ms–2 s | 100 ms | Reference rise time from zero to the envelope peak. |
| Decay | 1 ms–2 s | 100 ms | Reference transition time from the peak to Sustain. |
| Sustain | 0–1 | 0.8 | Held envelope level. |
| Release | 1 ms–2 s | 100 ms | Reference return time after releasing the last held note. |
| LFO Speed | 0.8–21 Hz | 5 Hz | Pitch/cutoff modulation rate. |
| LFO Target | VCO, VCF | VCO | Routes the LFO to pitch or cutoff. |
| Mod Depth / modulation wheel | 0–1 | 0 | LFO amount; at zero, LFO Speed/Target do not create audible modulation. |
| Pitch Bend | -1 to +1 | 0 | Live bend, scaled separately by the up/down ranges. |
| Pitch Bend Up | 0–12 semitones | 12 | Maximum upward bend. |
| Pitch Bend Down | 0–12 semitones | 0 | Maximum downward bend; the initial value disables downward bending. |
| Breath Input | 0–1 | 0 | Live breath-controller value, received through MIDI CC2/34. |
| Breath VCF | 0–1 | 0 | Sensitivity of cutoff to breath input. |
| Breath VCA | 0–1 | 0 | At zero, breath does not affect gain; at one, zero Breath Input mutes the sound. |
| Volume | 0–1 | 0.7 | Final level control with a nonlinear response. |

Envelope times are reference durations. Retriggering from a residual level can
shorten attack; changing a running stage's time changes its remaining duration
without resetting its level. The engine runs internally at four times the host
sample rate, and its output converter adds 16 host samples of reported latency.

## Factory and User Presets

The **MEMORY** menu separates **Factory** (Default and six named sounds) and
**User** presets. A checkmark identifies the current preset.
Select a name or use `<` / `>` to move through the list. The named sounds and
manual panel settings are listed in [factory presets](Factory-presets.md).

- **Save:** enter a name without the `.xml` extension to write the current sound
  to a user preset. Replacing an existing user file requires confirmation.
  Factory resources remain unchanged.
- **Rename:** rename the selected user preset. If the filename already exists, a
  numbered suffix is added. Rename retains unsaved sound edits without writing
  them into the preset file.
- **Delete:** remove the selected user preset after confirmation. Deleting the selected preset loads Default. Factory presets cannot be
  renamed or deleted.
- **Import XML:** load sound controls from an XML file. Use Save to keep an imported
  sound in the user preset directory; importing alone does not copy the file there.

Names must be nonempty filenames of at most 128 UTF-8 bytes, without leading or
trailing spaces, trailing dots, path separators, control characters or reserved
Windows names such as CON. File operation failures are reported in the editor.

User XML files are stored in:

| Platform | Directory |
| --- | --- |
| Windows | `%APPDATA%\CheapSynth01\UserPresets` |
| macOS | `~/Library/Application Support/CheapSynth01/UserPresets` |

The panel list refreshes after in-app save/rename/delete operations and when the
editor opens. Host preset names and snapshots are fixed when the instance starts;
create a new instance to refresh the host catalogue after filesystem changes.
AUv2 and VST3 provide host preset integration; CLAP and standalone use the panel
controls. See [host presets and state](iPlug2-host-integration.md).

## Preset Files and DAW Sessions

| State | Sound controls / bend ranges | Volume | Breath Input, Pitch Bend, Mod Depth | Held notes |
| --- | --- | --- | --- | --- |
| Preset save/load | Stored and restored | Omitted; current value retained | Omitted; current values retained | Not stored; selection retains held notes |
| DAW session save/restore | Stored and restored | Stored and restored | Omitted; current values retained | Not stored; restoration clears held notes |

Breath sensitivity is a sound control; Breath Input is a live performance value.
MIDI Program Change messages are ignored. Current XML presets remain supported,
but JUCE-era DAW projects, automation and binary state are outside the migration
scope. See [migration notes](iPlug2-migration.md).

## If There Is No Sound

Check the standalone output device or DAW track monitoring, held note and Volume.
With Breath VCA at one and Breath Input at zero, breath control mutes the output;
reduce Breath VCA or send breath-controller input. Some factory sounds use breath
sensitivity. Sustain zero with a short Decay creates a brief sound rather than a
held tone. WN selects noise and ignores the tonal waveform choice.

For cutoff modulation from the LFO, select VCF as its target and raise Modulation.
The factory programs preserve the current modulation wheel and breath input;
selecting a sound does not move those performance controls.

## Host Parameter Interaction

Parameters have VCO, VCF, VCA, EG, LFO and Performance groups for hosts that display
parameter groups. Their IDs and order remain unchanged. Attack, Decay and Release
are displayed and entered in milliseconds; Glissando uses milliseconds per
semitone. The DSP and XML preset/session values remain in seconds.

Right-clicking a parameter control uses iPlug2's host context menu in VST3 hosts
that support it. Available commands depend on the DAW. Double-click a slider or
knob to reset its default, or click its value to type a number in the shown unit.
Native editor and DAW interaction verification remains pending.
