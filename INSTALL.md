# Installing CheapSynth01

Download the ZIP for your operating system from
[GitHub Releases](https://github.com/yasuyuki-baba/cheapsynth01/releases) and
extract it before installing. Native release ZIPs contain the products under
`out/`; the source ZIP is for building, not installation.

Close your DAW and CheapSynth01 before copying or replacing any products. Install
only the plugin formats your DAW supports. Copy each complete bundle or file,
including its original name and contents.

## macOS

In Finder, choose **Go > Go to Folder…** and enter the destination below. `~`
means your home folder. Create any missing folders; these user-level locations
do not require administrator access.

| Product in `out/` | Destination folder |
| --- | --- |
| `CheapSynth01.vst3` | `~/Library/Audio/Plug-Ins/VST3/` |
| `CheapSynth01.clap` | `~/Library/Audio/Plug-Ins/CLAP/` |
| `CheapSynth01.component` (AUv2) | `~/Library/Audio/Plug-Ins/Components/` |
| `CheapSynth01.app` (standalone) | `~/Applications/` |

Copy the whole product into its destination. For example, the installed VST3
bundle should be `~/Library/Audio/Plug-Ins/VST3/CheapSynth01.vst3`. When updating,
replace the existing bundle rather than merging its contents. AUv2 is for hosts
such as Logic Pro; VST3 and CLAP require a host that supports those formats.

### Unsigned builds

The current release workflow does not sign or notarize products. macOS may block
the standalone application or prevent a DAW from scanning a downloaded plugin.

For the standalone application, try opening it, then use **System Settings >
Privacy & Security > Open Anyway** if macOS offers that option. On older macOS
versions, this is in **System Preferences > Security & Privacy**.

If a downloaded plugin is blocked by quarantine and you trust the release you
downloaded, open Terminal and run the command for the installed format:

```sh
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/VST3/CheapSynth01.vst3"
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/CLAP/CheapSynth01.clap"
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/Components/CheapSynth01.component"
```

Run only the lines for products you installed. These commands remove the
download quarantine attribute from those CheapSynth01 bundles; they do not sign
the plugins or disable Gatekeeper globally. Restart the DAW and rescan afterward.
A host that requires signed code may still reject an unsigned build.

## Windows

In File Explorer, copy the products from the extracted `out` folder to the
following locations. These shared plugin folders usually require administrator
permission; accept the copy prompt and create missing folders as needed.

| Product in `out/` | Destination folder |
| --- | --- |
| `CheapSynth01.vst3` (entire folder) | `C:\Program Files\Common Files\VST3\` |
| `CheapSynth01.clap` | `C:\Program Files\Common Files\CLAP\` |

Keep the complete VST3 folder structure. Do not copy only the executable inside
it. When updating, replace the old CheapSynth01 product with the new one.

If your DAW supports a custom scan folder, you can instead use a folder you own
and add it to the DAW's plugin search paths. Check the DAW's documentation for
support and configuration of VST3 or CLAP search paths.

For standalone use, keep `CheapSynth01.exe` and any accompanying runtime files
together in a folder you own, then launch the executable. You can create a
shortcut to it; a DAW is not required.

### Unsigned builds

The current release workflow does not sign Windows products. SmartScreen may
show a warning when launching the standalone application. If you trust the
release, use **More info > Run anyway** when that option is available.

If Windows reports that a downloaded file is blocked, right-click it, open
**Properties**, and select **Unblock > Apply** if shown. You can also unblock the
downloaded ZIP before extracting it. Managed computers may enforce policies
that do not allow unsigned products.

## Load the plugin in your DAW

1. Restart the DAW after installation.
2. Enable the installed format and check its scan folders in the DAW's plugin
   settings. Run a plugin rescan; if CheapSynth01 previously failed validation,
   use the DAW's option to retry failed or blocked plugins.
3. Find **CheapSynth01** in the instrument list and load it on an instrument or
   MIDI track. Configure MIDI input and track monitoring in the DAW.

If the plugin is missing, check that the complete product is in the correct
folder, the DAW supports its format, and the build's CPU architecture matches
the host. On Apple Silicon, a DAW running natively and a DAW running under Rosetta
may need different compatible builds. For AUv2 discovery problems, logging out
and back in can refresh macOS's Audio Unit registration before another scan.
Check the DAW's scan or validation log for the specific failure.

Native installation and host validation are still
pending; see [native validation status](docs/iPlug2-migration-plan.md#remaining-runtime-checks).
For controls, audio/MIDI settings and presets, see the
[user guide](docs/User-guide.md).

## Uninstall

Close the DAW and standalone application, then remove the CheapSynth01 products
from the folders where you installed them. Rescan the DAW if it still lists the
plugin. User presets are stored separately and remain available; their locations
are listed in the [user guide](docs/User-guide.md#factory-and-user-presets).
