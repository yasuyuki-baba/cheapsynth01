# Host preset integration

The processor prepares factory and user XML presets before SDK construction.
The SDK receives the correct program count and UTF-8 names without requiring an
editor. AUv2 and the enabled VST3 program list use the SDK's preset interfaces;
the pinned CLAP wrapper has no equivalent preset-discovery/load extension.
CLAP and standalone retain the panel's preset controls.

Each SDK preset contains a versioned 12-byte internal request, not XML. The
request validates its index and publishes a token into an immutable, per-instance
`HostProgramBank`. At the next audio block boundary, including zero-frame
callbacks, the audio thread applies the prepared patch. Multiple requests
coalesce to the latest valid selection. Re-selecting the same program restores
its sound again. Volume, breath input, modulation and pitch bend are preserved;
held notes continue as with panel selection. No disk access, XML, heap operation,
mutex or catalogue reclamation occurs in the core request/apply path.

Host names, count and sound snapshots remain fixed for the instance lifetime.
Panel saves, renames and deletes retain their existing filesystem behavior, but
refresh the host catalogue on the next instance launch. A cached host selection
can therefore still play a renamed/deleted file's prepared sound. Session XML
stores that filename/type and actual patch; if the file is absent on restoration,
the patch remains and the panel selection falls back to Default. A panel-only
sound added after startup has no host program slot in this instance. Host selection
identity is matched to the current panel list by filename/type, not sorted index.

Saving before a queued selection is rendered serializes its requested patch and
identity, with the live volume. After application, edits to that patch are saved.
Panel preset/import and successful XML session restore supersede queued host
selection. Session output remains XML; the internal request format is not a new
persisted session format. Legacy raw/binary JUCE XML decoding remains supported.

The processor overrides `OnRestoreState()` to schedule UI work. Parameters are
atomic; `OnIdle()` updates controls, selection labels and filter visibility.
The VST3 controller's separate normalized cache is refreshed there too. This
also avoids relying on the generic SDK API parameter queue, which is not drained
by the pinned non-distributed VST3 timer branch. Panel changes use the SDK's
`DirtyParametersFromUI()` mechanism.

## Evidence and remaining gates

Nine additional portable-core regressions check deferred/latest/repeated
selection, factory parameter and held-audio equivalence, cached user sound,
invalid files/chunks, pending-session identity, cancellation, sorting identity,
concurrent panel refresh and instance isolation. The allocation probe checks
4096 request/apply pairs. Debug, Release and ASan/UBSan pass all 62 cases.
Real SDK headers compile the Windows processor/editor paths, including VST3.
These core assertions and compilation do not establish actual SDK callback
thread behavior or DAW program selection.

Arbitrary XML session import still parses and writes live atomic parameters
through `ProgramManager`; coherent whole-state publication during rendering is
pending. The state serializer is not a real-time API and concurrent saves/edits
still require snapshot work. The SDK's own preset-index/controller thread behavior
needs native instrumentation. Full host tests must exercise AU/VST3 program
lists, panel/host selection synchronization, zero-size callbacks, state restore
and automation with and without an editor.

## Legacy automation identifiers

Plugin identities are retained, but parameter identities are still different.
The [numeric inventory](iPlug2-legacy-parameter-ids.csv) derives the original
24 IDs/order from frozen main `32b91c8`, pinned JUCE 9.0.3 and its default
`JUCE_USE_STUDIO_ONE_COMPATIBLE_PARAMETERS=1`. JUCE VST3/AUv2 use
`String::hashCode()` (32-bit multiply by 31) with the high bit cleared. Original
CMake does not enable `JUCE_FORCE_USE_LEGACY_PARAM_IDS`. iPlug2 uses indices.
The CSV records derived IDs, not a native-host capture or implemented mapping.

In the pinned SDK, VST3 parameter creation, controller queries, process events,
MIDI assignment and host notifications assume index IDs. AU also expects index
addresses. A mapping must cover all directions through a reproducible adapter or
SDK patch, not only parameter registration. Preserve the SDK checkout/pin while
implementing that adapter. CLAP IDs must be independently captured from the
original `clap-juce-extensions` pin `55525c9`; do not infer them from AU/VST3.
Native old-project sound/automation fixtures remain the final compatibility gate.
