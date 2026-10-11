# MIDI realtime control flow

`MidiProcessor` owns a fixed 128-key bitset and selects the highest held note.
The first held key starts EG; additional keys change pitch without retriggering.
Duplicate note-ons coalesce, unmatched releases are ignored and velocity is
passed through but does not alter synthesized amplitude. MIDI channels address
one shared voice, not separate per-channel voices.

Every mapped MIDI parameter is written synchronously into audio-owned
`cs01::ParameterState` using production normalization/snapping/skew. A fixed
atomic bitmask records changed parameters. At callback end, the wrapper publishes
changed values to the delegate parameters and sets an idle-refresh flag; it does
not send UI messages from rendering. `OnIdle` refreshes IGraphics and the VST3
controller cache. Native callback/notification thread behavior remains deferred
runtime verification; core probes do not establish SDK allocation/lock behavior.

`MidiQueue` is audio-owned, has 2048 fixed slots and stable equal-offset ordering.
Negative offsets clamp to zero before insertion. Due zero-offset events apply
even at an empty callback; events at/beyond the block end carry to the next block.
Overflow discards the batch and triggers panic at the next boundary. UI notes
use a separate per-note atomic desired-state bridge with PC/mouse ownership, so
a burst cannot lose a note-off by overflowing an event queue. UI cleanup releases
only UI-owned notes; CC120/123 clear shared UI/external voice ownership.

CC120 clears notes, EG and filter/VCA/converter history immediately. CC123 uses
normal release. CC121 centers bend and resets modulation/breath pair caches while
preserving held keys, volume and patch. Session saves capture current MIDI edits
without waiting for an editor/notification callback. Whole XML patches are
validated off audio and copied coherently at boundaries; their note-reset serial
is captured with the patch. Live scalar edits remain separate.

See [the implementation chart](MIDI-Implementation.md), [host/state publication](iPlug2-host-integration.md)
and [active assertions](../Tests/README.md). Current stage ownership and modeling
limits are described in [the DSP design guide](DSP-design.md). Earlier framework
implementation and lock measurements are available in Git history.
