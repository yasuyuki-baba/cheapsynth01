# Requested Circuit Resources

See [Circuit model unknowns](Circuit-model-unknowns.md) for unresolved details and
current approximations. The earlier audit used TC7476BP documentation supplied
locally as `docs/tech/TC7476BP.pdf`; that PDF is absent from this checkout.
See the [external source catalog](Source-catalog.md) for recorded sources
and availability. Preserve the existing analysis; reproducing its source
inspection requires obtaining the original material again.

## Priority 1: EG stage switching

- Analyze the Toshiba **TC7476BP** manufacturer datasheet (prefer the exact part
  over another 7476 variant): pinout, asynchronous Set/Reset polarity,
  simultaneous-input truth table, supply range, and input thresholds. These are
  needed to determine how storage-capacitor feedback to IC4 controls the
  Attack-to-Decay transition.
- Yamaha CS-01 EG adjustment/inspection procedures and TP4 voltage/timing
  conditions. Supplements to the existing manual would be useful.

## Priority 2: Analog audio and control characteristics

- **IG02610**: VCF internal topology, control-current-to-cutoff relationship,
  input impedance, and High/Low resonance measurement conditions.
- **IG02600**: control-input-to-gain relationship and EG/breath input combination.
- **YM10150**: keyboard priority, gate, retrigger, and waveform output specifications.

## Priority 3: Devices and controls

- Y-rank characteristics of 2SK30A and operating conditions of 2SC1815/2SA1015.
- Panel potentiometer A-taper specifications and slider-position-to-time inspection values.

Keep external originals and private inventories locally under `references/`.
Record only shareable source, revision and page information in
[the catalog](Source-catalog.md).
Keep authored analysis in `docs/`. Prefer manufacturer material; treat documents
for other instruments or compatible parts as references, not proof of identical
characteristics.

## Implementation policy until evidence is sufficient

Keep the production EG as a provisional stateful exponential envelope with
durations in seconds, as described in [EG-stateful-model.md](EG-stateful-model.md).
Its implemented and regression-tested curvature is not verified hardware behavior.
RC charging/discharging calculations have been tested, but CS-01 switching
conditions remain unresolved. Prioritize continuity, monotonicity, retriggering,
parameter changes, and independence from sample rate and block partitioning,
rather than claiming circuit accuracy based on speculation.
A timbre-changing exponential EG migration should be a separate change with
explicit timing definitions and endpoint conditions.
