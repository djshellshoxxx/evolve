# Chromatic Portal — hidden webcam synth minigame

## Unlock
- Unlock after **1,800 seconds (30 minutes) of unpaused active play**.
- Once discovered, store `webcamSynthUnlocked` in the existing progression JSON.
- The CHROMATIC PORTAL button remains available on later launches.
- No automatic webcam activation at unlock or when opening the minigame.

## Interaction
- Selecting **ENABLE CAMERA** opens the first available JUCE-compatible webcam.
- The latest frame is sampled at a low-resolution 12×8 grid on the GUI thread, and its brightness centroid controls a pitch in MIDI notes 48–72 and its vertical location controls velocity.
- Notes are sent through the existing on-screen MIDI keyboard state; output continues through the normal synthesizer engine.
- Animated, multicolour portal rings indicate X/Y control position.
- Mouse movement provides a no-camera fallback.
- **DISABLE CAMERA** closes the webcam without leaving the game.
- **CLOSE** stops the GUI timer, releases all held notes, closes the webcam and returns to the main game.

## Integration and limitations
- Implementation: `Source/GUI/CameraSynthGame.h`, `Source/PluginEditor.h/.cpp`, `Source/Engine/ProgressionSystem.h/.cpp`, and `CMakeLists.txt`.
- Camera support depends on the host OS, its permission policy, and JUCE camera support. Webcam access is always opt-in; no capture is saved or uploaded.
- Unlock counts active editor game time, not host transport time while the editor is closed.
- Initial prototype uses brightness centroid, not object recognition or gesture tracking.
- Plugin formats and standalone need platform builds and physical camera QA before release.

## Acceptance tests
1. Below 1800 active seconds: minigame remains hidden.
2. At 1800 seconds: button appears and progress is saved; relaunch preserves unlock.
3. Opening the game does not access the webcam until explicit consent.
4. Mouse fallback plays notes with no camera.
5. With camera enabled, moving a bright object changes pitch and velocity; video is displayed.
6. Closing the game and disabling the webcam release the device and active notes.
7. Confirm camera-denied/unavailable fallback and no crashes on repeated opens.
8. Run Windows and macOS VST3, AU, CLAP and standalone builds and verify audio and camera permissions.
