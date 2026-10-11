# Release archives

## Preparing 0.4.0

The release tag is `v0.4.0`. CMake, `CheapSynth01/config.h`, Windows resource
versions and macOS bundle metadata must all describe 0.4.0 before tagging.
The plugin's packed version is `0x00000400`. Release CI rejects archives whose
version or source commit differs from the tag checkout.

Before publishing:

1. Merge the release preparation changes and require successful lint, core,
   native Windows/macOS and source-package CI for the final revision.
2. Complete and record the pending [native runtime and installation checks](iPlug2-migration-plan.md#remaining-runtime-checks),
   including scanning/loading, automation, current state save/reload, editor
   controls, standalone devices and 16-sample latency compensation.
3. Prepare signed/notarized products when distributing signed builds, and verify
   the resulting archives as described below.
4. Review the [0.4.0 release notes](release-notes-0.4.0.md), especially old-session
   incompatibility and changed envelope timing. Update validation status when
   actual runtime and signing results are available.
5. Create and push `v0.4.0` at the verified release revision. **Pushing a tag
   triggers CI to publish a GitHub Release automatically** after its checks pass.
   CI requires `docs/release-notes-0.4.0.md` and uses it as the release
   description, alongside generated commit-based notes.

## Creating and verifying archives

`scripts/release_package.py` packages built artifacts without installing or loading
plugins. Native products support Windows (APP/VST3/CLAP) and macOS
(APP/VST3/AUv2/CLAP). The iPlug2 submodule and fetched SDK/dependency sources must
match their recorded commits. Commit tracked project changes before packaging.
The setup script's restored SDK placeholder notices are verified against iPlug2.

```
python scripts/release_package.py create --build build-native --platform macOS --output release-packages/cheapsynth01-macOS.zip
python scripts/release_package.py create --build build-native --platform source --output release-packages/cheapsynth01-source.zip
python scripts/release_package.py verify --commit FULL_COMMIT_SHA --version 0.4.0 --platform source release-packages/cheapsynth01-source.zip
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


## Optional signing and notarization preparation

Signing is opt-in and uses a certificate already installed on the native machine.
`scripts/sign_products.py` prints its commands by default and does not read private
keys or certificate passwords. Inspect the plan before adding `--execute`:

```sh
python scripts/sign_products.py --platform macOS --output build-native/out --identity "Developer ID Application: Your Team"
python scripts/sign_products.py --platform Windows --output build-native/out --identity YOUR_40_HEX_CERTIFICATE_THUMBPRINT
```

Windows signs/verifies APP, CLAP and the inner VST3 executable with SHA256 and a
trusted timestamp. macOS signs nested Mach-O code before enclosing APP/VST3/AUv2/
CLAP bundles, with hardened runtime and timestamp, then verifies nested signatures.
The script requires its native platform to execute signing. Dry-run fixture tests
check product discovery, command order and escaping symlink rejection; no products
are signed by ordinary CI. Certificates/SDK tools must be provisioned separately.

For macOS notarization, prepare a `notarytool` keychain profile outside the
repository. After signing, create the macOS archive and submit that exact ZIP:

```sh
xcrun notarytool submit release-packages/cheapsynth01-macOS.zip --keychain-profile CheapSynth01 --wait
```

Check the returned acceptance result. ZIP files cannot themselves be stapled;
plugin bundles rely on Apple's online notarization ticket. If distributing the
standalone APP for offline launch, staple its accepted ticket with `xcrun stapler
staple build-native/out/CheapSynth01.app`, validate the ticket, then recreate and
verify the archive so its manifest includes the stapled contents. These commands
are release hooks, not evidence of a completed signed/notarized release. Validate
real products and installation before publishing a release; those runtime checks
are currently deferred. No credentials or release tags are created by this migration.
