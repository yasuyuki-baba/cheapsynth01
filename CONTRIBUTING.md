# Development

CheapSynth01 uses C++20 and iPlug2. The audio engine is a separate static library;
it can be developed and tested without building a plugin.

## Core tests (Windows/macOS/Linux)

Requires CMake 3.25+, a C++20 compiler, and Git. CMake fetches the pinned TinyXML2
10.0.0 and Google Test 1.14.0 dependencies on first configuration.

```sh
cmake -S . -B build-core -DBUILD_PLUGINS=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build-core --config Debug --parallel 2
ctest --test-dir build-core -C Debug --output-on-failure
```

## Desktop products (Windows/macOS)

Initialize iPlug2 and its SDKs with Git Bash on Windows or bash on macOS:

```sh
bash scripts/setup-iplug2.sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release -DBUILD_PLUGINS=ON -DIPLUG_DEPLOY_PLUGINS=OFF
cmake --build build-native --config Release --parallel 2
ctest --test-dir build-native -C Release --output-on-failure
```

Windows requires Visual Studio with C++ desktop tools and Windows SDK; macOS
requires Xcode command-line tools. The deployment target is macOS 10.15 or
newer because user preset/state paths use `std::filesystem`. Add `-DSTANDALONE_ONLY=ON` for an APP-only
build. `-DIPLUG2_UNIVERSAL=ON` builds Intel/Apple Silicon binaries on macOS.
Products go into `build-native/out`. Installation is disabled by default; set
`IPLUG_DEPLOY_PLUGINS=ON` only when you intend to install them.

Supported products are APP, VST3 and CLAP on both platforms, plus AUv2 on macOS.
Development and CI testing target Windows and macOS.

## Linux (experimental)

The Linux presets use Ninja and default to Release. Core-only builds do not need
iPlug2 or a display:

```sh
cmake --preset linux-core
cmake --build --preset linux-core
ctest --preset linux-core
```

Native products require an X11-capable iPlug2 checkout. The pinned submodule does
not provide this backend. The patch baseline is upstream PR #1336,
commit `03d3c1af13d3456b09df1e94800dc48546f3aaef`, with the included
`patches/iplug2-linux-pr1336.patch`. The patch reproduces the tracked tree of
`yasuyuki-baba/iPlug2` commit `d12749072ec94ef6f1bdcb803daa119579d0696e`
(`fix/linux-pr1336-app-build`), including its merged upstream changes. It includes
APP build and startup fixes, editor resizing, idle/host lifecycle synchronization,
clipboard and dialog handling, resource isolation, path validation, and Linux
regression tests. It supersedes the earlier APP-focused compatibility patch.
Do not apply it to another iPlug2 revision without reviewing the differences.
The regular `scripts/setup-iplug2.sh` still initializes the pinned Windows/macOS
submodule, not this external Linux checkout.

For Ubuntu 24.04, install the native development prerequisites:

```sh
sudo apt-get install build-essential cmake ninja-build git curl pkg-config python3 \
  libfreetype-dev libfontconfig-dev libx11-dev libxi-dev libxext-dev \
  libxrender-dev libxrandr-dev libxfixes-dev libxcursor-dev libgl-dev \
  libgtk-3-dev libasound2-dev libjack-jackd2-dev libpulse-dev xvfb xauth
```

From the CheapSynth01 repository root, in bash:

```sh
git clone --no-checkout https://github.com/iPlug2/iPlug2.git ../iPlug2-linux
git -C ../iPlug2-linux fetch origin pull/1336/head
git -C ../iPlug2-linux checkout --detach 03d3c1af13d3456b09df1e94800dc48546f3aaef
git -C ../iPlug2-linux apply "$PWD/patches/iplug2-linux-pr1336.patch"
pushd ../iPlug2-linux/Dependencies/IPlug
bash download-vst3-sdk.sh
bash download-clap-sdks.sh
popd
cmake --preset linux-native -DIPLUG2_DIR="$(realpath ../iPlug2-linux)"
cmake --build --preset linux-native
ctest --preset linux-native
```

If using an existing checkout of `yasuyuki-baba/iPlug2` at
`d12749072ec94ef6f1bdcb803daa119579d0696e`, skip patch application and pass that
checkout as `IPLUG2_DIR` instead. For example, with the sibling local checkout:

```sh
cmake --preset linux-native -DIPLUG2_DIR="$(realpath ../iPlug2)"
cmake --build --preset linux-native
ctest --preset linux-native
```

The external checkout still needs the VST3 and matching CLAP SDKs described above.
The Windows/macOS submodule and `scripts/setup-iplug2.sh` are unchanged.

Products are written to `build-linux-native/out`, with deployment disabled. The
`linux-standalone` configure/build/test presets build APP only; pass the same
external `IPLUG2_DIR` when configuring them. `Xvfb` and `xauth` enable the X11
keyboard-focus and standalone-startup regression tests; without `xvfb-run` those
tests are not registered. The startup test builds and launches the actual APP.
GTK3 is an iPlug2/SWELL dependency; the synth does not call GTK directly.

Launch `build-linux-native/out/CheapSynth01` in an X11 desktop session for audio
and MIDI device setup. Adding `--no-io` disables audio/MIDI device I/O and is
useful for GUI-only smoke tests; it does not verify audio output. Keep the
accompanying resources directory with the executable/plugin products. User
audio/MIDI settings use `$XDG_CONFIG_HOME/cheapsynth01/settings.ini`, falling back
to `$HOME/.config/cheapsynth01/settings.ini` if `XDG_CONFIG_HOME` is unset, empty
or relative. Missing ancestors are created with private permissions (0700,
subject to the process umask); existing directory permissions are preserved.
An absolute configuration base is required. Directory or settings-write failures
are reported to stderr and terminate startup with exit code 1, before audio or
editor initialization. Audio-device selection failures after successful host
initialization retain the SDK's existing settings-recovery behavior. User
presets use `$XDG_DATA_HOME/CheapSynth01/UserPresets`, falling back to
`~/.local/share/CheapSynth01/UserPresets` if `XDG_DATA_HOME` is unset, empty or
relative.

Local validation used Ubuntu 24.04 ARM64, GCC 13.3, Xvfb and Mesa llvmpipe:
APP/VST3/CLAP builds, APP drawing, preset switching, fader changes, keyboard
expansion/collapse, PC-key highlighting, XML Save As, and normal exit/reopen.
The native suite has 255 CTest entries (including the Xvfb focus test); three
existing `SignalPathTest` numeric-reference comparisons fail on this setup:

- `BehavioralModelsMatchFrozenArithmeticIncludingSafetyExactly`
- `OriginalCombinedModulationMatchesIndependentSemitoneCutoff`
- `ModernWrapperMatchesFrozenCircuitArithmeticAndReset`

All three also fail on the unmodified CheapSynth01 commit
`8517b92aca28dccdd6aa3cc8ef7db12185403c5e` in the same ARM64 Release environment.
Do not suppress these failures or treat that ARM64 suite as fully passing.
That run did not validate real audio devices, DAW-hosted VST3/CLAP, x86_64, HiDPI
or native Wayland. Linux packages are not produced by the release workflow yet.

Additional validation on 2026-10-20 used commit `4aa40d0`, Ubuntu 24.04.4 x86_64
under WSL2/WSLg, GCC 13.3 and Release builds with the patched iPlug2 baseline above.
The VST3 SDK was `v3.8.0_build_66`; CLAP SDK and helpers used the revisions pinned
in `scripts/setup-iplug2.sh`. Mixing older CLAP headers with current helpers caused
compile errors; using the matching pinned revisions resolved them.

- Core suite: all 241 CTest entries passed.
- Native suite: all 255 entries passed, including the Xvfb keyboard-focus test,
  parameter presentation and VST3 SDK boundary cases. The three ARM64 numerical
  failures listed above did not reproduce in this x86_64 environment.
- APP, VST3 and CLAP built successfully, with no missing shared libraries.
  WSLg window creation was checked. Automated Xvfb smoke checks verified nonblank
  drawing, next-preset display changes, keyboard expansion/collapse (400/640
  pixels), PC-key press/release highlighting and normal exit/reopen, both with
  `--no-io` and with device I/O enabled.
- APP audio was captured from a dedicated PulseAudio null sink through the ALSA
  default device at 44.1 kHz, with a 512-frame application buffer. PC-key notes
  produced a captured peak of approximately 0.0662, with no nonfinite samples.
  This verifies the software audio path, not physical playback or latency.
- The actual VST3 binary loaded and exposed its factory/class metadata.
  A minimal local CLAP host loaded the actual binary, activated stereo processing
  at 48 kHz/256 frames, and verified initial silence, finite identical left/right
  output after MIDI note-on (peak approximately 0.2030), and silence after note-off.
  These are smoke checks, not DAW-hosted plugin/editor validation.

The initial validation also reproduced SIGSEGV (exit 139) when the configuration
parent did not exist: settings initialization failed, but APP startup continued
with no audio driver. The compatibility patch now fixes this by creating missing
ancestors, checking initialization, and rejecting uninitialized audio-device
access. Its SWELL INI writer propagates open, buffered-write, close and rename
errors instead of reporting successful saves; failed writes leave existing
settings intact and invalidate the in-memory cache.

The fix was verified with 16 executable-level Xvfb startup cases, registered as
one additional CTest entry (256 total native entries). They cover absent parents,
private/new versus preserved/existing permissions, unset/empty/relative XDG
paths, unavailable configuration bases, reopen/settings restoration, directory
and file-write failures, and invalid audio drivers. Successful startup cases use
`--no-io`; failure cases also exercise ordinary device-enabled startup. The
existing keyboard-focus test still passes. APP/VST3/CLAP builds and patch
application against the exact SDK baseline were also checked. Apply the updated
patch to the clean baseline, not over an older version of the patch.

WSL2 also lacked `/dev/snd/seq`; APP logged ALSA
sequencer initialization errors but continued running with PC-keyboard audio.
Physical audio/MIDI devices, real DAW hosting, HiDPI and native Wayland remain
unverified.

Validation on 2026-10-22 with iPlug2 commit `d12749072` used Ubuntu 24.04.4
x86_64 and a Release build. Applying the regenerated patch to the baseline
reproduced that commit's complete tracked Git tree exactly. APP, VST3 and CLAP
built successfully against the local SDK checkout, and all 256 native CTest
entries passed, including the Xvfb startup and keyboard-focus regressions.
The iPlug2 Linux regression suite also passed all 56 entries, covering APP,
VST3/CLAP host lifecycle and failure paths, dialogs, clipboard, timers, SDK
detection, and COPY/SYMLINK deployment with resource isolation.

Additional smoke tests loaded the actual CheapSynth01 VST3 and CLAP products
into iPlug2's minimal test hosts: all eight VST3 modes and eleven CLAP modes
passed. The temporary host probes only adjusted editor-size detection for the
400-pixel-high synth UI and the VST3 gesture position to its LFO speed fader.
Checks included rendering, repeated editor open/close, native close/reopen,
VST3 parameter gestures, host-loop switching, callback fallbacks, and injected
initialization failures. These are Xvfb/software-rendering checks, not real DAW
validation. This run did not repeat audio capture or physical device testing.

## Standalone audio and MIDI settings

The APP product uses iPlug2's built-in Preferences dialog. Open
`File > Preferences…` (`Ctrl+,`) on Windows or `CheapSynth01 > Preferences…`
(`Cmd+,`) on macOS. Select the audio output device/channels, sample rate, buffer
size and MIDI input device/channel there. The host saves settings to its
`settings.ini` and restores them on the next launch. These settings belong to
the standalone host; plugin formats use their DAW's device settings.

The macOS SWELL dialog/menu definitions in `CheapSynth01/resources/main.rc_mac_*` are generated
from the shared Windows resource script. Regenerate them after changing dialogs
or menu commands:

```sh
perl iPlug2/WDL/swell/swell_resgen.pl CheapSynth01/resources/main.rc
```

Read [the architecture notes](docs/iPlug2-migration.md) for implementation details.
Keep runtime DSP free of allocations and GUI/file
operations. The engine processes mono internally and duplicates its final output
for stereo. Parameter indices in `CheapSynth01/core/Parameters.h` are permanent host IDs;
append future parameters, do not reorder existing ones.

Native validation includes opening the editor, all MIDI controls, XML import and
user preset CRUD, host automation/state save and restore, plugin scanning,
`auval -v aumu CS01 BABA` on macOS, and latency compensation in a real DAW.

## Out-of-source project layout

The build follows iPlug2OOS: a thin root entry includes the pinned `iPlug2/`
submodule and adds `CheapSynth01/`, whose `CMakeLists.txt` calls
`iplug_add_plugin` with an explicit desktop format list. `CheapSynth01/config.h`
and `CheapSynth01/resources/` belong to the plugin; `CheapSynth01/core/` owns the
independent core target. Native IDE projects are generated by CMake rather than maintaining
additional copies of SDK/common IDE settings.

The product directory follows the official
[iPlug2OOS template](https://github.com/iPlug2/iPlug2OOS/tree/9b214c1121a0da1e8db14b86a565c942a3bfea7e/TemplateProject):
product CMake, `config.h`, implementation and `resources/` stay together.
`core/` and `tests/` inside the product directory are project-specific extensions;
the template does not prescribe a unit-test directory. Internal directory names
use lowercase, following the template's `resources/` and `scripts/` convention.
Core-only builds enter the same product directory and build its independent
target without initializing iPlug2.

The plugin class and its main source files use the product name, `CheapSynth01`,
following TemplateProject. UI implementation lives in `CheapSynth01_UI.cpp`.
The independent core uses `prepare(sampleRate)` and `reset()` for DSP lifecycle.
`setPitchBend(value)` accepts raw MIDI pitch bend (0..16383, center 8192);
`setPitchBendSemitones(value)` explicitly accepts a semitone offset.

```text
iPlug2/                  # Pinned SDK submodule
CheapSynth01/            # Product directory
  CMakeLists.txt
  config.h
  CheapSynth01.*
  CheapSynth01_UI.cpp
  ui/                    # IGraphics controls
  resources/             # Product and platform resources
  core/                  # Framework-independent DSP/state target
    dsp/
    cs01synth/
    interaction/         # Keyboard input and editor lifetime
    resources/presets/
  tests/                 # Core, SDK-interface and Python tests
```

Keep iPlug2-dependent code outside `CheapSynth01/core/`, including IGraphics controls in
`CheapSynth01/ui/`. `CheapSynth01/core/interaction/` contains framework-independent keyboard input
and editor-lifetime logic shared with the tests; it must not include iPlug2
headers. `CheapSynth01/core/resources/presets/` belongs to the core preset catalogue, while
`CheapSynth01/resources/` contains product and platform resources. Active tests
live in `CheapSynth01/tests/`.

With CMake 3.25+, use the platform presets, for example:

```sh
# macOS (universal native products, generated Xcode project)
cmake --preset mac-native
cmake --build --preset mac-native-release
ctest --preset mac-native-release
# Windows (generated Visual Studio 2022 x64 solution)
cmake --preset win-native
cmake --build --preset win-native-release
ctest --preset win-native-release
```

`mac-core`/`win-core` configure without the iPlug2 submodule; the
`mac-standalone`/`win-standalone` presets configure APP only. Both Debug and Release
build/test presets are available. Preset builds stay in separate `build-*`
directories and produce JUnit reports. Existing direct CMake commands remain valid.

### VS Code

Install the Microsoft C/C++ and CMake Tools extensions. Select a configure preset
with CMake Tools and configure the project; IntelliSense obtains the active
target's include paths and definitions from CMake Tools, including with Xcode and
Visual Studio generators that do not produce `compile_commands.json`.

To debug the standalone application, build `mac-native-debug` or
`win-native-debug`, then select the matching `iPlug2 standalone` launch
configuration. These configurations use the native presets' `build-*/out`
products. For standalone-only presets or a custom build directory, adjust the
launch configuration's `program` path to that build's output.
