# MIDI Implementation Chart - CheapSynth01

## Basic Information

| Function | Transmitted | Recognized | Remarks |
|----------|-------------|------------|---------|
| Basic Channel | X | 1-16 | |
| Default | X | 1 | |
| Changed | X | 1-16 | |
| Mode | X | Mode 3 (Omni Off, Poly) | Actually Monophonic |
| Note Number | X | 0-127 | |
| Velocity | X | X | |
| Aftertouch | X | X | |
| Pitch Bend | X | O | 14-bit precision |
| Control Change | X | O | See table below |
| Program Change | X | X | |
| System Exclusive | X | X | |

**Legend**: O = Yes, X = No

## Control Change Implementation

| CC# | Parameter | Resolution | Range | Remarks |
|-----|-----------|------------|-------|---------|
| 1/33 | Modulation Depth | 14-bit | 0-16383 | MSB/LSB |
| 2/34 | Breath Control | 14-bit | 0-16383 | MSB/LSB |
| 7/39 | Volume | 14-bit | 0-16383 | MSB/LSB |
| 11 | PWM Speed | 7-bit | 0-127 | |
| 35/37 | Glissando | 14-bit | 0-16383 | MSB/LSB |
| 70 | Sustain Level | 7-bit | 0-127 | |
| 71 | Resonance | 7-bit | 0-127 | |
| 73 | Attack | 7-bit | 0-127 | |
| 74 | Cutoff | 7-bit | 0-127 | |
| 75 | Decay | 7-bit | 0-127 | |
| 76 | LFO Speed | 7-bit | 0-127 | |
| 79 | Release | 7-bit | 0-127 | |
| 120 | All Sound Off | — | — | Immediately stops generators/EG and clears residual output state |
| 123 | All Notes Off | — | — | Clears held notes and starts normal release; repeated messages do not restart release |

## Notes
- Monophonic voice management (highest note priority)
- 14-bit CC uses MSB/LSB pair for high precision control
- All parameters update in real-time

## Panic and lifecycle behavior
- Channel messages currently control the single shared monophonic voice; there is no per-channel voice allocation.
- CC120/123 follow MIDI event positions. Events at the same position retain their input order.
- Preparing/releasing the audio processor clears held-note state. Transport stop alone does not trigger panic, allowing live playing while stopped.
- Sustain pedal (CC64) is not implemented; CC123 releases the shared gate directly.
