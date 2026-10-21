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
| 2/34 | Breath Control | 14-bit | 0-16383 | MSB/LSB (also driven by Channel Aftertouch) |
| 5/37 | Glissando | 14-bit | 0-16383 | Portamento Time MSB/LSB; discrete pitch steps |
| 7/39 | Volume | 14-bit | 0-16383 | MSB/LSB |
| 9 | VCA EG Depth | 7-bit | 0-127 | |
| 14 | Wave Type | 7-bit | 0-127 | Stepped (Triangle / Sawtooth / Square / Pulse / PWM) |
| 15 | Feet | 7-bit | 0-127 | Stepped (32' / 16' / 8' / 4' / WN) |
| 16 | Fine Pitch | 7-bit | 0-127 | -1 to +1 semitone |
| 70 | Sustain Level | 7-bit | 0-127 | |
| 71 | Resonance | 7-bit | 0-127 | |
| 72 / 79 | Release | 7-bit | 0-127 | Both CC72 (Sound Controller 3) and CC79 supported |
| 73 | Attack | 7-bit | 0-127 | |
| 74 | Cutoff | 7-bit | 0-127 | |
| 75 | Decay | 7-bit | 0-127 | |
| 76 | LFO Speed | 7-bit | 0-127 | |
| 82 | VCF EG Depth | 7-bit | 0-127 | |
| 85 | PWM Speed | 7-bit | 0-127 | |
| 88 | LFO Target | 7-bit | 0-127 | Stepped (VCO / VCF) |
| 89 | Breath VCF | 7-bit | 0-127 | |
| 90 | Breath VCA | 7-bit | 0-127 | |
| 91 | Filter Type | 7-bit | 0-127 | Stepped (Original / Modern) |
| 120 | All Sound Off | — | — | Immediately stops generators/EG and clears residual output state |
| 121 | Reset All Controllers | — | — | Centers bend, clears modulation/breath and their 14-bit caches; preserves notes, volume and patch |
| 123 | All Notes Off | — | — | Clears held notes and starts normal release; repeated messages do not restart release |

## Notes
- Channel Aftertouch (Channel Pressure) drives the same Breath Control input as CC2/34, allowing Breath VCF and Breath VCA modulation from standard MIDI keyboards without a dedicated breath controller.
- Monophonic voice management (highest note priority)
- 14-bit CC uses MSB/LSB pair for high precision control
- All parameters update in real-time

## Panic and lifecycle behavior
- Channel messages currently control the single shared monophonic voice; there is no per-channel voice allocation.
- CC120/123 follow MIDI event positions. Events at the same position retain their input order.
- Preparing/releasing the audio processor clears held-note state. Transport stop alone does not trigger panic, allowing live playing while stopped.
- Sustain pedal (CC64) is not implemented; CC123 releases the shared gate directly.

## Realtime implementation

Held-key tracking and immediate DSP control writes use fixed storage and lock-free
atomics. MIDI edits are published atomically to the iPlug2 delegate; the idle callback
refreshes host/UI values outside rendering. See
[MIDI realtime control flow](MIDI-realtime-control.md) for timing, persistence,
threading and remaining audit limitations.
