# CheapSynth01 0.5.2

CheapSynth01 0.5.2 adds experimental Linux desktop products and automated Linux
builds, tests and release packages.

## Changes

- **Linux APP, VST3 and CLAP:** X11 keyboard-focus handling, XDG user preset
  storage and standalone startup regression tests. Linux uses NanoVG/GL3.
- **Shared iPlug2 fork:** all desktop platforms use the pinned
  `yasuyuki-baba/iPlug2` revision `9979d442dd393b546c8fe5d9210f4c121ee8a3c8`,
  including the Linux backend and upstream fixes. Separate Linux compatibility
  patches are no longer needed.
- **Linux CI and release archives:** Ubuntu 24.04 x86_64 builds all three Linux
  products. Xvfb tests cover keyboard focus and standalone startup. Linux ZIPs
  include the adjacent font resources and preserve executable permissions.
- **Manufacturer display name:** the displayed manufacturer is now `babab`.
  The `BABA` manufacturer identifier and existing plugin IDs are preserved.

The filter behavior introduced in 0.5.1 is retained. See the
[0.5.1 release notes](release-notes-0.5.1.md) for those sound changes.

## Platforms and validation

- Windows: standalone APP, VST3 and CLAP.
- macOS: standalone APP, VST3, AUv2 and CLAP (macOS 10.15 or later).
- Linux (experimental): standalone APP, VST3 and CLAP; Ubuntu 24.04 x86_64,
  X11 and OpenGL. Native Wayland and other distributions are not validated.

The preceding CI revision built and packaged all three desktop platforms. Linux
passed all 299 tests, including Xvfb keyboard-focus and standalone-startup tests.
Release CI rebuilds and tests the tagged revision, verifies native/source ZIPs
against the release version and commit, and checks an offline core rebuild from
the source archive before publication.

Builds are unsigned; macOS notarization is not performed. Real DAW and physical
audio/MIDI device validation remains pending. Automated tests do not establish
hardware emulation accuracy or compatibility with every host.

See [installation instructions](https://github.com/yasuyuki-baba/cheapsynth01/blob/v0.5.2/INSTALL.md).
