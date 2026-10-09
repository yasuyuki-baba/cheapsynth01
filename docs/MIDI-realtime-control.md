# MIDI realtime control flow

`MidiProcessor` keeps held notes in `std::bitset<128>`. A descending scan of at
most 128 bits selects the highest note. Duplicate note-ons still represent one
held key and invoke the existing legato change-note path; one matching note-off
removes that key. Unmatched releases are ignored. The first held key supplies
velocity and starts the envelope; legato notes retain the gate. All-notes-off
releases the envelope, while all-sound-off clears it immediately. This preserves
the tested software policy, not a claim about verified YM10150 arbitration.

All twelve mapped parameters (pitch bend plus eleven CC targets) are standard
JUCE `AudioParameterFloat` objects. Construction caches their pointers; missing
parameters in reduced test layouts are ignored. Custom subclasses are rejected:
the realtime path relies on the standard class's empty `valueChanged` hook.
`setValue` updates its lock-free atomic float with the existing normalized range
conversion, including snapping and skew. It does not notify listeners or the
host. One lock-free atomic pending flag per target records a notification request.
The audio thread does not post messages, allocate a queue, sort notes, or lock.
Compile-time assertions require lock-free float and bool atomics.

DSP reads the authoritative float through `getMidiParameterValue`, rather than
APVTS's listener-maintained raw cache. The small changes at DSP read sites only
change the source of parameter values; no oscillator, filter, amplifier or
envelope equations or hardware assumptions change. MIDI event segmentation in
the audio processor remains intact, so control changes affect the next rendered
segment immediately even when the message thread is stalled.

A 60 Hz JUCE message-thread timer exchanges pending flags and calls
`sendValueChangedMessageToListeners` with each parameter's current value. This
is the notification portion of `setValueNotifyingHost`, without rewriting the
parameter. It updates host/UI listeners and APVTS's raw cache and dirty state.
Intermediate MIDI notifications are coalesced; host/UI visibility can lag by a
timer interval or longer when the message thread is busy. DSP does not wait.
Polling is intentional: `AsyncUpdater::triggerAsyncUpdate` may block while
posting a system message from the audio thread.

The timer never replays an old parameter value, so newer MIDI or host edits
cannot be overwritten by dispatch. If MIDI arrives during dispatch it leaves a
pending flag for a later tick. Listener notifications may briefly describe an
earlier value during concurrent edits, but DSP always reads the current atomic
parameter. As before, concurrent host and MIDI parameter writes follow the
order in which their atomic stores occur. Destruction stops the timer; the
processor and APVTS must have their existing message-thread lifetime ordering.
Audio prepare/resource release resets held notes and the envelope, retaining
controller values and pending synchronization just as controls were retained
before this change.

APVTS's raw cache and `copyState()` can lag before the timer fires. DSP readers
must use the authoritative accessor for MIDI targets. `ProgramManager` uses
`copyCurrentMidiParameterState` for both patch and session saves: it overlays
current float values onto a copied APVTS tree, without dispatching notifications.
State capture retains its existing non-realtime contract. Saved values therefore
include MIDI edits made before a timer tick; identifiers, ranges, mappings and
preset units are unchanged.

Regression tests cover the entire note range, duplicates, unmatched releases,
legato/velocity, fallback, note/sound reset commands, every mapped CC and pitch
bend, delayed/coalesced message-thread notification, save-before-dispatch,
concurrent/reentrant edits and destruction with pending notification. Existing
sample-timed graph and envelope regressions remain the behavior checks.

## Historical concerns before the October stability audit

The following described commit `4f2e59d722ca2c761ce7a72d2f6ee719bcc9e900`.
The next section records the current implementation and remaining limits.

The surrounding audio callback still grows temporary `MidiBuffer` storage and
can allocate for long MIDI messages (including SysEx copies).
`MidiMessageCollector::removeNextBlockOfMessages` and
`MidiKeyboardState::processNextMidiBuffer` take JUCE critical-section locks.
Graph routing uses `AsyncUpdater::triggerAsyncUpdate` from parameter listeners,
whose message posting can block. These existing paths and state/preset operations
need independent realtime audits. General host automation still uses
JUCE's normal synchronous parameter listeners; this patch defers notifications
originating specifically from MIDI. Existing DSP parameter lookups and
coefficient work are unchanged. This refactor does not establish that the whole
synth callback is allocation-free or bounded in cost.

## October 2026 stability audit

`RealtimeMidiQueue` replaces the panel collectors and the audio-side keyboard
state injection. Each input/mirror queue has 2048 inline short-event slots; full
or contended input queues request panic (discard that queued batch, stop all voices,
reset audio tails). Long SysEx and unsupported messages are ignored. The host's
MIDI buffer is read without payload copies or a capacity cap; host Note Off is not
subject to the GUI queue overflow policy. Accepted host events retain positions
and same-position order; host events precede GUI events at equal positions.
Positions outside 0..N-1 clamp to 0 or N. At N events apply after rendering and
change the next nonempty block's state. A zero-sample block applies host events
without advancing DSP or draining the GUI queue. UI key highlighting follows
host MIDI on the message-thread timer; it no longer locks keyboard state on audio.

Choice UI listeners publish only atomic state. Routing reads the authoritative
choice values on its message-thread timer, without APVTS listener registration.
The VCO already polls feet when selecting its source on audio, so its redundant
APVTS subscription is also removed. This avoids the first-notification iterator
allocation in JUCE's ListenerList. Routing is still message-thread graph mutation,
polled at 60 Hz; it depends on message-loop service,
and is not a sample-accurate filter/LFO switch. A fixed graph was not adopted in
this patch: compare CPU with both filters processing nonzero input, test crossfade
semantics and source/host compatibility before a separate routing redesign.

Host program calls from any thread select an immutable, pre-parsed
catalogue entry. The next audio callback applies the parameter values without XML,
I/O or notifications; the message-thread timer sends current-value notifications.
Latest valid request wins. The reserved selection is immediately visible to
host queries; sound values apply at the next callback, including an empty block.
Session capture before application serializes the reserved patch and identity
together. A successful explicit UI/session load supersedes a queued request.
The host entry point does not query the
MessageManager or branch into UI loading. User files are cached at refresh/save/rename, so external file
edits need a refresh. UI selections still validate and read the actual file before
committing selection. Catalogues retain queued/selected entries and readers;
allocation/reclamation happens on the message thread. Preset identity is filename
plus type, not a mutable ordinal. Missing preset parameters use JUCE defaults,
and live-control exclusions are unchanged.

The measured callback probe covers C++ new/delete, malloc/calloc/realloc/free
and pthread_mutex_lock calls linked into the Linux test executable. It does not
cover every allocation inside shared libraries, all possible host automation
callbacks, scheduling or system calls. JUCE graph nodes still acquire callback
mutexes. The host program entry point avoids JUCE 9.0.3's mutex-protected
`isThisTheMessageThread` entirely by separating it from the explicit editor path.
The unprotected thread-ID getter is not used.

All production slider/button/combo bindings now use
`CS01PollingParameterAttachment`. They register no audio parameter listener.
Their message-thread timers read `RangedAudioParameter::getValue()` at 60 Hz,
including silent MIDI edits and applied program values. Initial values are read
synchronously at construction. Incoming automation never accesses a control,
posts a message or checks MessageManager through these bindings. Host display
changes may lag one timer interval or longer when the message loop is busy;
DSP and user edits do not wait for that tick.

Slider ranges, custom mappings, snapping, text parsing/formatting and default
double-click values preserve the JUCE 9.0.3 binding semantics. Slider drags issue
balanced host gestures; button/combo edits issue complete gestures. APVTS
constructors retain its UndoManager. A binding invalidates its last polled value
after a GUI write: otherwise a host returning to that same old value before the
next tick could leave the GUI displaying the intervening edit. Timers stop on
message-thread destruction, and a drag gesture still active at closing is ended.
The parameter and control must outlive their binding.

This removes the production attachments' AsyncUpdater posting path, not JUCE's
host parameter listener mutexes or the graph callback locks. The Linux probe does
not interpose all system message APIs or shared-library allocation. This work
does not establish a lock-free callback or a hard deadline guarantee. See the
audit report for counts, timing percentiles, conditions and remaining work.
