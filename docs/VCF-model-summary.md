# Current VCF DSP decisions

The product uses lightweight musical approximations. Component-level simulation
is not required merely to account for every visible component. Keep known circuit
connections, uncertain IC hypotheses and product tuning distinct in documentation.

## Shared implementation

`VCFBehavioralCore<Stages, Parameters>` implements the identical runtime API,
input shaping, integration, recovery and output limiting for both IC models.
`VCFIntegratorCore<Stages>` solves integrator states and effective feedback
simultaneously. There is no extra delay at the IC/circuit boundary.

The IC declarations select two or four stages and separate nonlinear parameter
types. The two parameter sets currently have the same values, but can evolve
independently. Sharing code does not establish identical physical internals.

| Choice | Original / IG02610 | Modern / IG05630 |
| --- | --- | --- |
| Behavioral stages / asymptotic slope | 2 / 12 dB per octave | 4 / 24 dB per octave |
| Panel resonance | Low/High | Continuous |
| Current resonance range | Provisional Q 0.5/1.6 | Stable range followed by bounded self-oscillation near the upper end |
| Small-signal DC gain | Unity | Unity at all resonance settings |
| Self-oscillation | Not enabled | Begins near resonance 0.967 in the current musical approximation |
| VCF input/output high-pass additions | None | None |

Circuit settings assemble the complete effective feedback arrays; the IC classes
do not embed resistor ratios, switch mappings or a scalar resonance law.
Modern's normalization and cutoff alignment are documented product choices.
These arrays approximate the combined effect of external feedback and assumed
IC response; they do not prove that physical resonance control is wholly external.

## Evidence and provisional choices

- The supplied CS01/CS01II drawings identify the target ICs and visible external
  branches. The corrected C2 node and additional LP-to-Vin path are recorded in
  the [drawing review](hardware/VCF-external-network-review.md).
- The Yamaha guide documents the **different** IG00156 VCF's gm/integrator and
  internal Q-control blocks. It supports a comparison architecture, not target
  IC pin equivalence, exact gain laws, saturation or Q limits.
- The externally supplied summaries about OTA structure and self-oscillation are
  hypotheses, not device specifications. Modern now includes self-oscillation
  as an explicit musical approximation, with in-loop amplitude stabilization.
  Slider onset, level and nonlinear law are chosen parameters, not calibration.
- The old empirical VCF 20/8 Hz coupling filters were removed intentionally.
  Their absence is not a claim that physical coupling capacitors have no effect.
- The IG02600 VCA is shared by both modes. Its control-policy separation and
  provisional reference/output settings follow direct part-number guide evidence;
  numerical transfer curves remain assumptions.

## Current work and validation

The consolidation removes duplicate IC processing code while preserving the
existing musical tuning and host/state parameter interfaces. Compare rendered
audio before and after consolidation, then run independent response/DC tests,
resonance and stability checks, modulation/partition tests and the full core suite.
Numerical checks establish software behavior; musical preference is assessed by
listening and does not require a claim of calibrated hardware reconstruction.

See the [musical tuning note](hardware/VCF-musical-model.md),
[guide review](hardware/Yamaha-IC-guide-review.md) and
[DSP design](DSP-design.md) for details.
