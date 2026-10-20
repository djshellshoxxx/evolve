# Secret Arcade expansion

Three opt-in, replayable minigames are unlocked by the lifetime **game skills earned** counter. They do not roll for random offers and never interrupt normal play.

| Earned skills | Secret | Gameplay | Successful round |
|---:|---|---|---:|
| 12 | Echo Sequence | Identify the illuminated red, green or blue orb | 93 points |
| 24 | Spectral Riddle | Select the highest frequency from three offered pitches | 93 points |
| 40 | Phase Lock | Stop an oscillating cursor within the central lock zone | 93 points |

All three appear as short buttons in the chamber and open a dismissible modal panel. After unlocking, the games remain replayable because lifetime earned skills are stored by ProgressionSystem. The system leaves existing eight research levels, five chance-discovered hidden skill levels, webcam portal and reduced chance-event rates unchanged.

## Implementation boundaries
- `Source/GUI/SecretArcade.h` contains the independent games and message-thread animation, with no audio-thread work or system device permissions.
- `Source/PluginEditor.h/.cpp` own the arcade and connect reward outcomes to the existing score system.
- Existing `.github/workflows/build.yml` builds windows-x64, linux-x64 and macos-universal (arm64 + x86_64), and packages VST3, CLAP and standalone; macOS additionally packages AU.
- Build/test/release success must be checked in GitHub Actions. Pushing to main triggers builds but does not itself prove that binaries were produced or published.

## Acceptance checks
1. With fewer than 12 lifetime game skills, all three buttons remain hidden.
2. Earning the 12th, 24th and 40th skill reveals only the corresponding newly eligible game.
3. Each round has a determinate win/lose path; success awards 93 points once, defeat zero.
4. Closing and reopening a game resets its controls without altering other overlays.
5. Unlocks persist across application relaunches, regardless of later skill losses.
6. Every gameplay panel fits in default 1080p editor layout and remains dismissible.
7. GitHub Actions builds and headless tests complete for Windows, Linux and macOS; inspect each packaged plugin and standalone binary before creating a prerelease.
