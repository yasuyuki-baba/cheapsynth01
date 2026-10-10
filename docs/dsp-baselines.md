# DSP characterization baselines

Run `python3 tools/dsp_characterize.py` from the repository root to regenerate
the non-VCF reference CSVs. Phase 2 VCF CSVs are retained as historical observations of
the removed biquad and cannot be regenerated from the current source tree.

## Meaning and limits

These outputs are regression characterization: deterministic descriptions of
the current model and its observed numeric behavior, useful for comparing a
future code revision against this baseline. They are not correctness limits,
and benchmark timings are machine, compiler and load dependent.

The archived VCF response used the former IG02610 biquad equation. Its harmonics
CSV is a documented polynomial coloration estimate, not measured steady-state
THD from a rendered plugin. VCA rows apply the listed empirical
one-pole high-pass/coupling approximations independently and in combinations;
the stage mask enumerates which are included. Its phase is the sum of the
one-pole phase responses. Frequencies include the low notes associated with
32-foot operation. VCO rows are ideal analytic Fourier references, rather than
the oversampled YM10150 output. EG trajectory is a reproducible linear ADSR
reference, not a capture of JUCE's stateful envelope implementation. Thus the
non-VCF CSVs are starting reference probes and not execution captures of the
plugin.

Hardware calibration means comparing the model with controlled measurements
of an identified physical CS-01, with documented instrument, loading, signal
level, component condition and uncertainty. These files do not establish CS-01
hardware accuracy: several model stages are explicitly empirical, and the
response/harmonic/VCO/EG probes above include idealized references. No matched
hardware measurements with identified units, loading and measurement bandwidth
are recorded here. A model-to-model match is not hardware validation.

## Archived VCF comparisons

`artifacts/dsp/original_vcf_ab.json`, `original_vcf_cpp_benchmark.csv`,
`original_vcf_cpp_response.csv`, `original_vcf_cpp_harmonics.csv`, and the
legacy update CSVs preserve before/after observations gathered while both
implementations were present. The comparative Python harness and Legacy C++
core have been removed. These files remain useful for provenance but are not
current outputs and cannot be recreated from this checkout.

## Cutoff modulation exponent comparison

The processor now sums EG, LFO, and breath contributions in semitones and
evaluates one `exp2` instead of three separate `exp2` calls. This follows the
same equal-tempered cutoff law; because floating-point addition and
multiplication round differently, it can produce tiny numerical differences.
The dedicated formula comparison test renders 262,144 deterministic control
tuples through both expressions, verifies finite checksums and a maximum
relative cutoff error below `1e-6`, and reports five timing repetitions per
build. Raw observations are in
`artifacts/dsp/original_vcf_modulation_exp2_comparison.csv`. The Release
median was about 2.15x faster for this modulation calculation alone; Debug
timings were effectively tied and noisy. This is a formula-kernel benchmark,
not a whole-processor timing. The former filter path produced a maximum absolute
sample difference of `3.34e-6` in the characterization stimulus. Existing Original VCF routing
and modulation tests also pass with the combined expression.

## Processor buffer write probe

The per-block modulation buffer was zeroed before a loop that overwrites every
sample. The clear has been removed. A Release `processBlock` probe over 256
blocks of 1,024 samples measured nearly identical median time with and without
the clear (53.75 versus 54.04 ns/sample, within run-to-run noise), so this is a
redundant-write cleanup rather than a demonstrated CPU win. All output samples
were finite. The five raw repetitions are in
`artifacts/dsp/original_vcf_processblock_clear_comparison.csv`.
