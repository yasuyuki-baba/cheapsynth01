# Release archives

`scripts/release_package.py` packages built artifacts without installing or loading
plugins. Native products support Windows (APP/VST3/CLAP) and macOS
(APP/VST3/AUv2/CLAP). The iPlug2 submodule and fetched SDK/dependency sources must
match their recorded commits. Commit tracked project changes before packaging.
The setup script's restored SDK placeholder notices are verified against iPlug2.

```
python scripts/release_package.py create --build build-native --platform macOS --output release-packages/cheapsynth01-macOS.zip
python scripts/release_package.py create --build build-native --platform source --output release-packages/cheapsynth01-source.zip
python scripts/release_package.py verify --commit FULL_COMMIT_SHA --version 2.0.0 --platform source release-packages/cheapsynth01-source.zip
```

Each archive contains the project license, dependency notices and BUILD-INFO.json.
The manifest lists every payload file's SHA-256 and Unix mode, including symlink
content. It also records product/platform/version, full project revision and
SDK/dependency commits. Archives reject duplicate/unlisted/missing files, unsafe
paths, escaping symlinks, modified file modes and mismatched release identity.
Hashes detect corruption; they are not signatures. Signing/notarization is not
performed by this packager. Real plugin/host validation is currently deferred.

The source ZIP includes tracked project files, iPlug2 and its fetched plugin SDKs,
TinyXML2 and test-only Google Test. Git metadata and build products are excluded.
Configure the extracted source without running the SDK setup script:

```
cmake -S . -B build-offline -DBUILD_PLUGINS=OFF -DBUILD_VST3_BOUNDARY_TESTS=ON -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DFETCHCONTENT_SOURCE_DIR_TINYXML2="$PWD/dependencies/tinyxml2" -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/dependencies/googletest"
cmake --build build-offline --parallel 2
ctest --test-dir build-offline --output-on-failure
```

For native compilation on Windows/macOS, use BUILD_PLUGINS=ON and the corresponding
platform generator. CI independently rebuilds/tests the extracted core and SDK
interface tests with FetchContent disconnected, verifies native archives, and
checks every release ZIP against the tag version and checkout SHA before producing
SHA256SUMS.txt. Archive checks run on ordinary branch/PR builds as well as tags.

Windows Git text attributes can report the restored CLAP SDK placeholder notices
as modified. Only the named SDK README/readme placeholders may differ in LF/CRLF
representation, and their complete text must still match iPlug2's fixed Git blob.
Other SDK modifications continue to block packaging.
