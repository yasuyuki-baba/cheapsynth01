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

## Whole-state publication

XML is validated outside the audio callback and published through a versioned
whole-patch writer. The renderer owns separate parameters and attempts one
bounded snapshot at each block boundary. If publication is in flight, it keeps
the previous complete snapshot. Note-reset serial and patch values are captured
together; a session clears held notes when its sound reaches rendering.
Preset changes preserve held notes. Explicit imports cancel cached host requests
before committing, and a request rechecks cancellation while holding the writer.
Serialization captures a complete patch and retries if host selection changes
while capturing its identity. XML parsing, filesystem operations and serialization
remain non-real-time operations; scalar automation/MIDI edits remain independent.

## Evidence and remaining gates

The portable suite has 72 cases (69 correctness regressions and three
observations). Patch tests deliberately stall a writer, publish 20,000 concurrent
changes, serialize during publication, and check cancellation, note-reset
ordering and allocation-free boundary copies. Prepared-host regressions cover
latest/repeated requests, factory sound and held-audio equivalence, cached user
sound, invalid chunks, pending-session identity and instance isolation.

Eight additional tests use the real VST3 SDK interfaces with simulated host
queues/streams. They cover queue IDs, every MIDI channel, original point offsets,
invalid program values/IDs, read-only COM interfaces, short state reads, bypass
trailers, malformed/oversized state and legacy binary payload forwarding. Native
product CI runs these in addition to the core suite. They do not load a plugin
binary, instrument SDK callback threads, open an editor, or establish DAW behavior.
Actual host tests must still exercise AU/VST3 lists, panel/host synchronization,
zero-frame callbacks, concurrent state/automation and restoration with/without UI.

## Legacy automation identifiers

Plugin identities are retained. The [numeric inventory](iPlug2-legacy-parameter-ids.csv)
derives 24 AUv2/VST3 IDs from frozen main `32b91c8`, JUCE 9.0.3 and default
`JUCE_USE_STUDIO_ONE_COMPATIBLE_PARAMETERS=1`: 32-bit multiply-by-31 string hashes
with the high bit cleared. Original CMake does not force legacy index IDs.

`PluginAPI.h` translates VST3 metadata, normalized/plain/string queries, edit
notifications, MIDI assignment and input queues in both directions. Program and
bypass IDs and JUCE's 16×130 MIDI window are preserved too. The early WIP index
IDs are accepted as input aliases; only JUCE IDs are advertised. Unknown IDs are
ignored instead of becoming accidental MIDI CCs. The adapter leaves the pinned
SDK unchanged. It preserves the SDK's last-point processing semantics.

The VST3 state entry reads the complete bounded stream, including final short
reads, separates iPlug2's four-byte bypass trailer and restores the XML payload.
Legacy JUCE binary XML is forwarded to its length-checked decoder; JUCE-private
bypass metadata and VST2-compatible envelopes are not yet migrated. The default
SDK entry previously sought past the trailer because our XML reader consumed
the whole chunk. Native state round trips and old-project fixtures remain gates.

AU parameter-address translation is still pending. CLAP IDs must be captured
independently from the original `clap-juce-extensions` pin `55525c9`; do not infer
them from AU/VST3. Old-project sound/automation fixtures remain the final
compatibility gate for every format.
