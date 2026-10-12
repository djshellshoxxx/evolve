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
    answer.clear(); scheduled.clear(); room.reset(); steps.clear();
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
    else
    {
        duel.start (seed, act, mode, root, relics);
        lastDuelState = story::duel::State::idle;
    }
    setVisible (true); toFront (true); grabKeyboardFocus(); repaint();
}
void EncounterOverlay::playMotif (const story::phantom::Motif& m)
{
    for (size_t i = 0; i < m.degrees.size(); ++i)
        scheduled.push_back ({ clock + m.beats[i] * story::phantom::kBeatSec, rootNote + m.degrees[i] });
}
void EncounterOverlay::finish (bool won, std::uint32_t mask)
{
    if (! isVisible()) return;
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
    else
    {
        duel.update (dt);
        if (duel.state() != lastDuelState)
        {
            lastDuelState = duel.state();
            for (int n : duel.notesToPlay()) if (onNote) onNote (n, 1.5);
        }
        if (duel.finished()) { finish (duel.state() == story::duel::State::won, duel.relicMask()); return; }
    }
    repaint();
}
void EncounterOverlay::resized()
{
    leave.setBounds (getWidth() - 100, 10, 90, 28);
    field = getLocalBounds().toFloat().reduced (28).withTrimmedTop (95).withTrimmedBottom (40);
}
void EncounterOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111827)); g.setColour (juce::Colour (0xff7eead0)); g.setFont (21.0f);
    const juce::String title = kind == Kind::room ? story::roomName (room->id()) : kind == Kind::phantom ? "PHANTOM: CALL AND ANSWER" : "TUNER DUEL";
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
            const double p = room->id() == story::RoomId::combHall ? room->combSolvePos() : room->gardenDriftNow();
            g.setColour (juce::Colours::gold); g.drawVerticalLine ((int) (field.getX() + p * field.getWidth()), field.getY(), field.getBottom());
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
{ position = std::clamp ((double) ((e.position.x - field.getX()) / std::max (1.0f, field.getWidth())), 0.0, 1.0); }
void EncounterOverlay::mouseUp (const juce::MouseEvent&)
{ if (grabbed >= 0) { duel.drop (grabbed, (float) position); grabbed = -1; } }
bool EncounterOverlay::keyPressed (const juce::KeyPress& k)
{ if (k == juce::KeyPress::escapeKey) { finish (false); return true; } return false; }
}
