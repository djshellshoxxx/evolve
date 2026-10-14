#include "EncounterOverlay.h"

namespace mutagen
{
EncounterOverlay::EncounterOverlay()
{
    setWantsKeyboardFocus (true);
    addAndMakeVisible (leave);
    leave.onClick = [this] { finish (false); };
}
void EncounterOverlay::begin (Kind k, std::uint64_t seed, int act, int mode, int root, std::uint32_t relics)
{
    kind = k; rng = story::Rng (seed); rootNote = root;
    clock = 0; position = 0.5; clicked = false; grabbed = -1;
    answer.clear(); scheduled.clear(); tunerNotes.clear(); room.reset(); steps.clear();
    for (int s : story::modes()[(size_t) std::clamp (mode, 0, (int) story::modes().size() - 1)].steps)
        if (s != story::kNo) steps.push_back (s);
    if (k == Kind::room)
    {
        room = std::make_unique<story::DreamRoom> ((story::RoomId) rng.range (0, story::kNumRooms - 1), seed);
        room->enter();
    }
    else if (k == Kind::phantom)
    {
        call = story::phantom::makeCall (rng, act, steps);
        playMotif (call);
        answerAt = call.beats.back() * story::phantom::kBeatSec + 0.8;
    }
    else if (k == Kind::pulse)
    {
        pulse.start (seed); pulseFlash = -10; pulseErr = 0;
    }
    else if (k == Kind::freq)
    {
        freq.start (seed); freqAnswered = false; freqDoneAt = 1.0e9; freqLastMidi = -1; position = 0.5;
        playFreq();
    }
    else if (k == Kind::seek)
    {
        seek.start (seed); seekLastNote = -1; huntDoneAt = 1.0e9;
    }
    else if (k == Kind::flash)
    {
        flash.start (seed); flashDoneAt = 1.0e9; flashLastOk = false;
    }
    else if (k == Kind::hunt)
    {
        hunt.start (seed, act); huntFlash = -1; huntLastOk = false; huntDoneAt = 1.0e9;
    }
    else if (k == Kind::chorale)
    {
        chorale.start (seed);
        laneNotes = story::encore::laneSteps (steps);
        flashLane = -1; flashUntil = 0;
    }
    else
    {
        duel.start (seed, act, mode, root, relics);
        lastDuelState = story::duel::State::idle;
    }
    setVisible (true); toFront (true); grabKeyboardFocus(); repaint();
}
void EncounterOverlay::playHunt()
{
    const int low = rootNote + hunt.rootOffset();
    scheduled.push_back ({ clock, low });
    scheduled.push_back ({ clock + 0.7, low + hunt.answerSemitones() });
}
void EncounterOverlay::playFreq()
{
    const double ref = freq.current().refHz;
    if (ref > 0.0) scheduled.push_back ({ clock + 0.1, story::mini::FrequencyLab::midiFor (ref) });
}
void EncounterOverlay::auditionFreq()
{
    // MIDI notes are semitone steps, so the audition snaps; the readout shows the exact hertz.
    const int m = story::mini::FrequencyLab::midiFor (story::mini::FrequencyLab::hzAt (position));
    if (m != freqLastMidi && onNote) { freqLastMidi = m; onNote (m, 0.5); }
}
void EncounterOverlay::submitFreq()
{
    if (freq.finished()) return;
    freqResult = freq.submit (position); freqAnswered = true;
    if (freq.finished()) freqDoneAt = clock + 3.0;
    else { freqLastMidi = -1; scheduled.clear(); playFreq(); }
}
void EncounterOverlay::playFlash()
{
    const int root = rootNote + flash.rootOffset();
    for (int t : story::mini::qualityTones (flash.answerQuality())) scheduled.push_back ({ clock, root + t });
}
void EncounterOverlay::pressFlash (int i)
{
    if (flash.finished()) return;
    flashLastOk = flash.answer (i);
    if (flash.finished()) flashDoneAt = clock + 1.5;
}
void EncounterOverlay::playMotif (const story::phantom::Motif& m)
{
    for (size_t i = 0; i < m.degrees.size(); ++i)
        scheduled.push_back ({ clock + m.beats[i] * story::phantom::kBeatSec, rootNote + m.degrees[i] });
}
void EncounterOverlay::finish (bool won, std::uint32_t mask)
{
    if (! isVisible()) return;
    if (kind == Kind::freq && mask == 0 && ! won)
        mask = (std::uint32_t) freq.correctCount() | ((std::uint32_t) freq.perfects() << 8);
    if (kind == Kind::seek && mask == 0 && ! won) mask = (std::uint32_t) seek.foundCount();
    if (kind == Kind::flash && mask == 0 && ! won) mask = (std::uint32_t) flash.correctCount();
    if (kind == Kind::pulse && mask == 0 && ! won)
        mask = (std::uint32_t) pulse.hits() | ((std::uint32_t) pulse.perfects() << 8);
    if (kind == Kind::hunt && mask == 0 && ! won) mask = (std::uint32_t) hunt.correctCount();
    if (kind == Kind::chorale && mask == 0 && ! won)   // left early: keep the rounds already cleared
        mask = (std::uint32_t) chorale.roundsCleared() | ((std::uint32_t) chorale.lives() << 8);
    setVisible (false); scheduled.clear();
    if (onRoomTargets) onRoomTargets (story::RoomTargets::baseline());
    if (onFinished) onFinished (kind, won, mask);
}
void EncounterOverlay::tick (double dt)
{
    if (! isVisible()) return;
    clock += dt;
    for (auto i = scheduled.begin(); i != scheduled.end();)
        if (i->at <= clock) { if (onNote) onNote (i->note, 0.35); i = scheduled.erase (i); }
        else ++i;
    if (kind == Kind::room)
    {
        room->update (dt, { position, clicked }); clicked = false;
        if (onRoomTargets) onRoomTargets (room->targets());
        if (room->finished()) { finish (room->solved(), room->solvedMaskBit()); return; }
    }
    else if (kind == Kind::phantom)
    {
        if (clock >= answerAt + 8.0 && answerAt < 10000)
        {
            const auto result = story::phantom::judge (call, answer, steps);
            playMotif (story::phantom::makeReply (answer, result, rng, steps));
            const double duration = scheduled.empty() ? 0.5 : scheduled.back().at - clock + 1.0;
            answerAt = 10000 + clock + duration;
            clicked = story::phantom::scoreFor (result) > 0;
        }
        if (answerAt >= 10000 && clock >= answerAt - 10000)
        { finish (clicked); return; }
    }
    else if (kind == Kind::pulse)
    {
        pulse.update (dt);
        for (int i : pulse.takeSounded()) if (onNote) onNote (rootNote - 12 + (i == 0 ? 7 : 0), 0.12);
        if (pulse.finished())
        { finish (pulse.won(), (std::uint32_t) pulse.hits() | ((std::uint32_t) pulse.perfects() << 8)); return; }
    }
    else if (kind == Kind::freq)
    {
        if (freq.finished() && clock > freqDoneAt)
        { finish (freq.won(), (std::uint32_t) freq.correctCount() | ((std::uint32_t) freq.perfects() << 8)); return; }
    }
    else if (kind == Kind::seek)
    {
        const int note = rootNote + (int) std::lround (position * 24.0);
        if (seek.update (dt, position) && onNote) onNote (note, 0.12);
        if (seek.finished() && clock > huntDoneAt) { finish (seek.won(), (std::uint32_t) seek.foundCount()); return; }
    }
    else if (kind == Kind::flash)
    {
        flash.update (dt);
        if (flash.consumePlay()) playFlash();
        if (flash.finished() && clock > flashDoneAt) { finish (flash.won(), (std::uint32_t) flash.correctCount()); return; }
    }
    else if (kind == Kind::hunt)
    {
        if (hunt.consumePlay()) playHunt();
        if (hunt.finished() && clock > huntDoneAt)
        { finish (hunt.won(), (std::uint32_t) hunt.correctCount()); return; }
    }
    else if (kind == Kind::chorale)
    {
        const double beat = story::encore::beatSeconds (chorale.roundsCleared());
        chorale.update (dt);
        for (int lane : chorale.takeSounded())
        {
            flashLane = lane; flashUntil = clock + beat * 0.7;
            if (onNote) onNote (rootNote + laneNotes[(size_t) lane], beat * 0.9);
        }
        if (chorale.finished())
        {
            // Rounds cleared ride in the mask, so a lost chorale still pays for what was earned.
            finish (chorale.state() == story::encore::State::won, (std::uint32_t) chorale.roundsCleared() | ((std::uint32_t) chorale.lives() << 8));
            return;
        }
    }
    else
    {
        duel.update (dt);
        if (duel.state() != lastDuelState)
        {
            lastDuelState = duel.state();
            for (int n : tunerNotes) if (onNoteStop) onNoteStop (n);
            tunerNotes = duel.notesToPlay();
            const double duration = duel.state() == story::duel::State::resolve ? duel.resolveTimeLeft() + 0.1 : 2.2;
            for (int n : tunerNotes) if (onNote) onNote (n, duration);
        }
        if (duel.finished()) { finish (duel.state() == story::duel::State::won, duel.relicMask()); return; }
    }
    repaint();
}
juce::Rectangle<float> EncounterOverlay::laneRect (int lane) const
{
    const float w = field.getWidth() / (float) story::encore::kLanes;
    return { field.getX() + (float) lane * w + 4.0f, field.getY() + field.getHeight() * 0.25f, w - 8.0f, field.getHeight() * 0.5f };
}
void EncounterOverlay::resized()
{
    leave.setBounds (getWidth() - 100, 10, 90, 28);
    field = getLocalBounds().toFloat().reduced (28).withTrimmedTop (95).withTrimmedBottom (40);
}
void EncounterOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111827)); g.setColour (juce::Colour (0xff7eead0)); g.setFont (21.0f);
    const juce::String title = kind == Kind::room ? story::roomName (room->id()) : kind == Kind::phantom ? "PHANTOM: CALL AND ANSWER" : kind == Kind::chorale ? "ENCORE: THE ECHO CHORALE" : kind == Kind::pulse ? "PULSE: KEEP THE HEARTBEAT" : kind == Kind::hunt ? "INTERVAL HUNT" : kind == Kind::seek ? "SEEK: HOT AND COLD" : kind == Kind::flash ? "CHORD FLASH" : kind == Kind::freq ? "FREQUENCY LAB" : "TUNER DUEL";
    g.drawText (title, 20, 16, getWidth() - 130, 30, juce::Justification::centredLeft);
    g.setFont (14.0f); g.setColour (juce::Colours::white);
    juce::String instruction;
    if (kind == Kind::room)
    {
        instruction = story::roomLaw (room->id());
        instruction += "  " + juce::String (room->hintActive() ? "Follow the glowing marker." : "Move across the field; click to explore.");
        if (room->id() == story::RoomId::dopplerCarousel) instruction = "Click when each orbiting tone reaches its highest pitch.";
        if (room->id() == story::RoomId::cathedral) instruction = "Click eight notes, waiting for each tail to finish.";
        if (room->id() == story::RoomId::standingWave) instruction = "Find and click two loud and two quiet positions.";
        if (room->id() == story::RoomId::phaseGarden) instruction = "Track the moving tone and hold alignment for three seconds.";
        if (room->id() == story::RoomId::combHall) instruction = "Move to find the notch and hold the quiet position for two seconds.";
        g.drawRect (field);
        g.fillEllipse (field.getX() + (float) position * field.getWidth() - 7, field.getCentreY() - 7, 14, 14);
        if (room->hintActive())
        {
            g.setColour (juce::Colours::gold);
            auto marker = [&] (double p) { g.drawVerticalLine ((int) (field.getX() + p * field.getWidth()), field.getY(), field.getBottom()); };
            if (room->id() == story::RoomId::combHall) marker (room->combSolvePos());
            else if (room->id() == story::RoomId::phaseGarden) marker (room->gardenDriftNow());
            else if (room->id() == story::RoomId::standingWave)
                for (double p : room->standingSpots()) marker (p);
        }
        if (room->id() == story::RoomId::dopplerCarousel)
            for (int i = 0; i < 3; ++i)
            {
                g.setColour (room->cellCaught (i) ? juce::Colours::green : juce::Colours::orange);
                g.fillEllipse (field.getX() + (i + 1) * field.getWidth() / 4 - 10,
                               field.getCentreY() - (room->cellRatio (i) - 1) * 600, 20, 20);
            }
    }
    else if (kind == Kind::phantom)
    {
        instruction = clock < answerAt && answerAt < 10000 ? "Listen to the call." : answerAt < 10000 ? "Answer by clicking notes across the field. Change the melody or rhythm." : "The Phantom answers your phrase.";
        for (size_t i = 0; i < steps.size() * 2; ++i)
        {
            const float w = field.getWidth() / (float) (steps.size() * 2);
            g.drawRect (field.getX() + (float) i * w, field.getY(), w, field.getHeight());
        }
    }
    else if (kind == Kind::pulse)
    {
        const bool audible = pulse.clockNow() < pulse.beatTime (story::mini::Pulse::kAudible);
        instruction = audible ? "Listen to the heartbeat, then tap (click or Space) on every beat. It goes silent - keep the time."
                              : "Silent now. Keep the beat.";
        instruction += "  Hits " + juce::String (pulse.hits()) + "/" + juce::String (story::mini::Pulse::kBeats)
                     + " (need " + juce::String (story::mini::Pulse::kToWin) + ")"
                     + (clock - pulseFlash < 0.6 ? juce::String (pulseErr < 0 ? "   EARLY" : "   LATE") : juce::String());
        const auto c = field.getCentre();
        const float r = 20.0f + (1.0f - (float) pulse.phase()) * 120.0f;
        g.setColour (juce::Colour (0xff7eead0).withAlpha (0.8f)); g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 3.0f);
        g.setColour (juce::Colours::white); g.drawEllipse (c.x - 20, c.y - 20, 40, 40, 2.0f);
    }
    else if (kind == Kind::freq)
    {
        namespace F = story::mini;
        const auto& q = freq.current();
        instruction = juce::String (q.prompt) + "  Drag along the strip, release to answer.   Correct "
                    + juce::String (freq.correctCount()) + "/" + juce::String (F::FrequencyLab::kToWin) + " needed";
        const auto strip = field.withSizeKeepingCentre (field.getWidth(), 50.0f);
        g.setColour (juce::Colour (0xff1f2a44)); g.fillRoundedRectangle (strip, 8.0f);
        g.setFont (11.0f);
        for (int o = 0; o <= 5; ++o)
        {
            const float x = strip.getX() + (float) o / 5.0f * strip.getWidth();
            g.setColour (juce::Colours::grey); g.drawVerticalLine ((int) x, strip.getY(), strip.getBottom());
            g.drawText (juce::String ((int) (55 << o)) + " Hz", (int) x - 30, (int) strip.getBottom() + 4, 60, 14, juce::Justification::centred);
        }
        const float cx = strip.getX() + (float) position * strip.getWidth();
        g.setColour (juce::Colour (0xff7eead0)); g.fillRect (cx - 2.0f, strip.getY() - 8.0f, 4.0f, strip.getHeight() + 16.0f);
        g.setFont (22.0f);
        g.drawText (juce::String (F::FrequencyLab::hzAt (position), 1) + " Hz", strip.translated (0, -60.0f).toNearestInt(), juce::Justification::centred);
        g.setFont (13.0f); g.setColour (juce::Colours::white);
        if (freqAnswered)
        {
            const juce::String verdict = freqResult.perfect ? "PERFECT" : freqResult.ok ? "CLOSE ENOUGH" : "MISSED";
            g.drawFittedText (verdict + " (" + juce::String (freqResult.cents, 0) + " cents).  " + juce::String (freq.lastLessonText()),
                              (int) field.getX(), (int) strip.getBottom() + 30, (int) field.getWidth(), 50, juce::Justification::centredLeft, 3);
        }
    }
    else if (kind == Kind::seek)
    {
        instruction = "Move across the field. The pings speed up as you near the hidden pitch. Click to lock in.  Round "
                    + juce::String (seek.roundNumber()) + "/" + juce::String (story::mini::Seek::kRounds)
                    + "  Found " + juce::String (seek.foundCount()) + "  Time " + juce::String (seek.timeLeft(), 1);
        g.drawRect (field);
        g.setColour (juce::Colour (0xff7eead0));
        g.fillEllipse (field.getX() + (float) position * field.getWidth() - 8, field.getCentreY() - 8, 16, 16);
    }
    else if (kind == Kind::flash)
    {
        instruction = "Question " + juce::String (flash.questionNumber()) + "/" + juce::String (story::mini::Flash::kQuestions)
                    + ". What quality is this chord? Keys 1-4.  Correct " + juce::String (flash.correctCount())
                    + "  Time " + juce::String (flash.timeLeft(), 1);
        if (! flash.finished())
            for (int i = 0; i < (int) story::mini::Quality::count; ++i)
            {
                g.setColour (juce::Colour::fromHSV ((float) i * 0.22f + 0.05f, 0.5f, 0.55f, 1.0f));
                g.fillRoundedRectangle (laneRect (i), 10.0f);
                g.setColour (juce::Colours::white);
                g.drawFittedText (juce::String (i + 1) + "  " + story::mini::qualityName ((story::mini::Quality) i),
                                  laneRect (i).toNearestInt(), juce::Justification::centred, 2);
            }
    }
    else if (kind == Kind::hunt)
    {
        instruction = "Question " + juce::String (juce::jmin (hunt.questionNumber(), story::mini::Hunt::kQuestions)) + "/"
                    + juce::String (story::mini::Hunt::kQuestions) + ". Which interval did the colony play (low note, then high)? Press R to replay."
                    + "  Correct " + juce::String (hunt.correctCount());
        if (huntFlash >= 0) instruction += huntLastOk ? "   RIGHT" : "   WRONG";
        if (! hunt.finished())
            for (int i = 0; i < story::mini::Hunt::kOptions; ++i)
            {
                g.setColour (juce::Colour::fromHSV ((float) i * 0.22f + 0.05f, 0.5f, 0.55f, 1.0f));
                g.fillRoundedRectangle (laneRect (i), 10.0f);
                g.setColour (juce::Colours::white);
                g.drawFittedText (juce::String (i + 1) + "  " + story::mini::intervalName (hunt.options()[(size_t) i]),
                                  laneRect (i).toNearestInt(), juce::Justification::centred, 2);
            }
    }
    else if (kind == Kind::chorale)
    {
        using story::encore::State;
        const auto st = chorale.state();
        instruction = st == State::demo ? "Listen to the colony sing."
                    : st == State::listen ? "Echo it back. Click the pads, or press 1 2 3 4."
                    : "Round cleared. The phrase grows.";
        instruction += "  Round " + juce::String (chorale.roundsCleared() + 1) + "/" + juce::String (story::encore::kRounds)
                     + "   Lives " + juce::String (chorale.lives()) + "   Echoed " + juce::String (chorale.inputIndex())
                     + "/" + juce::String (chorale.phraseLength());
        for (int i = 0; i < story::encore::kLanes; ++i)
        {
            const bool on = i == flashLane && clock < flashUntil;
            g.setColour (juce::Colour::fromHSV ((float) i * 0.22f + 0.05f, 0.65f, on ? 1.0f : 0.45f, 1.0f));
            g.fillRoundedRectangle (laneRect (i), 10.0f);
            g.setColour (juce::Colours::white); g.drawText (juce::String (i + 1), laneRect (i), juce::Justification::centred);
        }
    }
    else
    {
        instruction = juce::String (duel.line()) + "  Drag an orange cell onto a target ring below.";
        const auto cells = duel.cells();
        for (const auto& t : duel.targets())
        { g.setColour (t.hint ? juce::Colours::gold : juce::Colours::lightgreen); g.drawEllipse (field.getX() + t.x * field.getWidth() - 10, field.getY() + t.y * field.getHeight() - 10, 20, 20, 2); }
        for (const auto& c : cells)
        { g.setColour (c.resolved ? juce::Colours::green : c.unstable ? juce::Colours::orange : juce::Colours::grey); g.fillEllipse (field.getX() + c.x * field.getWidth() - 9, field.getY() + c.y * field.getHeight() - 9, 18, 18); }
    }
    g.setColour (juce::Colours::white);
    g.drawFittedText (instruction, 22, 55, getWidth() - 44, 45, juce::Justification::centredLeft, 2);
}
void EncounterOverlay::mouseDown (const juce::MouseEvent& e)
{
    if (kind == Kind::pulse) { pressPulse(); return; }
    if (kind == Kind::freq)
    {
        position = std::clamp ((double) ((e.position.x - field.getX()) / std::max (1.0f, field.getWidth())), 0.0, 1.0);
        auditionFreq();
        return;
    }
    if (kind == Kind::seek)
    {
        position = std::clamp ((double) ((e.position.x - field.getX()) / std::max (1.0f, field.getWidth())), 0.0, 1.0);
        if (field.contains (e.position) && ! seek.finished())
        {
            if (seek.lock (position) && onNote) onNote (rootNote + (int) std::lround (position * 24.0), 0.6);
            if (seek.finished()) huntDoneAt = clock + 1.2;
        }
        return;
    }
    if (kind == Kind::flash)
    {
        for (int i = 0; i < (int) story::mini::Quality::count; ++i)
            if (laneRect (i).contains (e.position)) { pressFlash (i); return; }
        return;
    }
    if (kind == Kind::hunt)
    {
        for (int i = 0; i < story::mini::Hunt::kOptions; ++i)
            if (laneRect (i).contains (e.position)) { pressHunt (i); return; }
        return;
    }
    if (kind == Kind::chorale)
    {
        for (int i = 0; i < story::encore::kLanes; ++i)
            if (laneRect (i).contains (e.position)) { pressLane (i); return; }
        return;
    }
    if (! field.contains (e.position)) return;
    position = std::clamp ((double) ((e.position.x - field.getX()) / field.getWidth()), 0.0, 1.0);
    if (kind == Kind::room) { clicked = true; if (onNote) onNote (rootNote, 0.3); }
    else if (kind == Kind::phantom && clock >= answerAt && answerAt < 10000 && answer.size() < 32)
    {
        const int n = story::phantom::degreeForX ((float) position, steps);
        answer.push_back ({ clock - answerAt, n }); if (onNote) onNote (rootNote + n, 0.3);
    }
    else if (kind == Kind::tuner)
    {
        const auto cells = duel.cells();
        for (size_t i = 0; i < cells.size(); ++i)
            if (std::abs (cells[i].x - position) < 0.06 && duel.grab ((int) i)) { grabbed = (int) i; break; }
    }
}
void EncounterOverlay::mouseDrag (const juce::MouseEvent& e)
{
    position = std::clamp ((double) ((e.position.x - field.getX()) / std::max (1.0f, field.getWidth())), 0.0, 1.0);
    if (kind == Kind::freq) auditionFreq();
}
void EncounterOverlay::mouseUp (const juce::MouseEvent&)
{
    if (kind == Kind::freq) { submitFreq(); return; }
    if (grabbed < 0) return;
    const auto cells = duel.cells();
    if (duel.drop (grabbed, (float) position) == story::duel::DropResult::correct)
    {
        if ((size_t) grabbed < cells.size() && onNoteStop) onNoteStop (cells[(size_t) grabbed].pitch);
        if (onNote) onNote (duel.xToPitch ((float) position), 0.8);
    }
    grabbed = -1;
}
void EncounterOverlay::pressPulse()
{
    const double err = pulse.tap();
    if (err > 1.0e8) return;
    pulseFlash = clock; pulseErr = err;
    if (onNote) onNote (rootNote - 12, 0.1);
}
void EncounterOverlay::pressHunt (int i)
{
    if (hunt.finished()) return;
    huntLastOk = hunt.answer (i); huntFlash = i;
    if (hunt.finished()) huntDoneAt = clock + 1.5;
}
void EncounterOverlay::pressLane (int lane)
{
    if (chorale.press (lane) == story::encore::Press::ignored) return;
    flashLane = lane; flashUntil = clock + 0.25;
    if (onNote) onNote (rootNote + laneNotes[(size_t) lane], 0.3);
}
bool EncounterOverlay::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { finish (false); return true; }
    if (kind == Kind::pulse && k == juce::KeyPress::spaceKey) { pressPulse(); return true; }
    if (kind == Kind::freq)
    {
        if (k == juce::KeyPress::spaceKey || k == juce::KeyPress::returnKey) { submitFreq(); return true; }
        if (k == juce::KeyPress::leftKey || k == juce::KeyPress::rightKey)
        {
            position = std::clamp (position + (k == juce::KeyPress::rightKey ? 1.0 : -1.0) / 60.0, 0.0, 1.0);
            auditionFreq(); return true;
        }
    }
    if (kind == Kind::flash)
    {
        const int pick = k.getTextCharacter() - '1';
        if (pick >= 0 && pick < (int) story::mini::Quality::count) { pressFlash (pick); return true; }
    }
    if (kind == Kind::hunt)
    {
        const int pick = k.getTextCharacter() - '1';
        if (pick >= 0 && pick < story::mini::Hunt::kOptions) { pressHunt (pick); return true; }
        if (k.getTextCharacter() == 'r' || k.getTextCharacter() == 'R') { hunt.replay(); return true; }
    }
    if (kind == Kind::chorale)
    {
        const int lane = k.getTextCharacter() - '1';
        if (lane >= 0 && lane < story::encore::kLanes) { pressLane (lane); return true; }
    }
    return false;
}
}
