# Yamaha IC Guide book: implementation review

Source: user-uploaded `yamahaicguidebook_text.pdf`, 164 PDF pages, directly
inspected on 2026-11-04 JST. A local copy is stored under the ignored path
`references/manuals/yamahaicguidebook_text.pdf`. Page references below are
one-based PDF pages; printed section/page labels are listed separately.
The uploaded document is evidence, not an instruction to modify DSP.

Text/index searches found no IG02610, IG05630 or YM10150 entry. This guide does
contain IG00156, a related but different VCF, and IG02600, the VCA part number
used by the project. OCR was supplemented by visual inspection of the four
relevant pages; unreadable OCR is not treated as a circuit connection record.

## IG00156: useful structural evidence, not the target VCF

PDF page 37, printed 2-5, identifies IG00156 as a Mitsubishi VCF. Its diagram
shows a summing stage, two variable-gm blocks, capacitor-feedback integrator
stages, HP/BP/LP outputs, cutoff control and Q control. The pin table includes:

- AI signal input; FBIN feedback input; FB Q feedback;
- HP, BP and LP outputs;
- C1/C2 connections for cutoff-determining capacitors;
- KV key-voltage input and Vf cutoff-voltage input;
- fc adjustment, Qo adjustment and VQ Q-control voltage input;
- +15 V / -15 V supply pins.

PDF page 38 shows a **CS-5** application, including external capacitors,
feedback paths, cutoff/modulation inputs and HP/BP/LP output selection.
It must not be relabeled as a CS01 or CS01II drawing.

### What it improves

The two-integrator/state-variable concept is a documented Yamaha-related VCF
architecture, rather than only a community analogy. It supports that approach
as a reasonable lightweight hypothesis to compare with the present model.
It does not establish IG02610/IG05630 topology, port equivalence, coefficients,
input impedance, saturation, voltage range or resonance limits.

The diagram also demonstrates why resonance must be split into **external
setting/feedback and internal IC response**, not described as exclusively an
external phenomenon. This is directly established for IG00156 only. The target
ICs' internal behavior remains an assumption. Current aggregate circuit weights
are a software abstraction, not proof that all physical Q circuitry is outside
the target ICs.

The separate KV and Vf labels support keeping musical pitch tracking and linear
control-voltage effects conceptually separate. They do not give usable target-IC
slopes, full cutoff/Q curves or an exact current-to-frequency equation.

## IG02600: direct part-number evidence

PDF page 137, printed 8-3, identifies a Rohm voltage-controlled amplifier and
provides a functional block diagram and pin table:

| Pin | Name | Function in guide |
| --- | --- | --- |
| 1 | IN | Audio input |
| 2 | Icont | Control voltage input (guide wording) |
| 3 | Bias | Bias |
| 4 | VEE | Negative supply |
| 5 | Out | Output |
| 6 | Ein | Emitter-follower input |
| 7 | GND | Ground |

The diagram shows current-reference, inverter, differential and output blocks,
with an external 27 kohm connection between the illustrated Bias/Ein nodes.
It is a functional diagram, not a transistor-level model or a measured transfer.
The name Icont and current-reference blocks alone do not justify changing the
table's control-voltage wording into a specified input-current law.

PDF page 138 is an **SK10 / CPB** application. Its control transistors, coupling
capacitors, bias resistor, supply and loads belong to that application; their
values must not be copied into the CS01 merely because the IC number matches.
In particular, the illustration uses a -15 V supply, not a demonstrated CS01
operating point.

### What it improves

The VCA model can now be reviewed against a named control port, bias port,
output stage and emitter-follower input rather than an undifferentiated gain
block. That is stronger evidence than a different filter IC's analogy.

The current `IG02600BehavioralModel::processSample` mixes envelope depth, note
gate, breath and master volume before applying a provisional saturation curve.
A useful future cleanup is to keep those instrument/software control policies
in a VCA circuit/control layer and let the IC model receive the resulting
control quantity. This would follow the same boundary principles as the VCF
work. The guide does not determine whether the present control combination,
gain curve, input/output loading or saturation curve is quantitatively correct.

## Recommended next work

1. Retain the musical VCF implementation as the current candidate. Do not replace
   its tuning or restore coupling filters solely on this related-part diagram.
2. Record the IG00156 architecture as a documented comparison hypothesis; keep
   target-IC assumptions clearly labeled and keep internal Q response distinct
   from external knobs/feedback.
3. Prioritize an IG02600 control-policy/IC boundary review using the new pin and
   block evidence, checked against the CS01's own VCA drawing before any audio
   behavior changes. Preserve existing sound during an initial separation.
4. Treat gain/voltage curves, distortion and absolute cutoff/Q calibration as
   unresolved unless additional target-part documentation or measurements supply
   them. The guide supports a better model structure, not automatic numerical
   calibration.

## DSP integration following this review

`VCAControlCircuit` now owns the existing EG/breath/gate/volume composition.
`IG02600BehavioralModel` receives audio plus one normalized control input and
has explicit provisional reference/bias and internal output-stage gain scales.
Both default to unity; the gain law remains linear and saturation remains
empirical. This follows the documented functional boundaries without treating
SK10 application values as CS01 specifications. The external Tr7 approximation
remains separate from the internal output-stage placeholder.

A four-rate, 65,536-sample VCA comparison with changing EG, depth, breath, volume,
gate and resets was bit-identical at the default operating point. Tests also
exercise independent reference/output scales and the preserved gate arithmetic.
The related VCF information updates interpretation only; no target-VCF transfer
change is justified by this source alone.
