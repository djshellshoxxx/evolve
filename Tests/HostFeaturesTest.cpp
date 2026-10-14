#include "../Source/PluginEditor.h"
#include "../Source/Engine/AudioImport.h"
#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <memory>
#include <thread>
#include <limits>
using namespace mutagen;
static int failures = 0;
#define CHECK(x) do { if (!(x)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, #x); } } while(0)
static void set (MutagenProcessor& p, const char* id, float value)
{ auto* parameter = p.apvts.getParameter (id); parameter->setValueNotifyingHost (parameter->convertTo0to1 (value)); }
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto p = std::make_unique<MutagenProcessor>();
    CHECK (p->getNumPrograms() > 1);
    CHECK (p->getProgramName (0) == "Factory Default");
    CHECK (p->getProgramName (-1).isEmpty()); CHECK (p->getProgramName (p->getNumPrograms()).isEmpty());
    for (int i = 0; i < p->getNumPrograms(); ++i) CHECK (p->getProgramName (i).isNotEmpty());
    p->setCurrentProgram (1); CHECK (p->getCurrentProgram() == 1);
    p->setCurrentProgram (-1); CHECK (p->getCurrentProgram() == 1);
    p->setCurrentProgram (p->getNumPrograms()); CHECK (p->getCurrentProgram() == 1);
    std::thread host ([&] { p->setCurrentProgram (2); }); host.join();
    CHECK (p->getCurrentProgram() == 2); // off-message calls stage the selection without changing parameters
    p->setCurrentProgram (0);
    p->setPlayConfigDetails (2, 2, 48000, 128); p->prepareToPlay (48000, 128);
    CHECK (p->getLatencySamples() == 0);
    juce::AudioBuffer<float> input (2, 128); juce::MidiBuffer midi;
    set (*p, params::pluginRole, 1);
    input.clear(); input.setSample (0, 0, 0.25f); input.setSample (1, 0, -0.1f);
    p->processBlockBypassed (input, midi);
    CHECK (input.getSample (0, 0) == 0.25f); CHECK (input.getSample (1, 0) == -0.1f);
    // The fully dry effect path has no fixed graph delay (creative echo/reverb tails are not host latency).
    set (*p, params::dryWet, 0); set (*p, params::morphMix, 0);
    p->processBlock (input, midi); CHECK (std::abs (input.getSample (0, 0) - 0.25f) < 1.0e-5f);
    set (*p, params::pluginRole, 0); p->processBlockBypassed (input, midi);
    CHECK (input.getMagnitude (0, input.getNumSamples()) == 0);
    p->setPlayConfigDetails (1, 2, 48000, 128);
    set (*p, params::pluginRole, 1); input.clear();
    input.setSample (0, 0, 0.25f); input.setSample (1, 0, 0.7f);
    p->processBlockBypassed (input, midi);
    CHECK (input.getSample (0, 0) == 0.25f); CHECK (input.getSample (1, 0) == 0.25f);
    CHECK (input.getSample (1, 1) == 0.0f);
    p->setPlayConfigDetails (2, 2, 48000, 128);
    set (*p, params::memory, 0.2f); p->parameterSlots.switchTo (true);
    set (*p, params::memory, 0.8f); p->parameterSlots.switchTo (false);
    CHECK (std::abs (p->apvts.getRawParameterValue (params::memory)->load() - 0.2f) < 0.001f);
    p->parameterSlots.switchTo (true);
    CHECK (std::abs (p->apvts.getRawParameterValue (params::memory)->load() - 0.8f) < 0.001f);
    p->setCurrentProgram (2);
    juce::MemoryBlock state; p->getStateInformation (state);
    auto restored = std::make_unique<MutagenProcessor>(); restored->setStateInformation (state.getData(), (int) state.getSize());
    CHECK (restored->getCurrentProgram() == 2);
    CHECK (restored->parameterSlots.isB()); restored->parameterSlots.switchTo (false);
    CHECK (std::abs (restored->apvts.getRawParameterValue (params::memory)->load() - 0.2f) < 0.001f);
    auto malformed = p->parameterSlots.toValueTree();
    auto slot = malformed.getChild (0); slot.setProperty (params::memory, 9.0, nullptr);
    slot.setProperty (params::fertility, std::numeric_limits<double>::infinity(), nullptr);
    restored->parameterSlots.fromValueTree (malformed); restored->parameterSlots.switchTo (false);
    CHECK (restored->apvts.getRawParameterValue (params::memory)->load() == 1.0f);
    restored->parameterSlots.fromValueTree ({}); CHECK (! restored->parameterSlots.isB());
    restored->midiLearn.beginLearn (params::memory); CHECK (restored->midiLearn.isLearning());
    restored->midiLearn.cancelLearn(); CHECK (! restored->midiLearn.isLearning());
    CHECK (audioImport::accepts ("recording.wav")); CHECK (! audioImport::accepts ("recording.unsupported"));
    std::unique_ptr<juce::AudioProcessorEditor> ownedEditor (restored->createEditor());
    auto* editor = dynamic_cast<MutagenEditor*> (ownedEditor.get()); CHECK (editor != nullptr);
    if (editor != nullptr)
    {
        editor->setSize (1060, 740);
        OptionsView* options = nullptr; HelpView* help = nullptr; GameBar* actions = nullptr;
        TopBar* topBar = nullptr;
        for (auto* child : editor->getChildren())
        {
            if (dynamic_cast<IntroOverlay*> (child) || dynamic_cast<NameOverlay*> (child)) child->setVisible (false);
            if (auto* toolbar = dynamic_cast<TopBar*> (child))
            {
                topBar = toolbar;
                auto* selector = dynamic_cast<juce::ComboBox*> (toolbar->findChildWithID ("factoryPresetSelector"));
                CHECK (selector != nullptr && selector->getSelectedId() == 3);
            }
            if (auto* view = dynamic_cast<OptionsView*> (child)) options = view;
            if (auto* view = dynamic_cast<HelpView*> (child)) help = view;
            if (auto* view = dynamic_cast<GameBar*> (child)) actions = view;
            if (child->isVisible()) CHECK (editor->getLocalBounds().contains (child->getBounds()));
        }
        CHECK (options != nullptr && help != nullptr && actions != nullptr);
        restored->midiLearn.beginLearn (params::memory);
        CHECK (editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey))); CHECK (! restored->midiLearn.isLearning());
        if (help != nullptr)
        { help->setVisible (true); CHECK (editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey))); CHECK (! help->isVisible()); }
        if (topBar != nullptr)
        {
            auto* fx = dynamic_cast<juce::Button*> (topBar->findChildWithID ("fxViewToggle"));
            auto* perform = dynamic_cast<juce::Button*> (topBar->findChildWithID ("performanceViewToggle"));
            CHECK (fx != nullptr && perform != nullptr);
            for (int i = 0; i < 2 && fx != nullptr; ++i)
            {
                CHECK (editor->keyPressed (juce::KeyPress ('F', juce::ModifierKeys (juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier), 0)));
                CHECK (fx->getToggleState());
                CHECK (editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
                CHECK (! fx->getToggleState());
            }
            if (perform != nullptr)
            {
                perform->setToggleState (true, juce::dontSendNotification); perform->onClick();
                CHECK (editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
                CHECK (! perform->getToggleState());
            }
        }
        if (options != nullptr && options->onHoverValuesChanged)
        { options->onHoverValuesChanged (false); options->onHoverValuesChanged (true); }
        if (actions != nullptr && actions->onNewWorld)
        { restored->savedRunScore.store (234); actions->onNewWorld(); CHECK (restored->savedRunScore.load() == 0); }
    }
    // A new run must start its own four-second bloom cooldown rather than
    // inheriting elapsed time from the preceding run.
    ScoreSystem runScore;
    EngineSnapshot bloomSnapshot;
    bloomSnapshot.population = 1;
    runScore.update (bloomSnapshot, 5.0);
    runScore.reset();
    bloomSnapshot.population = 20;
    runScore.update (bloomSnapshot, 0.1);
    bool earlyBloom = false;
    for (const auto& event : runScore.events())
        earlyBloom |= event.text == "BLOOM";
    CHECK (! earlyBloom);

    // Legitimate growth after four seconds must still award the bloom.
    bloomSnapshot.population = 40;
    runScore.update (bloomSnapshot, 4.1);
    bool earnedBloom = false;
    for (const auto& event : runScore.events())
        earnedBloom |= event.text == "BLOOM";
    CHECK (earnedBloom);
    // Loading a damaged run must not produce negative scores or an invalid
    // elapsed time; score restoration is also used for DAW project state.
    runScore.restoreRun (-750, -12.0);
    CHECK (runScore.score() == 0);
    CHECK (runScore.elapsed() == 0.0);
    runScore.restoreRun (1250, std::numeric_limits<double>::quiet_NaN());
    CHECK (runScore.score() == 1250);
    CHECK (runScore.elapsed() == 0.0);
    runScore.restoreRun (1250, std::numeric_limits<double>::infinity());
    CHECK (runScore.elapsed() == 0.0);
    runScore.restoreRun (1250, 32.5);
    CHECK (runScore.score() == 1250 && runScore.elapsed() == 32.5);

    std::printf ("Host feature tests: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
