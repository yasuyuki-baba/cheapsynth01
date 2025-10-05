Title: Yamaha CS-Series VCF (IG00156) — Technical Summary

Short summary
The CS-series VCF (implemented with Yamaha’s IG00156 family IC) is a 12 dB/octave state-variable filter designed for musical, smooth frequency shaping across the audible band. Its control input is extremely sensitive (reported to scale the full cutoff range within ~0.25 V), so front-panel controls and modulation sources are scaled through a precision summing network. The filter supports strong resonance (Q up to ~10) without self-oscillation and, together with generous modulation ranges (eg. LFO ±3 octaves, EG up to ~+10 octaves in related models), enables practical coverage of roughly 20 Hz — 20 kHz depending on panel settings and modulation.

Key technical points
- Topology: State-variable filter, 12 dB/octave (gentler slope than 24 dB designs).
- Control sensitivity: ~0–0.25 V control window to sweep cutoff across its practical range; per-source attenuation is done by a summing amplifier and resistor network.
- Resonance: High Q (document cites Q ≈ 10) but filter does not self-oscillate by design.
- Modulation ranges: LFO typically ±3 octaves (6-octave sweep), EG cited by related model data to reach +10 octaves (est.), enabling full audible range sweeps.
- Practical consequence: Designed for musical behavior and mix friendliness rather than extreme, self-oscillating resonance.

Detailed notes
Overview
The IG00156-based VCF used in Yamaha’s small CS-series is optimized for musical behavior and robust performance. Unlike steep 24 dB/octave ladder filters that can sound aggressive and may self-oscillate when resonance is high, the IG00156 design yields a smoother rolloff and a resonance characteristic tuned for clarity without runaway oscillation. The filter is typically implemented as a mono, front-panel-controllable unit; external modulation sources (EG, LFO, breath, keyboard tracking) feed an op-amp based summing network that weights and mixes control voltages before scaling them into the chip’s sensitive control input.

Control scaling and summing network
Service documentation and circuit traces indicate that front-panel controls (CUT OFF knob, LFO depth, EG depth, breath control, keyboard tracking) do not directly drive the VCF control pin. Instead, each source is routed through resistors of specific values (examples cited: R51=22k for manual cutoff, R52=68k for LFO attenuation, R53/R54=33k for EG and keyboard tracking respectively) into an op-amp summing stage. This creates the composite control voltage which is then attenuated/divided to the narrow control window required by IG00156 (e.g., an effective divider formed by a low-value resistor to ground such as R55=470Ω in documentation). The practical result is that manual control dominates small adjustments, EG and tracking have strong influence when raised, and LFO is relatively subtle unless explicitly increased.

Resonance behavior
Documentation cites a maximum Q around 10. Importantly, the circuit design and feedback topology prevent uncontrolled self-oscillation—resonance is implemented with damping so the filter emphasizes the cutoff region without becoming an independent oscillator. This design choice preserves low-frequency energy and avoids the “hollowness” sometimes associated with high-Q ladder filters.

Modulation and range implications
Combined modulation ranges (reported LFO ±3 octaves; EG depth data from related CS models up to +10 octaves) imply an intended design capable of sweeping effectively across the audible band. Taking a conservative base cutoff of ~20 Hz and adding a +10-octave sweep reaches ~20 kHz, matching the practical audible range. LFO modulation, with ±3 octaves, supports dramatic but controlled sweeping when the user engages modulation depth.

Practical guidance for emulation or modification
- Preserve the summing/attenuation architecture: simply driving the VCF control pin with large voltages will not replicate the original’s response; you must emulate the weighted summing and final attenuation.
- Respect the non-oscillating resonance: attempts to force self-oscillation will produce behavior that diverges from the intended CS-character.
- For digital emulation: implement a narrow control input mapping and scale sources into that window, maintain a 12 dB/octave SVF topology, and reproduce the Q behavior with a capped resonance parameter.

References & notes on confidence
- Primary sources: CS service manuals and schematic extracts (see references within repository). Where direct datasheet numbers for IG00156 are not available, circuit trace measurements and related model documentation were used to form estimates — such points are labeled “Estimated” in the detailed reference footnotes.
- Confidence: Many design points (12 dB/oct, summing network, LFO/EG coupling) are confirmed from service schematics; numeric figures for extreme modulation ranges are supported by related model documentation and community findings (confidence: medium-high).
