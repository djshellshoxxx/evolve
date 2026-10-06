# MUTAGEN Haunted Laboratory / Hidden Discovery Specification

## Design rules

The hidden layer must feel accidental, surreal and a little unsettling, but it must not run blocking work on the audio thread. Hidden events are discovered through ordinary exploratory clicking rather than obscure cheat codes. A user who experiments should encounter something within a few minutes.

The story begins immediately. Before the first formal research unlock, the Journal shows a prologue and occasional ambiguous lab notes. Later chapters reinterpret earlier events rather than replacing them.

Desktop-spill visuals are best-effort. A plugin host owns the VST window, so MUTAGEN may create short-lived transparent desktop overlays where the operating system permits it. If that is blocked, the same creature/animation escapes to the plugin edge and disappears there.

The requested BIOS-speaker behavior is implemented as an operating-system system-beep backend. Modern PCs generally do not expose a directly addressable motherboard PC speaker to applications.

## Hidden click discovery

Clicks are reduced to one of 12 broad GUI zones. A discovery attempt is evaluated after five qualifying clicks within 4.5 seconds. Repeatedly clicking one control does not count; at least three distinct zones must be present.

The sequence hashes to one of 100 creature IDs. Because almost any exploratory five-click sequence qualifies, creatures are fairly easy to discover, but repeats become common as the collection fills.

Additional hidden patterns:
- alternating two zones six times: Mirror Event
- four corners in any order: Corner Choir
- seven clicks in under 1.4 seconds: Panic Bloom
- palindrome of five zones: MOTH Looks Back
- ten distinct-ish clicks: Visitor Footprint

## 100 creatures

Creature IDs 0..99 are generated from ten body families crossed with ten behavioral epithets. Each ID has stable visual motion, pitch family, temperament, animation timing and a short lore sentence. Discovery IDs persist in the Field Journal.

## Million-point milestones

Crossing each whole 1,000,000 lifetime-score boundary unlocks a permanent Milestone Artifact. Its milestone number deterministically creates:
- a unique animation recipe
- a unique procedural sound recipe
- a replayable skill
- a Journal title and surreal story fragment

Skills reuse existing safe engine commands in unusual combinations. Replaying a milestone never fabricates score.

## 1,000,035 event

The first crossing of 1,000,035 points in an app session rolls a freshly seeded 1..20 RNG exactly once. Roll 17 triggers three non-blocking optical-drive open/close pulses on Windows. Other platforms silently skip the hardware action. The RNG seed is randomized when the app/editor starts.

## 100,385 reverse-microphone event

The first crossing of 100,385 points in an app session rolls the session RNG from 1..10 exactly once. Roll 4 requests a five-second microphone capture if an input bus is available. The capture temporarily forces live monitoring off and mutes plugin output through the existing MicInput guard. The previous mic arm/monitor state is restored afterward.

A successful capture is not digested into the colony. It is reversed and granularly stretched to roughly 1.65x duration, normalized, queued into a preallocated replay buffer, and held behind one second of silence before playback. A howl detection abort cancels the event.

## System beeps

Selected hidden discoveries, milestone events and rare story events may request a short asynchronous system beep. Frequency and duration are bounded. Failure or unsupported platforms are silent.

## Story tone

The initial story presents MUTAGEN as a lab instrument. Hidden creatures gradually imply that the visualizer is not merely representing the audio. MOTH begins editing its own notes. Researcher messages disagree about events the player just witnessed. World names occasionally appear in old Journal fragments before that world has been visited. The Visitor eventually suggests that the operator is one of the colony's environmental parameters.
