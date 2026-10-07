// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "SecretLayer.h"
#include "CultureChamber.h"
#include "StoryPanel.h"
#include "MutagenLookAndFeel.h"
#include "../PluginProcessor.h"

namespace mutagen
{
    namespace
    {
        juce::Colour rgb (unsigned int hex) { return juce::Colour (0xff000000u | hex); }

        juce::String utf8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

        /** Draws a line of text mirrored left-right or turned upside down. */
        void drawOdd (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r,
                      secrets::Glyphs glyphs, juce::Justification just)
        {
            juce::Graphics::ScopedSaveState save (g);
            const auto c = r.getCentre();
            if (glyphs == secrets::Glyphs::mirrored)
                g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f, c.x, c.y));
            else
                g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::pi, c.x, c.y));
            g.drawText (text, r, just, false);
        }

        juce::ColourGradient rainbow (juce::Rectangle<float> r, float phase)
        {
            juce::ColourGradient grad (juce::Colour::fromHSV (phase, 0.85f, 1.0f, 1.0f), r.getX(), r.getCentreY(),
                                       juce::Colour::fromHSV (std::fmod (phase + 0.85f, 1.0f), 0.85f, 1.0f, 1.0f),
                                       r.getRight(), r.getCentreY(), false);
            for (int i = 1; i < 6; ++i)
                grad.addColour (i / 6.0, juce::Colour::fromHSV (std::fmod (phase + i / 6.0f, 1.0f), 0.85f, 1.0f, 1.0f));
            return grad;
        }
    }

    SecretLayer::SecretLayer (MutagenProcessor& p, CultureChamber& c, StoryPanel& s)
        : processor (p), chamber (c), story (s)
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (false);
        chamber.addMouseListener (this, true);

        nextVisitorAt = 70.0 + rng.nextDouble() * 120.0;
        nextPolyglotAt = 100.0 + rng.nextDouble() * 160.0;
    }

    SecretLayer::~SecretLayer()
    {
        chamber.removeMouseListener (this);
    }

    void SecretLayer::ensureAnimating()
    {
        if (! isTimerRunning()) startTimerHz (60);
    }

    // ---- input ----------------------------------------------------------------

    void SecretLayer::mouseDown (const juce::MouseEvent& e)
    {
        const auto local = e.getEventRelativeTo (&chamber).position;
        const auto w = (float) juce::jmax (1, chamber.getWidth());
        const auto h = (float) juce::jmax (1, chamber.getHeight());

        const auto t = juce::Time::getCurrentTime();
        secrets::Context c;
        c.nx = juce::jlimit (0.0, 1.0, (double) (local.x / w));
        c.ny = juce::jlimit (0.0, 1.0, (double) (local.y / h));
        c.right = e.mods.isPopupMenu();
        c.now = clock;
        c.hour = t.getHours();
        c.minute = t.getMinutes();
        c.second = t.getSeconds();
        c.weekday = t.getDayOfWeek();
        c.day = t.getDayOfMonth();
        c.month = t.getMonth() + 1;
        c.score = liveScore;
        c.generation = liveGeneration;
        c.population = livePopulation;
        c.lexicon = story.progress().lexiconCount();
        c.actsSeen = story.progress().highestActSeen;
        c.secretsFound = story.progress().secretCount();
        c.gateActive = processor.temporaryGatorActive();
        c.delayActive = processor.tripDelayActive();
        c.sinceTwist = story.secondsSinceTwist();
        c.tunerSpeaking = story.currentSpeaker() == story::Speaker::tuner;

        if (roomId >= 0) return;   // one room at a time
        if (const int id = tracker.click (c, story.progress().secrets); id >= 0)
            openSecret (id);
    }

    // ---- secrets ----------------------------------------------------------------

    void SecretLayer::openSecret (int id)
    {
        const auto& s = secrets::all()[(std::size_t) id];
        roomId = id;
        roomAge = 0.0f;
        roomVisitors.clear();
        if (s.burst == secrets::Burst::rockets || s.skill == secrets::Skill::rockets)
            for (int i = 0; i < (int) secrets::visitors().size(); ++i)
                if (id == secrets::secretCount - 1 || rng.nextInt (3) == 0) roomVisitors.push_back (i);

        story.markSecret (id);
        processor.triggerSkillSound (52000 + id * 173);   // every room has its own voice
        applySkill (s.skill, s.skillParam);
        applyBurst (s.burst, s.burstCount);
        story.collectSoon (juce::String ("secret ") + s.name, 0.4, 3.0f);

        const float weight = s.tier == secrets::Tier::easy ? 0.5f
                           : s.tier == secrets::Tier::medium ? 0.75f : 1.0f;
        if (onReward) onReward (weight, juce::String ("SECRET ROOM  ") + s.room);

        ensureAnimating();
    }

    void SecretLayer::applySkill (secrets::Skill skill, int param)
    {
        using secrets::Skill;
        using story::Effect;
        switch (skill)
        {
            case Skill::mutate:  story.perform (Effect::mutate, { story::kNo, story::kNo, story::kNo, story::kNo }, 0); break;
            case Skill::radiate: story.perform (Effect::radiate, { story::kNo, story::kNo, story::kNo, story::kNo }, 0); break;
            case Skill::gate:    story.perform (Effect::gate, { story::kNo, story::kNo, story::kNo, story::kNo }, param); break;
            case Skill::delay:   story.perform (Effect::delay, { story::kNo, story::kNo, story::kNo, story::kNo }, param); break;
            case Skill::chord:   story.perform (Effect::chord, { param, param + 4, param + 7, param + 12 }, 0); break;
            case Skill::scale:   story.perform (Effect::scale, { story::kNo, story::kNo, story::kNo, story::kNo }, param); break;
            case Skill::haunted: story.perform (Effect::haunted, { story::kNo, story::kNo, story::kNo, story::kNo }, param); break;
            case Skill::rockets:
            {
                const auto b = getLocalBounds().toFloat();
                for (int i = 0; i < 5; ++i)
                    launchRocket ({ b.getX() + b.getWidth() * (0.15f + 0.175f * (float) i), b.getBottom() - 4.0f },
                                  juce::Colour::fromHSV (rng.nextFloat(), 0.8f, 1.0f, 1.0f), 61500 + i * 41);
                break;
            }
        }
    }

    void SecretLayer::applyBurst (secrets::Burst burst, int count)
    {
        using secrets::Burst;
        switch (burst)
        {
            case Burst::special:   chamber.spawnSpecialOrbs (count); break;
            case Burst::rainbow:   chamber.spawnRainbowOrbs (count, false); break;
            case Burst::glowing:   chamber.spawnRainbowOrbs (count, true); break;
            case Burst::monster:   chamber.spawnMonsterOrbs (count); break;
            case Burst::mini:      chamber.spawnMiniOrbs (count); break;
            case Burst::snow:      chamber.triggerSnowFireworks ((float) juce::jlimit (3, 20, count)); break;
            case Burst::inversion: chamber.setOrbInversion ((float) juce::jlimit (3, 30, count)); break;
            case Burst::speed:     chamber.setOrbSpeedBoost (2.5f, (float) juce::jlimit (3, 20, count)); break;
            case Burst::rockets:
            {
                const auto b = getLocalBounds().toFloat();
                for (int i = 0; i < juce::jlimit (1, 30, count); ++i)
                    launchRocket ({ b.getX() + b.getWidth() * rng.nextFloat(), b.getBottom() - 2.0f },
                                  juce::Colour::fromHSV (rng.nextFloat(), 0.85f, 1.0f, 1.0f), 61600 + i * 29);
                break;
            }
        }
    }

    // ---- visitors, rockets, polyglots -----------------------------------------------

    void SecretLayer::spawnVisitor (int which)
    {
        const auto& v = secrets::visitors()[(std::size_t) which];
        const bool fromLeft = rng.nextBool();
        const float w = (float) getWidth();
        Walker wk;
        wk.who = which;
        wk.x = fromLeft ? -60.0f : w + 60.0f;
        wk.targetX = w * (0.2f + 0.6f * rng.nextFloat());
        wk.age = 0.0f;
        wk.life = 9.0f + rng.nextFloat() * 4.0f;
        wk.nextRocket = 2.0f;
        wk.speech = secrets::gibberish ((std::uint64_t) rng.nextInt64(), 3 + rng.nextInt (4));
        walkers.push_back (wk);
        processor.triggerSkillSound (v.rocketRecipe + 7);   // a little arrival chirp
        ensureAnimating();
    }

    void SecretLayer::launchRocket (juce::Point<float> from, juce::Colour c, int recipe)
    {
        if (rockets.size() > 40) return;
        Rocket r;
        r.p = from;
        r.v = { (rng.nextFloat() - 0.5f) * 90.0f, -(260.0f + rng.nextFloat() * 180.0f) };
        r.c = c;
        r.fuse = 0.9f + rng.nextFloat() * 0.7f;
        r.recipe = recipe;
        rockets.push_back (std::move (r));
        ensureAnimating();
    }

    void SecretLayer::startPolyglot (int which)
    {
        const auto& p = secrets::polyglots()[(std::size_t) which];
        polyglot = which;
        polyAge = 0.0f;
        applySkill (p.skill, p.param);
        applyBurst (p.burst, p.count);
        processor.triggerSkillSound (57000 + which * 131);
        if (onReward) onReward (0.3f, juce::String (p.language).toUpperCase() + ": " + juce::String (p.meaning).toUpperCase());
        ensureAnimating();
    }

    // ---- orbs loose on the desktop ------------------------------------------------

    /** A transparent, click-through window over the whole monitor. Yellow orbs
        wander it (P); blue orbs race to its right edge and burst (A). */
    class SecretLayer::Roamer : public juce::Component, private juce::Timer
    {
    public:
        Roamer (bool blueRun, juce::Point<int> origin) : blue (blueRun)
        {
            setInterceptsMouseClicks (false, false);
            setOpaque (false);
            setAlwaysOnTop (true);
            const auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForPoint (origin);
            const auto area = d != nullptr ? d->userArea : juce::Rectangle<int> (0, 0, 1280, 800);
            setBounds (area);
            addToDesktop (juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowIgnoresKeyPresses
                          | juce::ComponentPeer::windowIsSemiTransparent | juce::ComponentPeer::windowIgnoresMouseClicks);
            setVisible (true);

            juce::Random r;
            const auto start = (origin - area.getPosition()).toFloat();
            for (int i = 0; i < 40; ++i)
            {
                const float a = r.nextFloat() * juce::MathConstants<float>::twoPi;
                const float sp = 120.0f + r.nextFloat() * 220.0f;
                orbs.push_back ({ start + juce::Point<float> (r.nextFloat() * 40.0f - 20.0f, r.nextFloat() * 40.0f - 20.0f),
                                  { std::cos (a) * sp, std::sin (a) * sp }, 0.0f, false });
            }
            startTimerHz (60);
        }

        bool finished() const { return age > (blue ? 7.0f : 14.0f); }

        void paint (juce::Graphics& g) override
        {
            for (const auto& o : orbs)
            {
                if (o.burst) continue;
                const auto c = blue ? juce::Colour (0xff3d8bff) : juce::Colour (0xfff2d64e);
                g.setColour (c.withAlpha (0.25f));
                g.fillEllipse (o.p.x - 14, o.p.y - 14, 28, 28);
                g.setColour (c);
                g.fillEllipse (o.p.x - 7, o.p.y - 7, 14, 14);
            }
            for (const auto& s : sparks)
            {
                g.setColour (s.c.withAlpha (juce::jlimit (0.0f, 1.0f, s.life)));
                g.fillEllipse (s.p.x - 2, s.p.y - 2, 4, 4);
            }
        }

    private:
        struct O { juce::Point<float> p, v; float wobble; bool burst; };
        struct S { juce::Point<float> p, v; juce::Colour c; float life; };

        void timerCallback() override
        {
            constexpr float dt = 1.0f / 60.0f;
            age += dt;
            const float w = (float) getWidth(), h = (float) getHeight();
            for (auto& o : orbs)
            {
                if (o.burst) continue;
                if (blue)
                {
                    o.v.x += 900.0f * dt;                                // race for the right edge
                    o.v.y *= 0.96f;
                    if (o.p.x > w - 30.0f)
                    {
                        o.burst = true;
                        for (int k = 0; k < 30; ++k)
                        {
                            const float a = rng.nextFloat() * juce::MathConstants<float>::twoPi, sp = 60.0f + rng.nextFloat() * 260.0f;
                            sparks.push_back ({ o.p, { std::cos (a) * sp, std::sin (a) * sp },
                                                juce::Colour::fromHSV (rng.nextFloat(), 0.8f, 1.0f, 1.0f), 1.4f });
                        }
                    }
                }
                else
                {
                    o.wobble += dt;                                      // wander the screen
                    o.v = o.v.rotatedAboutOrigin (std::sin (o.wobble * 1.7f) * 0.03f);
                    if (o.p.x < 0 || o.p.x > w) o.v.x = -o.v.x;
                    if (o.p.y < 0 || o.p.y > h) o.v.y = -o.v.y;
                    if (age > 12.5f) o.burst = rng.nextInt (30) == 0;    // and fade out one by one
                }
                o.p += o.v * dt;
            }
            for (auto& sp : sparks) { sp.v.y += 140.0f * dt; sp.p += sp.v * dt; sp.life -= dt * 0.7f; }
            sparks.erase (std::remove_if (sparks.begin(), sparks.end(), [] (const S& x) { return x.life <= 0.0f; }), sparks.end());
            repaint();
        }

        bool blue;
        float age = 0.0f;
        juce::Random rng;
        std::vector<O> orbs;
        std::vector<S> sparks;
    };

    void SecretLayer::playFate (secrets::Fate fate)
    {
        using secrets::Fate;
        const secrets::FateRule* rule = nullptr;
        for (const auto& r : secrets::fateRules()) if (r.fate == fate) rule = &r;
        if (rule == nullptr) return;

        fateTitle = rule->title;
        fateTitleAge = 0.0f;
        if (rule->points != 0 && onPoints) onPoints (rule->points, rule->title);

        const auto b = getLocalBounds().toFloat();
        switch (fate)
        {
            case Fate::none: break;
            case Fate::wallRain:     dotsToSpawn = 100000; dots.reserve (20000); break;
            case Fate::yellowEscape:
            case Fate::blueRun:
                roamer = std::make_unique<Roamer> (fate == Fate::blueRun, getScreenBounds().getCentre());
                processor.triggerSkillSound (fate == Fate::blueRun ? 63001 : 63101);
                break;
            case Fate::flowers:      chamber.setFlowers (true); chamber.spawnSpecialOrbs (40); break;
            case Fate::seizure:      chamber.triggerSeizure(); processor.triggerHauntedSound (4999, 0.8f); break;
            case Fate::hail:         chamber.triggerHail (12.0f); processor.triggerSkillSound (63301); break;
            case Fate::numberCurse:  if (onLoseSkills) onLoseSkills(); processor.triggerHauntedSound (666, 1.0f); break;
            case Fate::squidDrop:
                chamber.spawnPinkMutants (499);
                story.perform (story::Effect::interval, { -12, -24, -36, story::kNo }, 0);   // the drop
                processor.triggerHauntedSound (2011, 1.0f);
                break;
            case Fate::whitePop:
                for (int i = 0; i < 9; ++i)
                {
                    const float a = rng.nextFloat() * juce::MathConstants<float>::twoPi;
                    bigOrbs.push_back ({ b.getCentre(), { std::cos (a) * 420.0f, std::sin (a) * 420.0f }, 4.0f + i * 0.4f });
                }
                break;
            case Fate::moonCrash:    moonAge = 0.0f; moonLanded = false; break;
            case Fate::turbo:        chamber.triggerTurbo(); processor.triggerSkillSound (63401); break;
            case Fate::yellow:       chamber.spawnYellowOrbs (34); processor.triggerSkillSound (63901); break;
            case Fate::blackSwarm:
                swarm.clear();
                swarm.reserve (9448);
                for (int i = 0; i < 9448; ++i)
                {
                    const float a = rng.nextFloat() * juce::MathConstants<float>::twoPi, sp = 150.0f + rng.nextFloat() * 450.0f;
                    swarm.push_back ({ rng.nextFloat() * b.getWidth(), rng.nextFloat() * b.getHeight(),
                                       std::cos (a) * sp, std::sin (a) * sp, 0xff000000u });
                }
                swarmLife = 14.0f;
                processor.triggerHauntedSound (4242, 0.9f);
                break;
            case Fate::hugeMites:    chamber.spawnGiantsThatBecomeMites (32); processor.triggerSkillSound (63801); break;
        }
        ensureAnimating();
    }

    void SecretLayer::update (double dt, juce::int64 score, int generation, int population, bool paused)
    {
        clock += dt;
        liveScore = score;
        liveGeneration = generation;
        livePopulation = population;
        if (paused || getWidth() < 50) return;

        // Past 7,797,766 points: three times in four, 34 turquoise mites crawl
        // in and groan, loudly. Once per game.
        if (! groanChecked && score > 7797766)
        {
            groanChecked = true;
            if (rng.nextInt (4) < 3)
            {
                for (int i = 0; i < 34; ++i)
                    groaners.push_back ({ { rng.nextFloat() * (float) getWidth(), (float) getHeight() + 10.0f + rng.nextFloat() * 60.0f },
                                          rng.nextFloat() * juce::MathConstants<float>::twoPi, 12.0f + rng.nextFloat() * 4.0f });
                fateTitle = "THE TURQUOISE GROANING";
                fateTitleAge = 0.0f;
                ensureAnimating();
            }
        }

        if (clock >= nextVisitorAt)
        {
            nextVisitorAt = clock + 120.0 + rng.nextDouble() * 200.0;
            spawnVisitor (rng.nextInt ((int) secrets::visitors().size()));
            if (rng.nextInt (4) == 0)   // sometimes they come in pairs
                spawnVisitor (rng.nextInt ((int) secrets::visitors().size()));
        }
        if (clock >= nextPolyglotAt && polyglot < 0)
        {
            nextPolyglotAt = clock + 150.0 + rng.nextDouble() * 240.0;
            startPolyglot (rng.nextInt ((int) secrets::polyglots().size()));
        }
    }

    void SecretLayer::timerCallback()
    {
        constexpr float dt = 1.0f / 60.0f;

        if (roomId >= 0 && (roomAge += dt) > roomLife) { roomId = -1; roomVisitors.clear(); }
        if (polyglot >= 0 && (polyAge += dt) > 5.0f) polyglot = -1;

        const float floorY = (float) getHeight() - 30.0f;
        for (auto it = walkers.begin(); it != walkers.end();)
        {
            auto& w = *it;
            w.age += dt;
            const float leaving = w.age > w.life - 2.0f ? 1.0f : 0.0f;
            const float goal = leaving > 0.0f ? (w.targetX < getWidth() * 0.5f ? -80.0f : getWidth() + 80.0f) : w.targetX;
            w.x += (goal - w.x) * juce::jmin (1.0f, dt * 2.2f);
            if ((w.nextRocket -= dt) <= 0.0f && leaving == 0.0f)
            {
                const auto& v = secrets::visitors()[(std::size_t) w.who];
                launchRocket ({ w.x, floorY - 20.0f }, rgb (v.colour), v.rocketRecipe);
                w.nextRocket = 0.8f + rng.nextFloat() * 1.2f;
            }
            if (w.age > w.life) it = walkers.erase (it); else ++it;
        }

        for (auto it = rockets.begin(); it != rockets.end();)
        {
            auto& r = *it;
            r.trail.push_back (r.p);
            if (r.trail.size() > 14) r.trail.erase (r.trail.begin());
            r.v.y += 120.0f * dt;
            r.p += r.v * dt;
            if ((r.fuse -= dt) <= 0.0f)
            {
                for (int i = 0; i < 26; ++i)
                {
                    const float a = juce::MathConstants<float>::twoPi * (float) i / 26.0f;
                    const float sp = 70.0f + rng.nextFloat() * 110.0f;
                    sparks.push_back ({ r.p, { std::cos (a) * sp, std::sin (a) * sp },
                                        r.c.withRotatedHue (rng.nextFloat() * 0.15f), 1.0f });
                }
                processor.triggerSkillSound (r.recipe + rng.nextInt (5));   // the bang
                chamber.spawnMiniOrbs (3);
                it = rockets.erase (it);
            }
            else ++it;
        }
        if (sparks.size() > 900) sparks.erase (sparks.begin(), sparks.begin() + (long) (sparks.size() - 900));
        for (auto it = sparks.begin(); it != sparks.end();)
        {
            it->v.y += 90.0f * dt;
            it->v *= 0.985f;
            it->p += it->v * dt;
            it->life -= dt * 0.8f;
            if (it->life <= 0.0f) it = sparks.erase (it); else ++it;
        }

        // J: up to 100,000 dots pour out of the centre; each one that reaches a
        // wall dies and pays 23 points.
        if (dotsToSpawn > 0 || ! dots.empty())
        {
            const float w = (float) getWidth(), h = (float) getHeight();
            for (int i = 0; i < 1600 && dotsToSpawn > 0; ++i, --dotsToSpawn)
            {
                const float a = rng.nextFloat() * juce::MathConstants<float>::twoPi, sp = 180.0f + rng.nextFloat() * 520.0f;
                dots.push_back ({ w * 0.5f, h * 0.5f, std::cos (a) * sp, std::sin (a) * sp,
                                  juce::Colour::fromHSV (rng.nextFloat(), 0.9f, 1.0f, 1.0f).getARGB() });
            }
            int died = 0;
            for (auto& d : dots)
            {
                d.x += d.vx * dt; d.y += d.vy * dt;
                if (d.x < 0 || d.y < 0 || d.x >= w || d.y >= h) { d.vx = 0.0f; ++died; }
            }
            dots.erase (std::remove_if (dots.begin(), dots.end(), [] (const Dot& d) { return d.vx == 0.0f; }), dots.end());
            dotPointsPending += (juce::int64) died * 23;
            if (dotPointsPending > 0 && (dots.empty() || rng.nextInt (6) == 0))
            {
                if (onPoints) onPoints (dotPointsPending, {});
                dotPointsPending = 0;
            }
            if (died > 0 && rng.nextInt (8) == 0) processor.triggerSkillSound (63500 + rng.nextInt (40));
        }

        // T: the black swarm bounces off the walls until it fades.
        if (! swarm.empty())
        {
            const float w = (float) getWidth(), h = (float) getHeight();
            for (auto& d : swarm)
            {
                d.x += d.vx * dt; d.y += d.vy * dt;
                if (d.x < 0 || d.x >= w - 3) { d.vx = -d.vx; d.x = juce::jlimit (0.0f, w - 4.0f, d.x); }
                if (d.y < 0 || d.y >= h - 3) { d.vy = -d.vy; d.y = juce::jlimit (0.0f, h - 4.0f, d.y); }
            }
            if ((swarmLife -= dt) <= 0.0f) swarm.clear();
            else if (swarmLife < 3.0f) swarm.resize ((size_t) ((float) swarm.size() * 0.97f));
        }

        // H: nine huge white orbs zoom around, then pop one by one.
        for (auto it = bigOrbs.begin(); it != bigOrbs.end();)
        {
            it->p += it->v * dt;
            if (it->p.x < 40 || it->p.x > getWidth() - 40)  it->v.x = -it->v.x;
            if (it->p.y < 40 || it->p.y > getHeight() - 40) it->v.y = -it->v.y;
            if ((it->life -= dt) <= 0.0f)
            {
                for (int k = 0; k < 40; ++k)
                {
                    const float a = rng.nextFloat() * juce::MathConstants<float>::twoPi, sp = 80.0f + rng.nextFloat() * 240.0f;
                    sparks.push_back ({ it->p, { std::cos (a) * sp, std::sin (a) * sp }, juce::Colours::white, 1.0f });
                }
                processor.triggerSkillSound (63600 + (int) (it - bigOrbs.begin()) * 11);   // pop
                it = bigOrbs.erase (it);
            }
            else
            {
                if (rng.nextInt (40) == 0) processor.triggerSkillSound (63700 + rng.nextInt (9));   // little popping noises
                ++it;
            }
        }

        // K: the moon falls into the samples. Burps follow.
        if (moonAge >= 0.0f)
        {
            moonAge += dt;
            if (! moonLanded && moonAge > 2.4f)
            {
                moonLanded = true;
                const auto c = getLocalBounds().getCentre().toFloat();
                for (int k = 0; k < 120; ++k)
                {
                    const float a = rng.nextFloat() * juce::MathConstants<float>::twoPi, sp = 60.0f + rng.nextFloat() * 380.0f;
                    sparks.push_back ({ c, { std::cos (a) * sp, std::sin (a) * sp }, juce::Colour (0xffd8d2c0), 1.3f });
                }
                story.perform (story::Effect::radiate, { story::kNo, story::kNo, story::kNo, story::kNo }, 0);
                chamber.spawnMonsterOrbs (6);
            }
            if (moonLanded && rng.nextInt (25) == 0 && moonAge < 7.0f)
                processor.triggerHauntedSound (180 + rng.nextInt (20), 0.9f);   // burps: low, wet, short
            if (moonAge > 7.5f) moonAge = -1.0f;
        }

        for (auto it = groaners.begin(); it != groaners.end();)
        {
            it->heading += (rng.nextFloat() - 0.5f) * 0.6f;
            it->p += juce::Point<float> (std::cos (it->heading), std::sin (it->heading) - 0.6f) * (40.0f * dt);
            it->p.x = juce::jlimit (0.0f, (float) getWidth(), it->p.x);
            if (it->p.y < 0.0f) it->heading = juce::MathConstants<float>::halfPi;
            if (rng.nextInt (220) == 0) processor.triggerHauntedSound (90 + rng.nextInt (30), 1.0f);   // loud, low groan
            if ((it->life -= dt) <= 0.0f) it = groaners.erase (it); else ++it;
        }

        if (fateTitleAge < 6.0f) fateTitleAge += dt;
        if (roamer != nullptr && static_cast<Roamer*> (roamer.get())->finished()) roamer.reset();

        const bool busy = roomId >= 0 || polyglot >= 0 || ! walkers.empty() || ! rockets.empty() || ! sparks.empty()
                       || dotsToSpawn > 0 || ! dots.empty() || ! bigOrbs.empty() || moonAge >= 0.0f
                       || fateTitleAge < 6.0f || roamer != nullptr || ! groaners.empty() || ! swarm.empty();
        repaint();
        if (! busy) stopTimer();
    }

    // ---- drawing ------------------------------------------------------------------

    void SecretLayer::paint (juce::Graphics& g)
    {
        const auto b = getLocalBounds().toFloat();

        if (roomId >= 0)
        {
            const auto& s = secrets::all()[(std::size_t) roomId];
            const float in = juce::jmin (1.0f, roomAge / 0.5f), out = juce::jmin (1.0f, (roomLife - roomAge) / 0.8f);
            const float a = juce::jmax (0.0f, juce::jmin (in, out));
            const auto hue = rgb (s.hue);

            g.setGradientFill (juce::ColourGradient (hue.darker (2.2f).withAlpha (0.93f * a), b.getCentreX(), b.getY(),
                                                     theme::bg0.withAlpha (0.96f * a), b.getCentreX(), b.getBottom(), false));
            g.fillRoundedRectangle (b, 6.0f);

            // Each room has its own wallpaper.
            g.setColour (hue.withAlpha (0.18f * a));
            const float t = roomAge;
            switch (s.pattern)
            {
                case 0: for (float x = 0; x < b.getWidth(); x += 28) g.drawVerticalLine ((int) x, 0, b.getHeight());
                        for (float y = 0; y < b.getHeight(); y += 28) g.drawHorizontalLine ((int) y, 0, b.getWidth()); break;
                case 1: for (int i = 1; i < 14; ++i) { const float r = std::fmod (i * 40.0f + t * 60.0f, 560.0f);
                        g.drawEllipse (b.getCentreX() - r, b.getCentreY() - r, r * 2, r * 2, 1.5f); } break;
                case 2: for (int i = 0; i < 12; ++i) g.fillRect (b.getX() + i * b.getWidth() / 12.0f, b.getBottom() - (i + 1) * b.getHeight() / 14.0f,
                                                                 b.getWidth() / 12.0f - 3.0f, (i + 1) * b.getHeight() / 14.0f); break;
                case 3: for (int i = 0; i < 16; ++i) { const float h = (0.3f + 0.7f * std::abs (std::sin (t * 4.0f + i))) * b.getHeight() * 0.6f;
                        g.fillRect (b.getX() + i * b.getWidth() / 16.0f + 2, b.getBottom() - h, b.getWidth() / 16.0f - 4, h); } break;
                case 4: { juce::Random r ((juce::int64) roomId * 77 + (juce::int64) (t * 12));
                        for (int i = 0; i < 400; ++i) g.fillRect (r.nextFloat() * b.getWidth(), r.nextFloat() * b.getHeight(), 2.0f, 2.0f); } break;
                default: for (int i = 0; i < 24; ++i) { const float ang = i * juce::MathConstants<float>::twoPi / 24.0f + t * 0.3f;
                        g.drawLine (b.getCentreX(), b.getCentreY(), b.getCentreX() + std::cos (ang) * 900.0f,
                                    b.getCentreY() + std::sin (ang) * 900.0f, 2.0f); } break;
            }

            auto text = b.reduced (28.0f, 24.0f);
            g.setColour (hue.withAlpha (a));
            g.setFont (theme::monoFont (11.0f));
            g.drawText (juce::String ("SECRET ") + juce::String (roomId + 1) + "/39   " + secrets::tierName (s.tier),
                        text.removeFromTop (16.0f), juce::Justification::centredLeft);
            g.setFont (theme::uiFont (juce::jlimit (18.0f, 34.0f, b.getWidth() / 20.0f), true));
            g.setColour (juce::Colours::white.withAlpha (a));
            g.drawFittedText (juce::String (s.room).toUpperCase(), text.removeFromTop (44.0f).toNearestInt(),
                              juce::Justification::centredLeft, 1);
            g.setFont (theme::uiFont (13.0f, true));
            g.setColour (hue.brighter (0.4f).withAlpha (a));
            g.drawText (s.host, text.removeFromTop (20.0f), juce::Justification::centredLeft);
            g.setFont (theme::uiFont (14.0f));
            g.setColour (theme::text.withAlpha (a));
            g.drawFittedText (s.line, text.removeFromTop (60.0f).toNearestInt(), juce::Justification::topLeft, 3);
            g.setFont (theme::monoFont (12.0f));
            g.setColour (hue.withAlpha (a));
            g.drawText (juce::String ("SKILL LEARNED: ") + s.skillName + "    FOUND: " + s.name,
                        text.removeFromTop (20.0f), juce::Justification::centredLeft);

            // Odd visitors who turned up for the party.
            float vx = b.getX() + 30.0f;
            for (auto who : roomVisitors)
            {
                const auto& v = secrets::visitors()[(std::size_t) who];
                g.setColour (rgb (v.colour).withAlpha (a));
                g.setFont (theme::uiFont (16.0f, true));
                drawOdd (g, v.name, { vx, b.getBottom() - 40.0f, 110.0f, 22.0f }, v.glyphs, juce::Justification::centred);
                vx += 120.0f;
            }
        }

        // Visitors walking the floor, gibberish in a bubble.
        const float floorY = b.getBottom() - 30.0f;
        for (const auto& w : walkers)
        {
            const auto& v = secrets::visitors()[(std::size_t) w.who];
            const auto col = rgb (v.colour);
            const float bob = std::sin (w.age * 9.0f) * 3.0f;
            const juce::Rectangle<float> body (w.x - 14.0f, floorY - 30.0f + bob, 28.0f, 30.0f);
            g.setColour (col);
            g.fillEllipse (body);
            g.setColour (theme::bg0);
            g.fillEllipse (body.getX() + 7, body.getY() + 9, 4, 4);
            g.fillEllipse (body.getRight() - 11, body.getY() + 9, 4, 4);

            g.setColour (col);
            g.setFont (theme::uiFont (12.0f, true));
            drawOdd (g, v.name, { w.x - 50.0f, floorY + 2.0f, 100.0f, 16.0f }, v.glyphs, juce::Justification::centred);

            if (w.age > 0.8f && w.age < w.life - 2.0f)
            {
                juce::Rectangle<float> bubble (w.x - 90.0f, floorY - 78.0f + bob, 180.0f, 38.0f);
                bubble = bubble.withX (juce::jlimit (2.0f, juce::jmax (2.0f, b.getRight() - 182.0f), bubble.getX()));
                g.setColour (theme::text.withAlpha (0.92f));
                g.fillRoundedRectangle (bubble, 8.0f);
                g.setColour (theme::bg0);
                g.setFont (theme::uiFont (12.0f, true));
                drawOdd (g, w.speech, bubble.reduced (6.0f, 2.0f), v.glyphs, juce::Justification::centred);
            }
        }

        // Bottle rockets and their sparks.
        for (const auto& r : rockets)
        {
            for (size_t i = 1; i < r.trail.size(); ++i)
            {
                g.setColour (juce::Colours::orange.withAlpha ((float) i / (float) r.trail.size() * 0.8f));
                g.drawLine ({ r.trail[i - 1], r.trail[i] }, 2.0f);
            }
            g.setColour (r.c);
            g.fillRect (r.p.x - 2.0f, r.p.y - 6.0f, 4.0f, 10.0f);
        }
        for (const auto& s : sparks)
        {
            g.setColour (s.c.withAlpha (juce::jlimit (0.0f, 1.0f, s.life)));
            g.fillEllipse (s.p.x - 1.8f, s.p.y - 1.8f, 3.6f, 3.6f);
        }

        // J dots and the T swarm: drawn straight into a bitmap, there are far too many for paths.
        if (! dots.empty() || ! swarm.empty())
        {
            const int w = getWidth(), h = getHeight();
            if (dotImage.getWidth() != w || dotImage.getHeight() != h)
                dotImage = juce::Image (juce::Image::ARGB, juce::jmax (1, w), juce::jmax (1, h), true);
            dotImage.clear (dotImage.getBounds());
            {
                juce::Image::BitmapData px (dotImage, juce::Image::BitmapData::writeOnly);
                for (const auto& d : dots)
                {
                    const int x = (int) d.x, y = (int) d.y;
                    if (x >= 0 && y >= 0 && x < w - 1 && y < h - 1)
                    {
                        const auto c = juce::Colour (d.argb);
                        px.setPixelColour (x, y, c);
                        px.setPixelColour (x + 1, y, c);
                        px.setPixelColour (x, y + 1, c);
                    }
                }
                // Black orbs: a 3x3 black body with a pale rim so they read on the dark dish.
                const auto rim = juce::Colour (0xff6a7280), core = juce::Colour (0xff000000);
                for (const auto& d : swarm)
                {
                    const int x = (int) d.x, y = (int) d.y;
                    if (x < 1 || y < 1 || x >= w - 4 || y >= h - 4) continue;
                    px.setPixelColour (x, y - 1, rim);
                    px.setPixelColour (x - 1, y, rim);
                    for (int yy = 0; yy < 3; ++yy)
                        for (int xx = 0; xx < 3; ++xx)
                            px.setPixelColour (x + xx, y + yy, core);
                }
            }
            g.drawImageAt (dotImage, 0, 0);
        }

        for (const auto& m : groaners)
        {
            const auto col = juce::Colour (0xff30d5c8);
            g.setColour (col);
            for (int leg = -1; leg <= 1; ++leg)   // legs, wiggling
            {
                const float wig = std::sin (m.life * 18.0f + (float) leg) * 3.0f;
                g.drawLine (m.p.x - 7, m.p.y + leg * 3.0f, m.p.x + 7, m.p.y + leg * 3.0f + wig, 1.2f);
            }
            g.fillEllipse (m.p.x - 5, m.p.y - 4, 10, 8);
            g.setColour (theme::bg0);
            g.fillEllipse (m.p.x + 1, m.p.y - 2, 2, 2);
        }

        for (const auto& o : bigOrbs)
        {
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.fillEllipse (o.p.x - 52, o.p.y - 52, 104, 104);
            g.setColour (juce::Colours::white);
            g.fillEllipse (o.p.x - 36, o.p.y - 36, 72, 72);
        }

        if (moonAge >= 0.0f && ! moonLanded)
        {
            const float t = moonAge / 2.4f;
            const auto c = b.getTopLeft().translated (-80.0f, -80.0f) + (b.getCentre() - b.getTopLeft() + juce::Point<float> (80.0f, 80.0f)) * (t * t);
            const float r = 46.0f + 30.0f * t;
            g.setColour (juce::Colour (0xffd8d2c0));
            g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
            g.setColour (juce::Colour (0xffa8a290));
            g.fillEllipse (c.x - r * 0.4f, c.y - r * 0.3f, r * 0.35f, r * 0.35f);
            g.fillEllipse (c.x + r * 0.2f, c.y + r * 0.15f, r * 0.25f, r * 0.25f);
            g.fillEllipse (c.x - r * 0.1f, c.y + r * 0.45f, r * 0.18f, r * 0.18f);
        }

        if (fateTitleAge < 6.0f)
        {
            const float a = juce::jmin (1.0f, fateTitleAge / 0.3f, (6.0f - fateTitleAge) / 0.8f);
            g.setGradientFill (rainbow (b, std::fmod (fateTitleAge * 0.3f, 1.0f)));
            g.setOpacity (a);
            g.setFont (theme::uiFont (juce::jlimit (20.0f, 40.0f, b.getWidth() / 18.0f), true));
            g.drawText (fateTitle, b.withHeight (b.getHeight() * 0.3f), juce::Justification::centred, true);
            g.setOpacity (1.0f);
        }

        // The polyglot banner: in another language, in every colour, for no reason.
        if (polyglot >= 0)
        {
            const auto& p = secrets::polyglots()[(std::size_t) polyglot];
            const float a = juce::jmin (1.0f, polyAge / 0.4f, (5.0f - polyAge) / 0.6f);
            auto band = b.withSizeKeepingCentre (b.getWidth(), 92.0f).translated (0.0f, -b.getHeight() * 0.18f);
            g.setColour (theme::bg0.withAlpha (0.55f * a));
            g.fillRect (band);
            g.setGradientFill (rainbow (band, std::fmod (polyAge * 0.25f, 1.0f)));
            g.setOpacity (a);
            g.setFont (theme::uiFont (juce::jlimit (22.0f, 44.0f, b.getWidth() / 16.0f), true));
            g.drawText (utf8 (p.phrase), band.removeFromTop (60.0f), juce::Justification::centred, true);
            g.setOpacity (1.0f);
            g.setColour (theme::text.withAlpha (a));
            g.setFont (theme::monoFont (11.0f));
            g.drawText (juce::String (p.language).toUpperCase() + "  /  \"" + p.meaning + "\"",
                        band, juce::Justification::centred, true);
        }
    }
}
