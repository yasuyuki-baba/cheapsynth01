# MIDI Implementation Chart - CheapSynth01

## Basic Information

| Function | Transmitted | Recognized | Remarks |
|----------|-------------|------------|---------|
| Basic Channel | X | 1-16 | |
| Default | X | All channels | Shared voice; standalone host may filter input |
| Changed | X | 1-16 | |
| Mode | X | Omni, monophonic | Shared voice across channels |
| Note Number | X | 0-127 | |
| Velocity | X | X | |
| Aftertouch | X | O | Channel Pressure (0xD0) controls Breath Input |
| Pitch Bend | X | O | 14-bit precision |
| Control Change | X | O | See table below |
| Program Change | X | X | |
| System Exclusive | X | X | |

**Legend**: O = Yes, X = No

## Control Change Implementation

| CC# | Parameter | Resolution | Range | Remarks |
|-----|-----------|------------|-------|---------|
| 1/33 | Modulation Depth | 14-bit | 0-16383 | MSB/LSB |
| 2/34 | Breath Control | 14-bit | 0-16383 | MSB/LSB; maximum with Channel Pressure |
| 5/37 | Glissando | 14-bit | 0-16383 | Portamento Time MSB/LSB; discrete pitch steps |
| 7/39 | Volume | 14-bit | 0-16383 | MSB/LSB |
| 64 | Sustain Pedal | Switch | 0-127 | 0-63 off, 64-127 on; retains released notes |
| 120 | All Sound Off | — | — | Immediately stops generators/EG and clears residual output state |
| 121 | Reset All Controllers | — | — | Centers bend, clears modulation/breath/pressure caches, lifts sustain; preserves physically held notes, volume and patch |
| 123 | All Notes Off | — | — | Releases held keys; honors sustain, otherwise starts normal release |

## Notes
- Channel Aftertouch (Channel Pressure) drives the same Breath Control input as CC2/34, allowing Breath VCF and Breath VCA modulation from standard MIDI keyboards without a dedicated breath controller. The two sources retain independent state and combine as `max(((MSB << 7) | LSB) / 16383, pressure / 127)`. Releasing either source does not clear the other; CC121 clears both.
- Monophonic voice management (highest note priority)
- 14-bit CC uses MSB/LSB pair for high precision control
- All parameters update in real-time
- Direct MIDI control is limited to performance inputs and controller reset/panic. Sound-design controls are exposed through the UI and host automation; use DAW-side mapping for external knobs. The standalone application has the same MIDI scope and does not provide MIDI Learn.
- All CCs not listed above are ignored. In particular, CC88 and CC91 do not change LFO Target or Filter Type.

## Compatibility change

The former panel mappings for CC9, 14, 15, 16, 70, 71, 72, 73, 74, 75, 76, 79,
82, 85, 88, 89, 90 and 91 have been removed. Older MIDI clips or controllers using
these assignments no longer change sound parameters. Recreate those operations
with host automation or DAW-side mapping when needed. Parameter IDs, UI controls,
preset/session values and host automation mappings are unchanged.

## Panic and lifecycle behavior
- Channel messages currently control the single shared monophonic voice; there is no per-channel voice allocation.
- CC120/123 follow MIDI event positions. Events at the same position retain their input order.
- Preparing/releasing the audio processor clears held-note state. Transport stop alone does not trigger panic, allowing live playing while stopped.
- Sustain is shared across channels and UI/external note sources, like the single voice. Highest-note priority includes keys retained by the pedal; overlapping or repeated retained notes do not retrigger the EG.
- Lifting CC64 removes retained notes, returns to the highest physically held key without retriggering, or starts the normal release if no keys remain. Repeated pedal-off/CC123 messages do not restart a release.
- CC121 lifts the pedal and releases only pedal-retained notes. CC120 and preparation clear both key sets and pedal state immediately. A fresh pedal-on message is required after these resets.

## Realtime implementation

Held-key tracking and immediate DSP control writes use fixed storage and lock-free
atomics. MIDI edits are published atomically to the iPlug2 delegate; the idle callback
refreshes host/UI values outside rendering. See
[MIDI realtime control flow](MIDI-realtime-control.md) for timing, persistence,
threading and remaining audit limitations.
