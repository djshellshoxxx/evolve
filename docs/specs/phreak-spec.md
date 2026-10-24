# Phreak cheat menu

Type the word `phreak` (case-insensitive, keys within 2.5 s of each other) anywhere in the editor except a text field. `Source/Engine/Phreak.h` is the pure matcher (tests: `Tests/PhreakTest.cpp`); `PhreakOverlay` is the feature tree; `Source/PhreakFlow.cpp` builds it.

Double-click (or Enter) runs an entry; the menu closes first so the entry can open its own overlay. Escape closes it. Every entry calls the game's own code path:

- **Minigames and encounters:** all nine encounter kinds.
- **Lab games:** all ten, scratch tickets forced to at least one.
- **Skills:** five game skills, 16 milestone-artifact skills.
- **Instrument unlocks:** the 12 reward instruments.
- **Story:** open any of the nine acts, play any of the four endings, twist, ear check, conversation, bonus fact, and UNLOCK (marks every ending, chance event, twist, secret and lexicon entry seen).
- **Chance events**, **secret rooms** (39), **haunted lab** (100 creatures, 14 name fates), **orb events**.

Entries change persisted progress exactly as legitimate play would. The word is typed into the plugin window, so a DAW that grabs letter keys for its own shortcuts may swallow it; the standalone always works.
