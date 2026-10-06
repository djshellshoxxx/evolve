#include "HelpView.h"
#include "ParamControl.h"

namespace mutagen
{
    using namespace theme;

    // =====================================================================
    //  The manual.
    // =====================================================================

    const std::vector<HelpSection>& HelpView::content()
    {
        static const std::vector<HelpSection> sections =
        {
            { "Overview", {
                { {}, "MUTAGEN is a synthesiser you garden rather than program. "
                      "Instead of an oscillator and a filter, it holds a colony of "
                      "small synthesis cells that reproduce, mutate, compete and die. "
                      "What you hear is the colony; what you control is the world it "
                      "lives in." },
                { {}, "There is no patch to dial in. You set the conditions - how much "
                      "food, how much mutation, how hard selection bites, what counts "
                      "as fit - and the colony finds its own sound under those "
                      "conditions. Leave it alone and it keeps moving. Change the "
                      "conditions and it adapts, over seconds or over minutes "
                      "depending on how fast you let it live." },
                { {}, "Every instance also rolls its own WORLD at startup: a region of "
                      "timbre space, a palette, a set of tuning biases. Two copies of "
                      "MUTAGEN on the same preset will not converge on the same sound, "
                      "because they are not in the same world." },
                { "In short", "Feed it, stress it, select for what you like, and steer. "
                              "You are a gardener with a radiation gun, not a "
                              "sound designer with a signal path." } } },

            { "Getting Started", {
                { "1. Make a noise",
                  "Play a note, or turn on Synth Drone in the FX rack if you want "
                  "sound without MIDI. The colony is an instrument: by default it "
                  "sounds when it is played." },
                { "2. Watch the dish",
                  "The Culture Chamber in the middle is the colony. Each blob is a "
                  "cell; its colour is its species, its size is how loud it is, and "
                  "its position is where it sits in timbre space." },
                { "3. Nudge the world",
                  "Raise NUTRIENTS to let the population grow. Raise MUTATION to make "
                  "it restless. Raise SELECTION to make fitness matter more. Small "
                  "moves; the colony amplifies them." },
                { "4. Say what you like",
                  "The SELECT knobs define fitness. Push SELECT: BRIGHT up and, over "
                  "a few generations, the dark cells stop reproducing. This is the "
                  "main steering wheel." },
                { "5. Play with it",
                  "Click in the chamber to mutate a cell. Drag to leave a wake. "
                  "Right-click to strip a cell back. Press RADIATE and accept that it "
                  "sometimes goes badly." },
                { "6. Keep what works",
                  "Hit FREEZE to stop evolution and hold the current sound, or save "
                  "the whole run from the FILE menu so you can come back to it." } } },

            { "The Window", {
                { {}, "Top to bottom, the window is: header, main area, timeline." },
                { "Header (upper strip)",
                  "The plugin name and the organism's name on the left - click the "
                  "organism name to rename it. On the right: the FILE menu, the preset "
                  "selector with its step arrows, the A/B compare pair, the OPTIONS "
                  "gear and this manual. The dot next to the name is lit while the "
                  "colony is exploring and dim while it is preserved." },
                { "Verb strip (lower)",
                  "Role and CPU budget, the live colony statistics, EXPLORING / "
                  "PRESERVED, and the run verbs: Clone, Freeze, Reanimate, Export, "
                  "RANDOM and RESET. Perform / FX / Inspector on the right open and "
                  "close the larger views." },
                { "A / B",
                  "Two whole parameter states you can flip between while the same "
                  "colony keeps playing. The first time you switch, the slot you are "
                  "leaving is copied across so you are always comparing against "
                  "something; the arrow button copies the slot you are in over the "
                  "other one." },
                { "Germination (left)",
                  "Where a colony comes from: the seed material, how many cells it "
                  "starts with, and the species mix." },
                { "Culture Chamber (centre)",
                  "The colony itself, and the play surface. Also the score HUD, which "
                  "floats over it." },
                { "Action bar (under the chamber)",
                  "One-shot verbs: ENZYME, CATALYST, HEAT, WATER, RADIATE, plus the "
                  "three gesture knobs and the run controls." },
                { "Inspector (right)",
                  "The genome of whatever is selected - a cell, a family, a species "
                  "or the whole colony." },
                { "Environment (far right)",
                  "The ecology: the five large controls and the deeper ones under "
                  "them, plus the selection targets." },
                { "Timeline (bottom)",
                  "Every generation the colony has passed through. Click one to look "
                  "at it; drag it to the Breeding Lab to cross it with another." },
                { "Signature",
                  "The small diagonal notch in the top-left corner and the version "
                  "number in the bottom-right are the house marks - they appear on "
                  "every plugin in the range." } } },

            { "The Culture Chamber", {
                { {}, "The chamber is not a display. It is the instrument's play "
                      "surface, and every mouse gesture in it changes the sound." },
                { "Left click", "Mutates the cell you clicked, and sends a ripple "
                                "through the wave field. Cheap, safe, repeatable." },
                { "Left drag",  "Leaves a wake. The faster you drag, the stronger the "
                                "disturbance - the colony is stirred, not just poked." },
                { "Right click / drag",
                  "Strips a cell back toward its primitive form. Use it to thin out a "
                  "texture that has gone opaque." },
                { "Ctrl + right click",
                  "Opens the selection menu: choose the cell, its family, its species "
                  "or the whole colony as the scope for the Inspector and the "
                  "Breeding Lab." },
                { "Drag and drop",
                  "Drop an audio file onto the chamber to feed it to the colony. See "
                  "FEEDING THE COLONY." },
                { "Colour",     "Saturation tracks variety: a colourful dish is a varied "
                                "one. As the colony drifts toward noise it desaturates, "
                                "so grey is a warning you can see before you hear it." },
                { "The ghost",  "A branching fractal structure that appears when the "
                                "colony has been varied, appealing and scoring well for "
                                "a while. It is a reward, not a control." } } },

            { "Field Journal", {
                { "JOURNAL", "Opens the persistent research record. Progress survives "
                             "between runs and is kept separately from presets, scores "
                             "and organism files." },
                { "Research levels", "Earned from score, discoveries, generations, worlds, "
                                     "rescues, source digestion, breeding, steering, anomalies "
                                     "and other meaningful play." },
                { "Researchers", "Six characters unlock through different styles of play. "
                                  "Their briefings add context and point toward systems you "
                                  "may not have explored yet." },
                { "Story", "Eight chapters reveal as the evidence accumulates. The story "
                             "does not pause the instrument and no branch deletes content." },
                { "Challenges", "The journal keeps three objectives appropriate to your "
                                  "current research level, encouraging under-used systems." },
                { "Anomalies and relics", "Rare descriptor combinations are recorded once. "
                                            "Major lifetime milestones award permanent relics." },
                { "Three Directives", "Late research reveals PRESERVE, ACCELERATE and RELEASE. "
                                       "The highlighted directive reflects your play history; "
                                       "all three remain available." } } },

            { "Environment", {
                { {}, "The five large controls do most of the work. Everything under "
                      "them is refinement." },
                { "Nutrients",  "Food supply. High means the population grows and cells "
                                "stay loud; low means a thin, starved dish." },
                { "Mutation",   "How often a genome changes when it reproduces. The "
                                "restlessness control." },
                { "Selection",  "How much fitness matters. At zero, reproduction is "
                                "random. At maximum, only the fit breed." },
                { "Metabolism", "How fast the colony lives. Sets the rate everything "
                                "else happens at." },
                { "Stability",  "Resistance to change. High stability damps mutation "
                                "and keeps a found sound." },
                { "Fertility",  "Reproduction rate, independent of food." },
                { "Mutation Depth", "How *large* a mutation is when one happens. "
                                    "Mutation is frequency; this is magnitude." },
                { "Radiation",  "Background mutation pressure applied to everything, "
                                "not just to offspring. Some of it is fatal." },
                { "Temperature","Raises the energy of the whole dish: faster movement, "
                                "shorter lives, more accidents." },
                { "Competition","How hard cells fight for the same niche. High values "
                                "thin the colony to a few winners." },
                { "Symbiosis",  "The opposite pull: cells reinforce each other and "
                                "textures thicken." },
                { "Lifespan",   "How long a cell lives before it dies of age." },
                { "Apoptosis",  "Programmed death rate - cells removing themselves." },
                { "Diversity",  "Pressure to stay spread out across timbre space "
                                "rather than clustering." },
                { "Migration",  "How readily traits cross between islands. High "
                                "migration spreads a good mutation through the whole "
                                "colony; low keeps sub-populations distinct." },
                { "PUSH THE SOUND",
                  "The eight direction buttons are shortcuts for direct selection. "
                  "DARK/BRIGHT, SPARSE/DENSE, NOISE/TONE and CALM/FIERCE clear the "
                  "other selection axes, set one target, and apply a selection burst "
                  "immediately. PUSH STRENGTH controls whether that move is a gentle "
                  "nudge or a hard evolutionary turn." },
                { "COUNTER-EVOLVE",
                  "Measures the current brightness, tonalness and roughness, then "
                  "selects in the opposite direction and forces a mutation pass. Use "
                  "it when a colony has become too settled in one character." },
                { "MUTATION DICE",
                  "Throws a new target across all four sound axes, raises mutation to "
                  "a bounded amount, adds divergence pressure and mutates once. It is "
                  "the deliberate surprise button." } } },

            { "Selection Targets", {
                { {}, "These define what 'fit' means right now. They are the steering "
                      "wheel: everything else is throttle." },
                { "Select: Bright",     "Negative selects for dark cells, positive for "
                                        "bright ones. Centre is neutral." },
                { "Select: Dense",      "Sparse versus dense textures." },
                { "Select: Harmonic",   "Noisy versus harmonic material." },
                { "Select: Aggressive", "Calm versus aggressive behaviour." },
                { "Select: Diverge",    "Rewards being unlike anything the colony has "
                                        "already been. Turn this up and it will not "
                                        "repeat itself - and will not settle." },
                { "Note", "Selection acts across generations, not instantly. Set a "
                          "target and give it time; if nothing is happening, "
                          "SELECTION or METABOLISM is probably too low." } } },

            { "Germination", {
                { "Source",        "What the first generation is grown from: a loaded "
                                   "sample, live input, one of the primitives, or a "
                                   "preserved organism. TONE is the default - a "
                                   "plucked harmonic seed." },
                { "Capture Length","How much material to take when capturing." },
                { "Transient Sensitivity",
                                   "How finely the source is cut into grains. High "
                                   "values find more attack points." },
                { "Initial Population", "How many cells the colony starts with." },
                { "Grain / Spectral / Resonator share",
                                   "The species mix at germination. Grain granulates "
                                   "the source, Spectral resynthesises it as partials, "
                                   "Resonator rings it through tuned filters." },
                { "Why TONE is the default",
                  "Granular synthesis of a noise seed produces noise no matter how "
                  "well the colony evolves. Seeding with a tonal primitive is what "
                  "lets the ecology actually show." } } },

            { "The Action Bar", {
                { {}, "Every control here is a verb. None of them is a setting you "
                      "can return to." },
                { "Add Enzyme",  "An enzyme digests. Sparkles go into the sound and "
                                 "the picture, and something is taken away - usually "
                                 "the roughest or most crowded material, so the usual "
                                 "result is cleaner. Roughly one press in twelve eats "
                                 "something the sound needed. Dose and target are "
                                 "re-rolled every press." },
                { "Add Catalyst","A fast wobble up and down in pitch, which quietly "
                                 "subtracts something as it fades. The excursion is "
                                 "what you hear; the removal it leaves behind is the "
                                 "actual effect. Louder gesture than the enzyme, "
                                 "gentler consequence - and also re-rolled each press." },
                { "Add Heat",    "Speeds up one randomly chosen thing: an oscillator, "
                                 "a modulation lane, or the loop/grain rate. Which of "
                                 "the three it catches is part of the roll." },
                { "Add Water",   "The opposite hand: slows one randomly chosen "
                                 "oscillator, lane or loop rate down." },
                { "Radiate",     "The gamble, and the only one with fixed odds: 5% "
                                 "kills the sound and resets the score to zero, 10% "
                                 "adds something beneficial, and the remaining 85% is "
                                 "a minor, mostly neutral mutation. A red flash and a "
                                 "few seconds of Geiger clicks tell you it landed." },
                { "Gesture knobs (PITCH / LFO / OSC)",
                  "These spring back to centre because there is nothing to return to. "
                  "What they send is a gesture - direction, size and speed - and the "
                  "engine decides what it means, partly at random. They can improve "
                  "the sound or damage it." },
                { "New World",   "Re-rolls the rules of the run: tuning, palette, "
                                 "tempo, routing. A different place, not a different "
                                 "patch." },
                { "Save Run / Load Run", "The whole run - colony, score and statistics "
                                         "- to a .mutagen file. Also on the FILE menu." },
                { "Scores",      "The high-score table." } } },

            { "Feeding the Colony", {
                { {}, "The colony can eat audio, and what it eats changes what it is." },
                { "Drag and drop", "Drop a WAV, AIFF, FLAC or MP3 onto the chamber." },
                { "Arm Mic / Capture",
                  "Arms the input and records a short capture. Output is muted while "
                  "capturing by default, because an open microphone next to monitors "
                  "is a feedback loop. There is a howl detector under that, and a hard "
                  "output ceiling under that." },
                { "How it is digested",
                  "Samples are *stitched*, not mixed. Summing several unrelated "
                  "recordings just makes noise, so each new drop replaces crossfaded "
                  "segments of the digest buffer, and the share it replaces shrinks "
                  "with each one. The newest sample can never completely displace what "
                  "the colony already is." } } },

            { "Score and Play", {
                { {}, "MUTAGEN keeps score. The rule rewards a varied, moving, "
                      "listenable colony - not a loud one and not a pretty one." },
                { "Variety",  "The main term. A colony exploring many timbres scores "
                              "far more than one holding a single sound." },
                { "Noise",    "Drifting toward noise sags the rate long before it "
                              "bites, so the number starts falling while there is "
                              "still time to act." },
                { "Frozen",   "If the colony noise-locks or dies out, scoring stops "
                              "and the reason is shown. Clearing a lock pays a bonus, "
                              "and acting while frozen builds combo at double rate - "
                              "that is the move the game is teaching." },
                { "Combo",    "Keep interacting and the multiplier climbs. Leave it "
                              "alone and it decays." },
                { "Events",   "NEW TIMBRE, BLOOM, GENERATION, COLONY RESCUED, MOVING "
                              "AGAIN, DIGESTED, FATAL DOSE, BENEFICIAL MUTATION." },
                { "High scores", "Saving a run files it on the board. The table lives "
                                 "alongside the settings file." } } },

            { "FX Rack and Performance", {
                { {}, "The colony is the instrument; the rack is the signal it flows "
                      "into. Open it with FX in the header." },
                { "Oscillator bank", "A full subtractive voice - four oscillators into "
                                     "a shared filter and ADSR." },
                { "Synth Blend",     "Bipolar. Positive layers the voice over the "
                                     "colony; negative phase-cancels it against the "
                                     "colony, carving holes instead of adding." },
                { "Filter",          "Low/high/band/notch with drive, modulated by "
                                     "LFO 1." },
                { "LFOs",            "Four. Three are audio-rate destinations (filter, "
                                     "volume, pan); the fourth modulates the *ecology* "
                                     "- pick its destination with LFO Mutation Dest." },
                { "EQ",              "Three band: low shelf, mid peak, high shelf." },
                { "Gator",           "A 16-step rhythmic gate, tempo-synced." },
                { "Glitch",          "Beat-repeat, stutter, reverse, crush and "
                                     "tape-stop on a per-slice probability." },
                { "Performance view","Eight macros and an XY pad for playing the "
                                     "ecology live rather than editing it." } } },

            { "Presets and Files", {
                { "Preset selector", "In the header. The factory bank is grouped by "
                                     "category; your own presets appear underneath." },
                { "What a preset holds",
                  "The parameter surface only - the world, not the organism. Loading a "
                  "preset changes the conditions the colony lives in and lets it "
                  "react, rather than replacing the thing that has been evolving." },
                { "File > Save Preset",    "Overwrites the loaded preset." },
                { "File > Save Preset As", "Writes a new .mutagenpreset file." },
                { "File > Open Preset",    "Loads one from anywhere on disk." },
                { "File > Save / Load Run",
                  "A run (.mutagen) is the whole thing: parameters, the living colony, "
                  "its history, the score and the run statistics. This is what to use "
                  "when you want to come back to a colony, not just its conditions." },
                { "File > Export Audio",
                  "Renders the colony to a WAV file. Choose the length when asked. "
                  "Because the colony keeps evolving while it renders, two exports of "
                  "the same state will not be identical." },
                { "Random",
                  "A completely new set of settings every press. The first press "
                  "randomises from where you are; every press after that resets to "
                  "defaults first, so press two is genuinely new rather than a drift "
                  "away from press one. Output level, dry/wet, CPU budget and role are "
                  "left alone." },
                { "Reset",
                  "Everything back to factory: all parameters to default, history and "
                  "Breeding Lab cleared, source back to the default primitive, and the "
                  "colony regrown from a fresh seed." } } },

            { "Controls and Options", {
                { "Right-click any control",
                  "Opens the control menu: Set Value (type an exact number), Reset to "
                  "Default, and Map to MIDI." },
                { "Map to MIDI",
                  "Choose it, then move a knob or fader on your controller. The first "
                  "CC that moves claims the parameter, and that same message sets the "
                  "value so the gesture lands immediately. A parameter answers to one "
                  "CC at a time; mapping it again replaces the old binding. Mappings "
                  "are saved with the plugin state." },
                { "Double-click",  "Resets a knob to its default." },
                { "Drag",          "Vertical on knobs and faders. Hold Shift for "
                                   "coarse, Ctrl (Cmd on macOS) for ultra-fine." },
                { "Tooltips",      "Hover any control for a moment and it explains "
                                   "itself. Turn them off in OPTIONS if you would "
                                   "rather not see them." },
                { "Options > Audio & MIDI",
                  "Only meaningful in the standalone app - choose the audio device, "
                  "sample rate, buffer size and MIDI inputs. Inside a host, the host "
                  "owns all of that, so the page says so instead of pretending." } } },

            { "Notes and Limits", {
                { "CPU",
                  "CPU Quality in the header caps the population: Eco 28 cells, "
                  "Balanced 56, Pristine 112. If the colony sounds like it is "
                  "struggling, that cap is the first thing to lower." },
                { "It keeps moving",
                  "By design, MUTAGEN does not hold still unless you tell it to. If "
                  "you need a fixed sound, use FREEZE (preserve mode) or render it to "
                  "audio." },
                { "Two instances differ",
                  "Same preset, different worlds, different entropy. That is the "
                  "point, and it is why a preset cannot promise you an exact sound." },
                { "How noise is detected",
                  "Two measurements. One asks how far the spectrum departs from its "
                  "own smoothed envelope - energy in partials, or smeared between "
                  "them. On its own that one is fooled by this instrument, because "
                  "every cell wobbles and a partial carrying vibrato sweeps across "
                  "many analysis bins, which looks exactly like broadband noise. So "
                  "the second measurement asks whether the waveform repeats at all, "
                  "which vibrato does not disturb: a wobbling note is still a note, "
                  "and noise repeats at no period. The verdict weights the second "
                  "more heavily." } } },
        };
        return sections;
    }

    // =====================================================================
    //  The scrolling page
    // =====================================================================

    class HelpView::Page : public juce::Component
    {
    public:
        void setSection (const HelpSection& s)
        {
            section = &s;
            layout();
            repaint();
        }

        void layout()
        {
            if (section == nullptr) return;

            // Measure first so the viewport gets a real height. Text height
            // depends on width, so this has to run again on every resize.
            const int contentWidth = juce::jmax (240, getWidth() - pad * 2);
            const int termWidth    = juce::jmin (168, contentWidth / 3);

            int y = pad;
            y += 34;    // section title

            for (const auto& e : section->entries)
            {
                const bool isDefinition = e.term.isNotEmpty();
                const int bodyWidth = isDefinition ? contentWidth - termWidth - gridUnit
                                                   : contentWidth;

                juce::AttributedString s;
                s.append (e.body, uiFont (13.0f), text);
                juce::TextLayout tl;
                tl.createLayout (s, (float) bodyWidth);

                const int h = juce::jmax ((int) std::ceil (tl.getHeight()),
                                          isDefinition ? 18 : 0);
                y += h + (isDefinition ? gridUnit + 2 : gridUnit * 2);
            }

            setSize (getWidth(), y + pad);
        }

        void resized() override { layout(); }

        void paint (juce::Graphics& g) override
        {
            if (section == nullptr) return;

            const int contentWidth = juce::jmax (240, getWidth() - pad * 2);
            const int termWidth    = juce::jmin (168, contentWidth / 3);

            int y = pad;

            drawSectionHeader (g, { pad, y, contentWidth, 20 },
                               section->title, accent);
            y += 34;

            for (const auto& e : section->entries)
            {
                const bool isDefinition = e.term.isNotEmpty();
                const int bodyWidth = isDefinition ? contentWidth - termWidth - gridUnit
                                                   : contentWidth;
                const int bodyX = isDefinition ? pad + termWidth + gridUnit : pad;

                juce::AttributedString s;
                s.append (e.body, uiFont (13.0f), isDefinition ? text : textDim);
                juce::TextLayout tl;
                tl.createLayout (s, (float) bodyWidth);
                tl.draw (g, juce::Rectangle<float> ((float) bodyX, (float) y,
                                                    (float) bodyWidth, tl.getHeight()));

                if (isDefinition)
                {
                    g.setColour (accent2);
                    drawTracked (g, e.term.toUpperCase(),
                                 { pad, y, termWidth, 18 },
                                 juce::Justification::topLeft, uiFont (11.0f, true), 0.08f);
                }

                const int h = juce::jmax ((int) std::ceil (tl.getHeight()),
                                          isDefinition ? 18 : 0);
                y += h + (isDefinition ? gridUnit + 2 : gridUnit * 2);
            }
        }

    private:
        static constexpr int pad = edgePadding;
        const HelpSection* section = nullptr;
    };

    // =====================================================================

    HelpView::HelpView()
    {
        page = std::make_unique<Page>();
        viewport.setViewedComponent (page.get(), false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        const auto& sections = content();
        for (int i = 0; i < (int) sections.size(); ++i)
        {
            auto* b = navButtons.add (new juce::TextButton (sections[(size_t) i].title));
            b->setClickingTogglesState (true);
            b->setRadioGroupId (0x11e19);
            b->setTooltip ("Jump to " + sections[(size_t) i].title);
            b->onClick = [this, i] { showSection (i); };
            addAndMakeVisible (b);
        }

        closeBtn.setTooltip ("Close the help window");
        closeBtn.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (closeBtn);

        showSection (0);
    }

    HelpView::~HelpView() = default;

    void HelpView::showSection (int index)
    {
        const auto& sections = content();
        if (! juce::isPositiveAndBelow (index, (int) sections.size())) return;

        currentSection = index;
        if (auto* b = navButtons[index])
            b->setToggleState (true, juce::dontSendNotification);

        page->setSection (sections[(size_t) index]);
        viewport.setViewPosition (0, 0);
    }

    void HelpView::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();

        juce::DropShadow (shadow, 16, { 0, 4 }).drawForRectangle (g, getLocalBounds());

        g.setColour (bg0);
        g.fillRoundedRectangle (r, radiusWindow);
        g.setColour (stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), radiusWindow, 1.0f);

        // header strip
        auto header = getLocalBounds().removeFromTop (headerHeight);
        g.setColour (panel);
        g.fillRect (header);
        g.setColour (stroke);
        g.drawHorizontalLine (header.getBottom() - 1, r.getX(), r.getRight());

        g.setColour (text);
        g.setFont (uiFont (14.0f, true));
        g.drawText ("MUTAGEN Help", header.withTrimmedLeft (edgePadding),
                    juce::Justification::centredLeft);

        // nav / content separator
        const int navRight = edgePadding + navWidth;
        g.setColour (stroke);
        g.drawVerticalLine (navRight + gridUnit, (float) headerHeight + gridUnit,
                            (float) getHeight() - 30.0f);

        // footer: version, in mono, the house way
        auto footer = getLocalBounds().removeFromBottom (24).reduced (edgePadding, 0);
        g.setColour (textDim);
        g.setFont (monoFont (9.0f));
        g.drawText ("MUTAGEN v" + juce::String (JucePlugin_VersionString)
                        + "   -   Colonyworks",
                    footer, juce::Justification::centredRight);

        drawSignatureNotch (g, getLocalBounds());
    }

    void HelpView::resized()
    {
        auto r = getLocalBounds();

        auto header = r.removeFromTop (headerHeight);
        closeBtn.setBounds (header.removeFromRight (72).reduced (gridUnit, 3));

        r.removeFromBottom (24);                       // footer
        r = r.reduced (edgePadding, gridUnit);

        auto nav = r.removeFromLeft (navWidth);
        for (auto* b : navButtons)
        {
            b->setBounds (nav.removeFromTop (24));
            nav.removeFromTop (2);
        }

        r.removeFromLeft (gridUnit * 2);
        viewport.setBounds (r);
        page->setSize (r.getWidth() - 12, page->getHeight());
    }
}
