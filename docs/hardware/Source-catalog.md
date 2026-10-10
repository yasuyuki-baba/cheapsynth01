# External source catalog

Use `docs/` for authored explanations, designs and audits. Use `references/`
for locally obtained external originals such as manuals, schematics and
datasheets that must remain local. The entire `references/` directory is
ignored by Git and must not be force-added. This shared catalog contains source
information from existing project documents; keep private inventories and
non-shareable source details locally in `references/`. The
[documentation guide](../README.md) indexes authored material.

## Optional provenance, not a reading prerequisite

Project documents preserve the relevant settings, connection tables, logic
rules, numerical limits and calculation assumptions in their own text.
This catalog records where those findings originated; the external files and
URLs are not required to understand or use the project documentation.
Preservation does not turn a conditional interpretation into a verified fact,
and does not make the recorded extract a complete substitute for a datasheet.

## Availability and provenance

Inventory checked on 2026-10-10 JST: no external source PDFs are present under
`references/` or `docs/` in this checkout. The entries below describe sources
cited by existing project documents, not newly reviewed originals. URLs are
recorded acquisition leads and have not been checked during this organization
pass. Earlier reports of local availability apply to their original audit.

An absent file does not invalidate a preserved transcription, but prevents
independent inspection of the original from this checkout. Do not infer that
similarly named documents or compatible parts are interchangeable.

## Manuals and schematics

| Source | Recorded acquisition lead or filename | Cited pages / region | Local original and project analysis |
| --- | --- | --- | --- |
| Yamaha CS01 Japanese owner's manual | User-supplied manual; another report cites the historical path `docs/tech/CS01J.pdf`. No public acquisition URL recorded; identity of the source copies has not been checked. | Printed 16–18 (EG descriptions), 24 / PDF 13 (specifications), 28–33 / PDF 15–18 (sound variations). | Absent. [Presets](../software/Factory-presets.md), [modulation ranges](../software/Modulation-speed-ranges.md), [EG timing](../software/EG-time-range-validation.md). |
| Yamaha CS-01 service manual / overall circuit | [Recorded service-manual URL](https://manuals.plus/m/7bf5a88f7fd7a35ab00af242825c27ab94ff0c3ffd909bbce88c3360fa997a4f). The audit's local overall-circuit filename is not recorded. | Sheet BC1, EG region H4–H6; VCF identification, controls and adjustments. PDF page mapping not recorded. | Absent. [EG circuit audit](EG-model-audit.md), [Original VCF design](../software/Original-VCF-behavioral-design.md), [low-frequency audit](Low-frequency-circuit-audit.md), [glissando audit](Glissando-circuit-audit.md). |
| Yamaha CS-01II service drawing | No exact acquisition URL or local filename recorded. | IC2 / IG05630 identification; page not recorded. | Absent. [CS-01II VCF model](../software/CS01II-VCF-model.md). |
| Yamaha PS-1 / PS-2 / PS-3 service manual | [Recorded manual URL](https://www.manualslib.com/manual/4155002/Yamaha-Portasound-Ps-1.html). | GE2 description, model-specific block diagrams and overall circuit diagrams; page numbers not recorded. | Absent. [Related-model evidence](../software/Original-VCF-behavioral-design.md). Related instruments, not proof of CS-01 topology. |
| Yamaha PS-30 service manual | [Solo generator page](https://www.manualslib.com/manual/4397023/Yamaha-Ps-30.html?page=14), [electronic components page](https://www.manualslib.com/manual/4397023/Yamaha-Ps-30.html?page=24). | Viewer pages 14 and 24; printed-page mapping not recorded. | Absent. [Related-model evidence](../software/Original-VCF-behavioral-design.md): identifies IG02612, not IG02610. |

## Datasheets and secondary interpretations

| Source | Recorded acquisition lead or filename | Cited pages / revision | Local original and project analysis |
| --- | --- | --- | --- |
| Toshiba TC7476BP datasheet | Historical local path `docs/tech/TC7476BP.pdf`; [1988 databook](https://www.bitsavers.org/components/toshiba/_dataBook/1988_Toshiba_TC4000_4500_5000_CMOS_Logic.pdf), [1985 databook](https://www.bitsavers.org/components/toshiba/_dataBook/1985_Toshiba_C2MOS_Integrated_Circuits.pdf), [archive index](https://www.datasheetarchive.com/?q=tc7476bp). Earlier retrievals of the 1988 scan failed; the 1985 scan returned HTTP 403. The archive index was located but its datasheet contents were not validated. | Local extract PDF 1–2 / printed 574–575: pinout, truth table and electrical limits. Equivalence of the online scans to the extract is not newly verified. | Absent. [Recorded logic and supply limits](TC7476BP-online-investigation.md), [datasheet-to-wiring audit](EG-model-audit.md). |
| Toshiba 2SC1815 datasheet | [Recorded distributor-hosted manufacturer PDF](https://media.digikey.com/pdf/Data%20Sheets/Toshiba%20PDFs/2SC1815.pdf). | 2007-11-01, page 1. | Absent. [EG device analysis](EG-model-audit.md); quoted high-current conditions do not establish low-current EG operating points. |
| IG02610/11 schematic interpretation | [Secondary reverse-engineering article](https://ss30m.blogspot.com/2020/05/fun-with-filters-pt2.html). | Article dated May 2020; no PDF page numbering. | No local copy. [Original VCF design](../software/Original-VCF-behavioral-design.md). Interpretation, not Yamaha documentation or measured hardware evidence. |
| YM10150, LFO and IG00156 background sources | Technical summaries mention service manuals and community analysis without exact source identifiers for every claim. | Individual revisions, pages and thread URLs are not recorded. | No identified local originals. [YM10150](ymf10150.md), [LFO](lfo.md), [IG00156](filter.md). Claims without exact source attribution are unverified interpretations. |

[Circuit evidence status](Circuit-evidence-status.md) and
[unresolved circuit details](Circuit-model-unknowns.md) describe the limits of
recorded findings for IC internals, device operating points and control curves.

## Storage and sharing boundary

Authored explanations and analysis are tracked under `docs/`. External originals
and private inventories are local-only under `references/`; ordinary Git staging
excludes the entire directory. This catalog contains shareable source information
from project documents. A catalog entry does not make an original available to
another checkout. Historical filenames identify the source used in the recorded
analysis, not a promise of current local availability.
