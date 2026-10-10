# Factory presets: recorded panel settings and plugin conversion

The tables below contain the complete panel readings used for the six named
factory programs. Default is an initialization patch. The readings were recorded
from owner's-manual examples; their provenance is in
[the source catalog](../hardware/Source-catalog.md). The tables and conversion
rules below are sufficient to understand the presets without those diagrams.

Continuous positions are approximate visual readings of slider-thumb centres,
rounded to 0.05 or 0.1; switches use discrete positions. The original examples
gave no numerical continuous values. These settings do not establish measured
hardware frequencies, timing or sound.

## Panel readings

All positions are fractions of travel: 0 is the low/slow/short endpoint,
and 1 is the high/fast/long endpoint. Panel labels use L/H for low/high,
S/F for slow/fast, S/L for short/long, or 0/10 for depth.
Pitch is centred and glissando is at S (off) for all six examples. All use
the CS01 (I) filter. RES is a two-position L/H switch, encoded as 0/1.

| Program | Wave | Feet | Cutoff | RES | VCF EG | VCA EG | LFO speed | Target | PWM speed | Breath VCF | Breath VCA |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Flute | Triangle | 8' | .20 | L | .40 | 1 | 0 | VCO | 0 | .70 | .70 |
| Violin | Sawtooth | 4' | .75 | L | .45 | 1 | .75 | VCO | 0 | .70 | 0 |
| Trumpet | Sawtooth | 8' | .35 | L | .50 | 1 | 0 | VCO | 0 | 0 | 0 |
| Clavinet | Pulse | 16' | .60 | H | .50 | 1 | .75 | VCF | 0 | .30 | .30 |
| Solo Synth Lead | Square | 4' | .35 | L | 1 | 1 | .55 | VCO | .60 | 0 | 0 |
| Synth Bass | Pulse | 32' | .40 | H | .25 | 1 | .10 | VCF | 0 | 0 | 0 |

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
current `CS01AudioProcessor::createParameterLayout` ranges and JUCE's
`NormalisableRange::convertFrom0to1` mapping:

`value = minimum + (maximum - minimum) * position^(1 / skew)`

Round to the parameter's interval. Sustain, EG depths, and breath depths are
linear fractions. Feet indices are 32'=0, 16'=1, 8'=2, 4'=3; waveform indices
are Triangle=0, Sawtooth=1, Square=2, Pulse=3, PWM=4.

| Parameter | Minimum | Maximum | Skew | Interval |
| --- | --- | --- | --- | --- |
| Cutoff | 20 Hz | 20000 Hz | .30 | 1 Hz |
| Attack/Decay/Release | .001 s | 2 s | .30 | .001 s |
| LFO speed | .8 Hz | 21 Hz | .30 | .01 Hz |
| PWM speed | .6 Hz | 12 Hz | .25 | .01 Hz |

The recorded EG labels are S–L (short–long), without numerical durations.
The timing range and all
continuous tapers above are the plugin's current approximations. LFO and PWM
endpoints match the manual's specifications, but their tapers are not calibrated.
PWM speed is retained even when the selected waveform does not use it.

## Using the programs

Select a name in the factory program menu. The programs preserve volume, live
breath input, pitch bend, and the modulation wheel. Use the modulation wheel to
introduce the selected preset's LFO effect; loading a program does not move the wheel.
Breath sensitivity follows the table, but breath input remains an external
performance control. Optional variations such as piccolo and trombone are not
part of these six base presets.

Tests link the same generated binary preset resources as the plugin. Regression
checks cover the decoded sound controls, switch settings, normalized panel
positions, and preservation of live performance controls.
