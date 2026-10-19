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
not provide this backend. The currently tested baseline is upstream PR #1336,
commit `03d3c1af13d3456b09df1e94800dc48546f3aaef`, with the included
`patches/iplug2-linux-pr1336.patch`. The patch fixes APP resource compilation and
GIO linkage, resize clipping, programmatic APP resizing, and `OnIdle()` access
to partially constructed/destroyed editors. It serializes idle callbacks with
the PR's existing graphics mutex; it does not move callbacks to the main thread.
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

Products are written to `build-linux-native/out`, with deployment disabled. The
`linux-standalone` configure/build/test presets build APP only; pass the same
external `IPLUG2_DIR` when configuring them. `Xvfb` and `xauth` enable the X11
keyboard-focus regression test; without `xvfb-run` that test is not registered.
GTK3 is an iPlug2/SWELL dependency; the synth does not call GTK directly.

Launch `build-linux-native/out/CheapSynth01` in an X11 desktop session for audio
and MIDI device setup. Adding `--no-io` disables audio/MIDI device I/O and is
useful for GUI-only smoke tests; it does not verify audio output. Keep the
accompanying resources directory with the executable/plugin products. User
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
Do not suppress these failures or treat the suite as fully passing. Real audio
devices, DAW-hosted VST3/CLAP, x86_64, HiDPI and native Wayland have not been
validated. Linux packages are not produced by the release workflow yet.

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

Read [the migration notes](docs/iPlug2-migration.md) before comparing sound or
opening an existing project. Keep runtime DSP free of allocations and GUI/file
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
live in `CheapSynth01/tests/`. Historical implementations and documents are available in Git
history; they are not maintained in the working tree.

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
