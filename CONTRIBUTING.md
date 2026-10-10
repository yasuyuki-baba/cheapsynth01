# Development

CheapSynth01 uses C++20 and iPlug2. The audio engine is a separate static library;
it can be developed and tested without building a plugin.

## Core tests (Windows/macOS)

Requires CMake 3.20+, a C++20 compiler, and Git. CMake fetches the pinned TinyXML2
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
requires Xcode command-line tools. Add `-DSTANDALONE_ONLY=ON` for an APP-only
build. `-DIPLUG2_UNIVERSAL=ON` builds Intel/Apple Silicon binaries on macOS.
Products go into `build-native/out`. Installation is disabled by default; set
`IPLUG_DEPLOY_PLUGINS=ON` only when you intend to install them.

Supported products are APP, VST3 and CLAP on both platforms, plus AUv2 on macOS.
Development and CI testing target Windows and macOS.

## Standalone audio and MIDI settings

The APP product uses iPlug2's built-in Preferences dialog. Open
`File > Preferences…` (`Ctrl+,`) on Windows or `CheapSynth01 > Preferences…`
(`Cmd+,`) on macOS. Select the audio output device/channels, sample rate, buffer
size and MIDI input device/channel there. The host saves settings to its
`settings.ini` and restores them on the next launch. These settings belong to
the standalone host; plugin formats use their DAW's device settings.

The macOS SWELL dialog/menu definitions in `resources/main.rc_mac_*` are generated
from the shared Windows resource script. Regenerate them after changing dialogs
or menu commands:

```sh
perl libs/iPlug2/WDL/swell/swell_resgen.pl resources/main.rc
```

Read [the migration notes](docs/iPlug2-migration.md) before comparing sound or
opening an existing project. Keep runtime DSP free of allocations and GUI/file
operations. The engine processes mono internally and duplicates its final output
for stereo. Parameter indices in `Source/Parameters.h` are permanent host IDs;
append future parameters, do not reorder existing ones.

Native validation includes opening the editor, all MIDI controls, XML import and
user preset CRUD, host automation/state save and restore, plugin scanning,
`auval -v aumu CS01 BABA` on macOS, and latency compensation in a real DAW.
