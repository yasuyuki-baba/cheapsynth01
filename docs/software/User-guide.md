# Using CheapSynth01

CheapSynth01 plays one note at a time. Install the appropriate format using
[the installation guide](Distribution.md#installing-zip-products), then load it
on an instrument track in your DAW or launch the Standalone application. In
Standalone, select your audio output and MIDI input in its audio/MIDI settings.
Play from a MIDI device or the on-screen keyboard.

All MIDI channels feed the same voice. When several notes are held, the highest
note sounds; overlapping notes retain the envelope gate. Releasing the highest
note returns to the highest remaining held note. Velocity does not change volume,
and the sustain pedal is unsupported. The [MIDI chart](MIDI-Implementation.md)
lists supported controllers and panic messages. Stopping DAW transport alone
does not stop a held note.

## Controls

The defaults below are parameter initialization values. Default restores the
initial sound controls; named factory sounds and saved sessions can set other
values. Loading a preset preserves the live controls described below.
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

Envelope time values are reference times: retriggering from a residual level can
shorten attack, and editing an active stage changes its remaining time without
resetting its level. Filter and LFO destination changes apply at host callback
boundaries and use a hard switch. Exact envelope behavior is documented in
[the EG policy](EG-stateful-model.md); output timing is in
[oversampling and timing](Oversampling-validation.md#production-output-timing).

If no sound is heard, check the audio output/track monitoring, held note, Volume,
and Breath VCA/Input combination. Sustain zero with a short Decay produces a
brief sound rather than a held tone. WN ignores the tonal waveform selection.

## Factory and user presets

The MEMORY menu contains Default, six named factory sounds and user presets.
Select a name, or use `<` / `>` to move through the list. The named sounds and
their recorded panel settings are listed in [factory presets](Factory-presets.md).

- **Save:** stores the current sound as a user preset. Enter a name without a
  filename extension. Saving an existing name asks for overwrite confirmation.
  Saving a factory sound creates a user copy; it does not replace the factory resource.
- **Rename:** renames the selected user preset. If the requested filename exists,
  a unique numbered name is chosen. Rename does not save unsaved sound edits.
- **Delete:** removes the selected user preset after confirmation.
  Factory presets cannot be renamed or deleted.

Names must be single filenames of at most 120 characters, without leading/trailing
whitespace, trailing dots, path separators, control characters or Windows-reserved
names such as CON. Save failures are reported and do not select an unsaved preset.
The list refreshes after in-app save/rename/delete operations. There is no dedicated
refresh button; restart the instance after adding or changing XML files externally
so host program selection uses the updated catalogue. UI selection reads the file.
The preset directory is listed in [the installation guide](Distribution.md#installing-zip-products).

## Preset files and DAW sessions

| State | Sound controls / bend ranges | Volume | Breath Input, Pitch Bend, Mod Depth | Held notes |
| --- | --- | --- | --- | --- |
| Preset save/load | Stored and restored | Omitted; current value retained | Omitted; current values retained | Not stored |
| DAW session save/restore | Stored and restored | Stored and restored | Omitted; current values retained | Not stored; a fresh instance needs a new Note On |

Breath sensitivity is a sound control; Breath Input is a live performance value.
The live Pitch Bend and Mod Depth parameters are not host-automatable. MIDI
Program Change messages are ignored, although the host's program interface can
select presets. See [the MIDI chart](MIDI-Implementation.md) for MIDI assignments.
