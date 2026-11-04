# CheapSynth01 0.5.1

CheapSynth01 0.5.1 refines the filter sound and adds self-oscillation to Modern.

## Sound changes

- **Modern self-oscillation:** near the top of the resonance range (approximately
  97% and above), the filter can sustain a tone without oscillator input.
  Nonlinear feedback stabilizes its amplitude; lowering resonance lets it decay.
  The normal VCA envelope and note gate still control the final output.
- **Modern resonance and cutoff:** the stable range preserves low-frequency
  gain, and low-resonance rolloff is aligned more closely with Original Low.
- **Original low-frequency response:** removed the additional empirical 20 Hz
  input and 8 Hz output high-pass filters. Original retains its 12 dB/oct
  response and Low/High resonance settings; Modern retains 24 dB/oct and
  continuous resonance.

**Existing presets and DAW sessions can sound different at the same parameter
values.** There is no legacy sound mode. Self-oscillation onset, saturation and
resonance tuning are musical approximations, not calibrated hardware measurements.

## DSP structure

Both filter IC models now use a shared processing implementation, with separate
stage counts and IC-specific nonlinear settings. Circuit controls and effective
feedback networks are kept outside the IC profiles. The common IG02600 VCA now
receives a composed control input; its default gain and saturation behavior are
preserved.

The documentation distinguishes observed circuit connections, IC-guide evidence
and provisional modeling choices.

## Supported platforms

Windows APP/VST3/CLAP and macOS APP/VST3/AUv2/CLAP (macOS 10.15 or later).

See [installation instructions](https://github.com/yasuyuki-baba/cheapsynth01/blob/v0.5.1/INSTALL.md).
