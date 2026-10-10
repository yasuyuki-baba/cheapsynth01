# Third-party notices

- **iPlug2**: pinned Git submodule `libs/iPlug2`;
  [zlib-like license](libs/iPlug2/LICENSE.txt). The platform resource templates
  under `resources/` are adapted from its IPlugControls example; names, IDs and
  version metadata have been changed. Original copyright: the iPlug2 developers.
- **TinyXML2 10.0.0**: fetched by CMake;
  [zlib license](https://github.com/leethomason/tinyxml2/blob/10.0.0/LICENSE.txt).
- **Roboto Regular**: `resources/fonts/Roboto-Regular.ttf`, version 1.100141 (2013),
  font data copyright Google 2012; design by Christian Robertson. Licensed under
  Apache License 2.0; see `resources/fonts/LICENSE-Apache-2.0.txt`.
- **Google Test 1.14.0**: test-only dependency, BSD-3-Clause.
- **Plugin SDKs and iPlug2 dependencies**: follow the individual licenses in the
  fetched VST3/CLAP SDKs and iPlug2 dependency tree. They are independent of the
  project's GPLv3 license.

No JUCE implementation code is linked into the iPlug2 products. Historical
JUCE-specific project tests are retained only as reference scenarios.
