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

## Remaining concerns outside this refactor

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
