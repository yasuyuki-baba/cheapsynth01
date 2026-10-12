# Factory presets

Source: the supplied Japanese Yamaha CS01 owner's manual PDF
(`d3af9c5b-8449-4b39-8f0d-d0521f2875a5.pdf`). Sound variations are on
printed pages 28–33 (PDF pages 15–18); the specifications are on printed
page 24. The six existing named programs now use those diagrams. Default
is an initialization patch, not a manual sound variation.

Read the centre of the black slider thumb against the printed endpoint marks,
not its upper or lower edge. Switches use their discrete positions. The scan
is skewed and the examples do not give numerical values: continuous positions
below are approximate visual readings, rounded to 0.05 or 0.1. They reproduce
panel settings, not measured hardware frequencies, timing, or sound.

## Manual panel readings

All positions are fractions of travel: 0 = L/S/0, 1 = H/F/L/10 as appropriate.
Pitch is centred and glissando is at S (off) for all six examples. All use
the CS01 (I) filter. RES is a two-position L/H switch, encoded as 0/1.

| Program | Printed page | Wave | Feet | Cutoff | RES | VCF EG | VCA EG | LFO speed | Target | PWM speed | Breath VCF | Breath VCA |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Flute | 28 | Triangle | 8' | .20 | L | .40 | 1 | 0 | VCO | 0 | .70 | .70 |
| Violin | 29 | Sawtooth | 4' | .75 | L | .45 | 1 | .75 | VCO | 0 | .70 | 0 |
| Trumpet | 30 | Sawtooth | 8' | .35 | L | .50 | 1 | 0 | VCO | 0 | 0 | 0 |
| Clavinet | 31 | Pulse | 16' | .60 | H | .50 | 1 | .75 | VCF | 0 | .30 | .30 |
| Solo Synth Lead | 32 | Square | 4' | .35 | L | 1 | 1 | .55 | VCO | .60 | 0 | 0 |
| Synth Bass | 33 | Pulse | 32' | .40 | H | .25 | 1 | .10 | VCF | 0 | 0 | 0 |

| Program | Attack | Decay | Sustain | Release |
| --- | --- | --- | --- | --- |
| Flute | .20 | 0 | 1 | 0 |
| Violin | .10 | .10 | 1 | 0 |
| Trumpet | .10 | .20 | .50 | 0 |
| Clavinet | 0 | .20 | 0 | .65 |
| Solo Synth Lead | 0 | .10 | .30 | .30 |
| Synth Bass | 0 | .30 | 0 | .50 |

## Conversion to plugin values

XML stores plain parameter values, not normalized slider positions. Use the
current `CheapSynth01/core/Parameters.h` definitions and `cs01::fromNormalized` mapping:

For Attack, Decay and Release, use the assumed 10% midpoint A taper:

`tau = (1800 + 2e6 * (81^position - 1) / 80) * 2.2e-6`

Other continuous parameters use:

`value = minimum + (maximum - minimum) * position^(1 / skew)`

For the existing non-EG factory values, round to the parameter's interval. Sustain, EG depths, and breath depths are
linear fractions. Feet indices are 32'=0, 16'=1, 8'=2, 4'=3; waveform indices
are Triangle=0, Sawtooth=1, Square=2, Pulse=3, PWM=4.

| Parameter | Minimum | Maximum | Skew | Interval |
| --- | --- | --- | --- | --- |
| Cutoff | 20 Hz | 20000 Hz | .30 | 1 Hz |
| Attack/Decay/Release | .00396 s | 4.40396 s | A taper (10% midpoint) | .000001 s |
| LFO speed | .8 Hz | 21 Hz | .30 | .01 Hz |
| PWM speed | .6 Hz | 12 Hz | .25 | .01 Hz |

The manual gives S–L for EG times, not seconds. The timing range and all
continuous tapers above are the plugin's current approximations. LFO and PWM
endpoints match the manual's specifications, but their tapers are not calibrated.
EG values are RC time constants rather than total stage durations; see
[EG timing assumptions](EG-RC-timing.md).
PWM speed is retained even when the selected waveform does not use it.

## Using the programs

Select a name in the factory program menu. The programs preserve volume, live
breath input, pitch bend, and the modulation wheel. Use the modulation wheel to
introduce the diagram's LFO effect; loading a program does not move the wheel.
Breath sensitivity follows the drawn knobs, but breath input remains an external
performance control. The manual's prose also suggests optional variations (for
example piccolo and trombone); those are not the base diagrams transcribed here.

Tests check the compiled presets against the source XML and independent manual
readings. Regression
checks cover the decoded sound controls, switch settings, normalized panel
positions, and preservation of live performance controls.

## Original sounds

Thirty-two additional patches are original CheapSynth01 sound designs. They are not
transcriptions of Yamaha manual examples. They follow the original seven entries
in the menu, keeping those factory program indices stable. All 32 play without breath input
or modulation-wheel movement; the wheel adds the listed LFO effect.
These are monophonic sounds, including PWM Strings.

| Program | Character | Wave / Feet | Filter | Wheel effect |
| --- | --- | --- | --- | --- |
| Sub Bass | Sustained, rounded low bass with a short release | Triangle / 32' | I, Low RES | Pitch vibrato |
| Acid Bass | Resonant bass with a fast envelope sweep | Sawtooth / 32' | II, RES .72 | Cutoff modulation |
| Warm Lead | Rounded lead with a small stepped glissando | Sawtooth / 8' | I, Low RES | Pitch vibrato |
| PWM Strings | Slowly opening and releasing string-like tone; PWM runs independently | PWM / 8' | I, Low RES | Pitch vibrato |
| Soft Organ | Sustained square-wave tone with quick key response | Square / 8' | I, Low RES | Pitch vibrato |
| Bright Pluck | Bright upper-register pluck with no sustain | Sawtooth / 4' | II, RES .25 | Cutoff modulation |
| Round Mallet | Rounded upper-register percussive tone with no sustain | Triangle / 4' | I, Low RES | Pitch vibrato |
| Noise Snare | Short filtered white-noise hit with no sustain | White noise | II, RES .20 | Cutoff modulation |
| Square Lead | Bright sustained square lead with a fast attack | Square / 8' | I, Low RES | Pitch vibrato |
| Pulse Lead | Nasal pulse lead with a softer decay | Pulse / 8' | I, High RES | Pitch vibrato |
| PWM Lead | Bright animated PWM lead | PWM / 8' | II, RES .30 | Pitch vibrato |
| Resonant Lead | Sustained resonant lead with an opening filter envelope | Sawtooth / 8' | II, RES .65 | Cutoff modulation |
| Glide Lead | Sustained square lead with stepped glissando between notes | Square / 8' | I, Low RES | Pitch vibrato |
| Chip Sequence | Short, bright game-like notes | Square / 4' | I, Low RES | Fast pitch modulation |
| Bubble Sequence | Rounded short notes with a resonant filter sweep | Triangle / 4' | I, High RES | Cutoff modulation |
| Pulse Sequence | Tight pulse notes for rhythmic patterns | Pulse / 8' | I, Low RES | Fast pitch modulation |
| Acid Sequence | Short resonant saw notes with slight stepped glissando | Sawtooth / 16' | II, RES .78 | Cutoff modulation |
| Synth Kick | Low triangle thump with a short filter envelope | Triangle / 32' | I, Low RES | Cutoff modulation |
| Synth Tom | Rounded pitched hit with a longer decay | Triangle / 16' | I, High RES | Cutoff modulation |
| Closed Hi Hat | Short bright filtered-noise hit | White noise | II, RES .45 | Cutoff modulation |
| Open Hi Hat | Bright filtered-noise hit with a longer decay and release | White noise | II, RES .40 | Cutoff modulation |
| Laser Zap | Short resonant downward filter sweep | Pulse / 4' | II, RES .86 | Fast cutoff modulation |
| Space Sweep | Slow rising and falling resonant PWM texture | PWM / 8' | II, RES .80 | Slow cutoff modulation |
| Metallic Ping | Resonant noise ping with no sustain | White noise | II, RES .90 | Fast cutoff modulation |
| Wind Noise | Slowly swelling filtered-noise texture | White noise | II, RES .30 | Slow cutoff modulation |
| Synth Brass | Rounded saw brass with a firm opening filter envelope | Sawtooth / 8' | I, Low RES | Pitch vibrato |
| Reed Organ | Sustained nasal reed-like tone with a quick attack | Pulse / 8' | I, Low RES | Pitch vibrato |
| Wood Block | Short hollow pitched knock | Triangle / 4' | I, High RES | Cutoff modulation |
| Cowbell | Bright resonant pulse percussion, a cowbell-like approximation | Pulse / 4' | II, RES .82 | Fast cutoff modulation |
| Bubble Drop | Rounded resonant downward filter sweep | Triangle / 8' | II, RES .90 | Cutoff modulation |
| Dark Drone | Low, slowly swelling PWM texture with a long release | PWM / 32' | II, RES .48 | Slow cutoff modulation |
| Breath Lead | Soft animated PWM lead with breath-controlled brightness and level | PWM / 8' | I, Low RES | Pitch vibrato |

The XML files in `CheapSynth01/core/resources/presets` contain the complete plain
parameter values. Envelope times there are RC time constants as described above.
Loading an original sound also preserves volume and live performance controls.

### Playing sequences, percussion and effects

The four Sequence sounds are short, zero-sustain patches for repeated MIDI notes
from a DAW, external sequencer or keyboard. They do not generate a pattern or sync
an LFO to tempo. Start with sixteenth notes at 120 BPM and approximately 80 ms gates.
Each new note retriggers the envelope while retaining its residual level.

For Synth Kick, start at MIDI note 48 (about 65 Hz with 32' Feet); use lower/higher
notes to tune the body. Synth Tom at note 48 is about 131 Hz with 16' Feet. Their
filter envelopes shape the transient; the synth has no pitch-envelope routing.
Hi hats and Metallic Ping use filtered white noise rather than sampled or
multi-oscillator metallic percussion. Hold each percussion note to hear its full
decay; early note-off uses Release. Open Hi Hat has a longer decay than Closed Hi Hat.

Hold Space Sweep and Wind Noise for several seconds for their slow attacks and
filter movement. Laser Zap sweeps the filter, not oscillator pitch. The modulation
wheel adds the effects listed above; preset selection preserves its current value.

### Brass, reeds, additional percussion and breath

Synth Brass opens its filter during the attack and settles to a sustained body.
Reed Organ uses a fixed pulse waveform for a nasal, reed-like tone. Wood Block,
Cowbell and Bubble Drop have zero sustain: hold a note to hear the full decay,
or send an early note-off to shorten it. Start Wood Block and Cowbell around MIDI
note 60 and tune them with the keyboard. Cowbell uses one pulse oscillator and a
resonant filter; it does not reproduce a two-oscillator metallic cowbell circuit.
Bubble Drop sweeps filter frequency rather than oscillator pitch.

Hold Dark Drone for at least two seconds to hear its slow rise. PWM movement runs
independently of the modulation wheel; the wheel adds slow cutoff movement.
Breath Lead plays at zero breath input, with PWM supplying a soft animated tone.
It does not mix white noise into the oscillator. MIDI CC2/34 adds brightness and
level through Breath VCF .70 and Breath VCA .40. Loading the patch preserves the
live breath value; the keyboard controls pitch and note gating as usual.
