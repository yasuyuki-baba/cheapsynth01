> This document records a past filter-only experiment, not current production
> routing. As of baseline `4f2e59d722ca2c761ce7a72d2f6ee719bcc9e900`, production
> oversamples the whole graph. See [Oversampling-validation.md](Oversampling-validation.md)
> and [Audit-stability.md](Audit-stability.md) for current behavior and validation.

# Historical experiment: VCF internal-rate experiment

## Subsequent whole-graph implementation

The standalone VCF experiment decision below is historical. Production now
runs the entire audio graph (including EG/LFO) at the shared internal rate,
and downsamples once at the output. The graph VCO bypasses its local resampler
to avoid a second factor of four. Host MIDI sample positions delimit rendering
segments before conversion to internal sample counts.

A new integration test covers 44.1/48/96 kHz, partitions 7/64/256, preparation
capacity 64, mid-block MIDI events, and release/reprepare reproducibility.
It compares outputs within numerical tolerances, not hardware responses.
The Standalone build passed at the recorded whole-graph implementation revision. Whole-graph observations and the latency
reporting policy are recorded below; they do not establish total alias reduction.

## Output timing policy and final observations

Production reports zero host latency for this IIR implementation. This means no
fixed compensable delay is declared, NOT that the output is physically delay-free.
The graph synthesizes at the internal rate and uses only the downsampling audio
path. JUCE's 4.43267-sample up/down latency must not be applied directly to this
path. The down-path impulse peaks at sample 3; its energy centroid is 4.31957
samples. Neither quantity is a frequency-independent delay. No fixed EG/audio
offset is added: EG, LFO and audio share the internal clock before output conversion.

Production does not implement a linear-phase FIR output converter or fixed
host compensation for its frequency-dependent IIR delay.

Release whole-graph observations (editor closed, blocks of 256, A8, saw/square,
CS-01 Low, cutoff 15 kHz) required roughly 58–70 ms per audio second at 44.1 kHz,
63–68 ms at 48 kHz and 125–131 ms at 96 kHz. These are environment-specific
averages, not worst-case realtime guarantees. One selected seventh-harmonic
fold bin measured -107/-165 dBc at 44.1 kHz and -85/-137 dBc at 48 kHz for
saw/square respectively. Extremely small values approach numerical limitations.
There is no non-oversampled whole-graph baseline, so no improvement amount is
claimed. Total alias power and all waveform/parameter combinations remain untested.

## Historical standalone VCF decision (superseded by whole-graph integration)

Historical decision at the time of this experiment: do not enable VCF oversampling yet. The current experiment uses
the shared factor (4) and JUCE polyphase IIR resampling. It reduces selected
folded components but also changes frequency response. In the interpolated sine
comparison, the largest observed fundamental-gain difference was +3.07683 dB
at 44.1 kHz, cutoff 5 kHz, resonance 0.7, amplitude 0.01, input 10 kHz.
Changing the digital biquad's internal rate changes its frequency warping;
alias reduction must not be presented as a transparent filter replacement.

## Historical biquad change

The empirical input-level follower was changed to preserve its 44.1 kHz time constant
using pow(0.99, 44100 / processingRate). This is sample-rate consistency,
not hardware calibration. No production VCF resampler was added.

## Coverage and limitations

- Finite output and decay after silence: 81 conditions.
- Spectral measurement validated with known fundamental, harmonic, DC and
  folded sine components.
- Interpolated sine comparison: 44.1/48 kHz, cutoff 250/1000/5000 Hz,
  resonance 0.2/0.7, amplitude 0.01/0.5, frequency ratios 0.5/1/2.
- Direct internal-rate sine experiment includes resonance 0.8, which is not
  the CS-01 High setting (0.7).
- Response comparisons are observations, not hardware pass/fail targets.
- Selected spectral bins do not establish total alias power.
- Standalone VCF comparisons do not cover VCO square/saw inputs. Subsequent
  whole-graph representative output and CPU observations are listed above.
