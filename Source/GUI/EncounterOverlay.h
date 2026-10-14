#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Engine/DreamRooms.h"
#include "../Engine/Encore.h"
#include "../Engine/Minigames.h"
#include "../Engine/Phantom.h"
#include "../Engine/TunerDuel.h"

namespace mutagen
{
// UI owns encounter state. Only bounded parameters and note commands cross to audio.
class EncounterOverlay : public juce::Component
{
public:
    enum class Kind { room, phantom, tuner, chorale, pulse, hunt, seek, flash };
    EncounterOverlay();
    void begin (Kind, std::uint64_t seed, int act, int mode, int root, std::uint32_t relics);
    void tick (double dt);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    std::function<void(int, double)> onNote;
    std::function<void(int)> onNoteStop;
    std::function<void(const story::RoomTargets&)> onRoomTargets;
    std::function<void(Kind, bool, std::uint32_t)> onFinished;
private:
    void finish (bool, std::uint32_t mask = 0);
    juce::Rectangle<float> laneRect (int) const;
    void pressLane (int);
    void pressPulse();
    void pressHunt (int);
    void playMotif (const story::phantom::Motif&);
    std::vector<int> steps;
    story::Rng rng { 1 };
    std::unique_ptr<story::DreamRoom> room;
    story::phantom::Motif call;
    story::phantom::Phrase answer;
    story::duel::Duel duel;
    story::encore::Chorale chorale;
    story::mini::Pulse pulse;
    story::mini::Hunt hunt;
    story::mini::Seek seek;
    story::mini::Flash flash;
    int seekLastNote = -1;
    double flashDoneAt = 1.0e9;
    bool flashLastOk = false;
    void playFlash();
    void pressFlash (int);
    double pulseFlash = -10, pulseErr = 0;
    int huntFlash = -1;
    double huntDoneAt = 1.0e9;
    bool huntLastOk = false;
    void playHunt();
    std::array<int, story::encore::kLanes> laneNotes {};
    int flashLane = -1;
    double flashUntil = 0;
    story::duel::State lastDuelState = story::duel::State::idle;
    struct Scheduled { double at; int note; };
    std::vector<Scheduled> scheduled;
    std::vector<int> tunerNotes;
    juce::TextButton leave { "LEAVE" };
    Kind kind = Kind::room;
    int rootNote = 60, grabbed = -1;
    double clock = 0, answerAt = 0, position = 0.5;
    bool clicked = false;
    juce::Rectangle<float> field;
};
}
