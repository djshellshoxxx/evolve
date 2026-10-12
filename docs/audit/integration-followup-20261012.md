# Integration follow-up

The relaxed-maxwell branch's DreamRooms, Phantom and TunerDuel engines are now included in the playable application through EncounterOverlay. The message thread owns each encounter. A room publishes nine bounded atomic parameters to DreamRoomFx on the audio thread, with preallocated delays and smoothed transitions. The Phantom plays a call, accepts up to 32 clicked scale notes over an eight-second answer window, judges the phrase and plays its reply. The Tuner plays the tension chord and accepts pitch drag/drop resolutions. Escape and Leave end an encounter without a reward. Successful encounters award points and a bonus fact; room solve bits and Tuner relics persist and merge across instances.

During active play, each minute rolls for a Dream Room (1/97), then if absent a Phantom encounter (1/131), then if absent a Tuner encounter (1/173). These encounters have no score or interaction-sequence prerequisites. They pause story and lab schedules and do not overlap another lab game. The existing three hidden variants remain independent chance offers. Their names now appear on the offer itself.

Audit follow-ups implemented:

- F-04/F-05/F-06: Instrument-only export, 4/8/12/30 second selection, actual stereo output capture instead of duplicated mono. Async menu/file callbacks use SafePointer.
- F-08: Memory parameter control in the environment panel.
- F-12: XY right-click menu centres both axes without applying the right-click as a drag.
- F-18/F-19: project state stores score, elapsed time and a stable run identity; leaderboard submission replaces the same run and suppresses unchanged duplicates.
- W9: payload slots are reserved until audio consumption acknowledges them. Full pools return -1 and full command queues release rejected reservations. Host/message command producers are serialized without involving the audio consumer.
- W11: lab-game play time stops during intro, naming, help, options, journal, breeding, performance and encounter overlays.
- Ring-buffer copies now exclude concurrent audio writes. The audio producer uses a try-lock and never waits for a reader.

Older feature branches were already incorporated through main's squash commits: 8bc1162 (deep progression), 7a52c1e (evolution steering), 5422bb3 (haunted discoveries) and 7e15275 (steering intensity). Their older trees should not replace subsequent fixes. Their ancestry is reconciled in this integration, retaining main's newer implementation. Branches already behind main require no content changes.

Validation: the six non-JUCE rule/engine tests pass; changed source and integration tests pass C++20 syntax checks against JUCE 8.0.6. GUI smoke tests now cover all encounter kinds and idempotent closing. Processor smoke tests cover stereo capture, pool exhaustion/acknowledgement/reuse, and project score-state round trips. Full application builds and CI must pass before publishing binaries. Native Windows interaction/listening QA is still a separate activity from automated Windows compilation and tests.
