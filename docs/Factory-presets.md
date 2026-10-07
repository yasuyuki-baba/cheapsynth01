# Factory presets transcribed from the CS01 owner's manual

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

## Panel readings

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

The manual gives S–L for EG times, not seconds. The timing range and all
continuous tapers above are the plugin's current approximations. LFO and PWM
endpoints match the manual's specifications, but their tapers are not calibrated.
PWM speed is retained even when the selected waveform does not use it.

## Using the programs

Select a name in the factory program menu. The programs preserve volume, live
breath input, pitch bend, and the modulation wheel. Use the modulation wheel to
introduce the diagram's LFO effect; loading a program does not move the wheel.
Breath sensitivity follows the drawn knobs, but breath input remains an external
performance control. The manual's prose also suggests optional variations (for
example piccolo and trombone); those are not the base diagrams transcribed here.

Tests link the same generated binary preset resources as the plugin. Regression
checks cover the decoded sound controls, switch settings, normalized panel
positions, and preservation of live performance controls.
