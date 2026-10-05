# EG control-link first pass

OriginalVCFProcessor and VCAProcessor now ramp EG depth over 5 ms using the
processing sample rate. Preparation initializes the ramp at the actual stored
value, avoiding a startup fade. The EG signal itself is not smoothed: its attack,
release and retrigger timings remain intact. Depth targets are read per block,
with sample-by-sample ramps, independent of partition for identical event times.

The VCF's 36-semitone EG span and VCA's linear depth blend are retained. Depth
zero reaches exact independence after the ramp, not immediately on an edit.
This is a provisional dezippering policy, not a measured control circuit.
Modern VCF, breath, volume and cutoff controls are outside this change.

The new VCA test compares live depth-change trajectories at block sizes 1, 7
and 64. All nine VCA tests pass after rebuild. The existing VCA endpoint test
now prepares identical controls/filter state before each endpoint measurement;
it is not a live-transition test. An earlier full run passed 184 of 185 tests
before this test correction. After the correction, both tests and the standalone
app were rebuilt; all 186 tests from 38 suites passed with `--all` (exit status
0), including the new partition test and observation tests.

No comparison WAVs, listening assessment or revised modulation calibration
were produced. This implements control continuity only; final musical balance
and large-resonance behavior still require audio evaluation.