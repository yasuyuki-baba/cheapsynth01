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
persisted session format. State and preset payloads use the current XML format.

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

The portable suite has 92 cases (89 correctness regressions and three
observations). Patch tests deliberately stall a writer, publish 20,000 concurrent
changes, serialize during publication, and check cancellation, note-reset
ordering and allocation-free boundary copies. Prepared-host regressions cover
latest/repeated requests, factory sound and held-audio equivalence, cached user
sound, invalid chunks, pending-session identity and instance isolation.

Eight additional tests use the real VST3 SDK interfaces with simulated host
queues/streams. They cover queue IDs, every MIDI channel, original point offsets,
invalid program values/IDs, read-only COM interfaces, short state reads, bypass
trailers, malformed/oversized state and raw XML/empty stream handling. Native
product CI runs these in addition to the core suite. They do not load a plugin
binary, instrument SDK callback threads, open an editor, or establish DAW behavior.
Actual host tests must still exercise AU/VST3 lists, panel/host synchronization,
zero-frame callbacks, concurrent state/automation and restoration with/without UI.

## Native parameter and state APIs

As agreed on 2026-10-11, JUCE-era project, automation and binary-state
compatibility is outside scope. Plugin identities remain unchanged, but all
formats use their standard iPlug2 parameter IDs. No legacy ID adapter, hash
manifest, migration aliases or old-session fixture gate is maintained.

`PluginAPI.h` inherits metadata, parameter queries, edit notifications, MIDI
assignment and program metadata directly from iPlug2. Its input-queue filter
passes the original native parameter/MIDI queues through unchanged, checks ID
bounds and handles program selection missing from the pinned SDK's process path.
It does not allocate or modify host queues. Normal automation retains the SDK's
last-point processing semantics.

The VST3 state entry reads the complete bounded stream, including final short
reads, separates iPlug2's four-byte bypass trailer and restores the XML payload.
The default SDK entry previously sought past the trailer because our XML reader
consumed the whole chunk. New-version state round trips, automation and host
program selection in actual native hosts remain validation gates. JUCE binary
XML, private tails and VST2 envelopes are not supported.
