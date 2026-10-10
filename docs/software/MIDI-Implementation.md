# MIDI Implementation Chart - CheapSynth01

## Basic Information

| Function | Transmitted | Recognized | Remarks |
|----------|-------------|------------|---------|
| Basic Channel | X | 1-16 | All channels share one voice; no receive-channel filter |
| Default / Changed Channel | X | X | No configurable receive-channel setting |
| Voice Mode | X | Monophonic | Highest-note priority; MIDI mode-change messages are ignored |
| Note Number | X | 0-127 | |
| Velocity | X | X | No amplitude response; zero-velocity Note On is handled as Note Off |
| Aftertouch | X | X | |
| Pitch Bend | X | O | 14-bit precision |
| Control Change | X | O | See table below |
| Program Change | X | X | MIDI messages are ignored; host program API is supported separately |
| System Exclusive | X | X | |

**Legend**: O = Yes, X = No

## Control Change Implementation

| CC# | Parameter | Resolution | Range | Remarks |
|-----|-----------|------------|-------|---------|
| 1/33 | Modulation Depth | 14-bit | 0-16383 | MSB/LSB |
| 2/34 | Breath Control | 14-bit | 0-16383 | MSB/LSB |
| 7/39 | Volume | 14-bit | 0-16383 | MSB/LSB |
| 5/37 | Glissando | 14-bit | 0-16383 | Portamento Time MSB/LSB; discrete pitch steps |
| 70 | Sustain Level | 7-bit | 0-127 | |
| 71 | Resonance | 7-bit | 0-127 | |
| 73 | Attack | 7-bit | 0-127 | |
| 74 | Cutoff | 7-bit | 0-127 | |
| 75 | Decay | 7-bit | 0-127 | |
| 76 | LFO Speed | 7-bit | 0-127 | |
| 79 | Release | 7-bit | 0-127 | |
| 120 | All Sound Off | — | — | Clears held notes, EG/note gate and residual output state |
| 121 | Reset All Controllers | — | — | Centers bend, clears modulation/breath and their 14-bit caches; preserves notes, volume and patch |
| 123 | All Notes Off | — | — | Clears held notes and starts normal release; repeated messages do not restart release |

## Notes
- PWM speed remains a panel/host parameter and has no MIDI CC assignment.
- Monophonic voice management (highest note priority)
- 14-bit CC uses MSB/LSB pair for high precision control
- All parameters update in real-time

## Panic and lifecycle behavior
- Channel messages control the single shared monophonic voice; note tracking and
  14-bit controller caches are shared across channels. A matching note-off on any
  channel releases that note number; there is no per-channel voice allocation.
- All Sound Off resets note bookkeeping and output tails. The selected source
  continues free-running in the production graph while the VCA is silent.
- CC120/123 follow MIDI event positions. Events at the same position retain their input order.
- Preparing/releasing the audio processor clears held-note state. Transport stop alone does not trigger panic, allowing live playing while stopped.
- Sustain pedal (CC64) is not implemented; CC123 releases the shared gate directly.

## Realtime implementation

Held-key tracking and immediate DSP control writes use fixed storage and lock-free
atomics. Host/UI notifications are coalesced on a message-thread timer. See
[MIDI realtime control flow](MIDI-realtime-control.md) for timing, persistence,
threading and remaining audit limitations.
