# Distribution contract

The CMake product version is 0.3.0. Tag builds require `v0.3.0` (or `0.3.0`);
`scripts/validate-release.py` checks the tag before building and checks the complete
asset set before publishing. Windows includes CLAP as well as VST3 and Standalone.
Linux has Standalone/VST3/LV2/CLAP; macOS additionally has AU. A corresponding-source
ZIP with JUCE/CLAP/GoogleTest sources and the build patch is also required. The workflow validates
nonempty, readable ZIPs, required notices and matching product/source manifests.
Packaging requires committed tracked source; Linux CI omits build RPATHs. These checks do not prove a plugin
loads in every host or that its CPU/OS minimum matches a consumer machine.

## Installing ZIP products

Unzip without flattening plugin bundles. Suggested per-user locations:

| Platform | VST3 | CLAP | Other |
|---|---|---|---|
| Windows | `%LOCALAPPDATA%\Programs\Common\VST3` (host support varies); system `%COMMONPROGRAMFILES%\VST3` | `%LOCALAPPDATA%\Programs\Common\CLAP` (host support varies); system `%COMMONPROGRAMFILES%\CLAP` | Standalone: unpack to a writable application directory |
| macOS | `~/Library/Audio/Plug-Ins/VST3` | `~/Library/Audio/Plug-Ins/CLAP` | AU: `~/Library/Audio/Plug-Ins/Components`; LV2: `~/.lv2`; app: `/Applications` or `~/Applications` |
| Linux | `~/.vst3` | `~/.clap` | LV2: `~/.lv2`; Standalone: unpack and run executable |

Choose the host's configured scan path where it differs from these conventions.
User presets use JUCE's `userApplicationDataDirectory/CheapSynth01/UserPresets`.
On Linux JUCE 9.0.3 resolves this through `~/.config/user-dirs.dirs`, with `~/.config`
as fallback; merely changing the environment's XDG_CONFIG_HOME is insufficient.

The workflow targets the runner's native architecture; it does not configure a
universal macOS binary, Windows ARM build, or portable Linux runtime baseline.
Record the runner image, compiler, `file`/PE/Mach-O architectures, dynamic dependencies
and deployment target for each actual release. `*-latest` alone is not an OS minimum.
This audit environment verifies Linux x86_64 only; Windows/macOS products and actual
DAW loading require their own checks. There is no signing/notarization step in the
workflow. Do not describe these packages as signed/notarized without inspecting
actual assets (`codesign`/`spctl` on macOS; Authenticode on Windows).

## Licensing decision required before release

The repository LICENSE remains GPLv3. JUCE **9.0.3** uses AGPLv3 or a JUCE commercial
licence, as confirmed by its LICENSE.md and module headers. The author's commercial
licence status is unknown. GPLv3 section 13 permits combination with AGPLv3 code;
the AGPL requirements apply to the combined work. A GPLv3-only badge must not be
read as replacing JUCE's terms. Alternatively, the distributor must establish an
applicable commercial JUCE licence. No licence acquisition or compliance is assumed.

Before publishing, the author must choose and document the applicable route, supply
corresponding source for the exact binaries (including JUCE, CLAP dependencies,
patches and build scripts), and include the applicable full licence texts and
third-party notices. Preserve JUCE's SPDX inventory and the licences of its bundled
codecs/fonts/graphics dependencies. GPLv3/AGPLv3 availability of an arbitrary branch
is insufficient when it does not reconstruct the distributed binaries.

The package script copies GPLv3, the full AGPLv3 text, JUCE LICENSE.md/SPDX inventory,
upstream dependency licence files, CLAP licences and
source revision information into each format archive. This improves traceability;
it does not certify that every dependency obligation or commercial licence condition
has been satisfied. LICENSE is deliberately unchanged. The release workflow remains
subject to the author's licensing decision; this task does not publish a release.
