#include "Colony.h"
#include <cmath>
#include <cstring>

namespace mutagen
{
    static float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    static float dist2 (float ax, float ay, float bx, float by)
    {
        const float dx = ax - bx, dy = ay - by;
        return dx * dx + dy * dy;
    }

    Colony::Colony()
    {
        for (auto& n : noteGroup) n = -1;
        baselineGenome.randomise (rng);
    }

    void Colony::prepare (double sampleRate, int maxBlock, int maxCells)
    {
        sr           = sampleRate;
        maxBlockSize = juce::jmax (32, maxBlock);
        maxCellCount = juce::jlimit (8, EngineSnapshot::maxCells, maxCells);

        // always allocate the absolute maximum so CPU-quality changes only move
        // a soft cap and never reallocate on the audio thread.
        cells.assign ((size_t) EngineSnapshot::maxCells, Cell {});
        for (auto& c : cells) { c.prepare (sr); c.setWorld (&world); c.alive = false; }

        excBuffer.setSize (2, maxBlockSize);
        workBuffer.setSize (2, maxBlockSize);
        analyser.prepare (sr);
        initNiches();

        if (src == nullptr)
        {
            src = std::make_unique<SourceMaterial>();
            SourceAnalyzer an;
            *src = an.makePrimitive (params::SourceMode::primitiveNoise, sr, 3.0f);
        }
        reset();
    }

    void Colony::reset()
    {
        for (auto& c : cells) { c.alive = false; c.stage = LifeStage::dead; c.prepare (sr); }
        generationCount = 0;
        tickCounter = 0;
        tickAccumSec = 0.0;
        geneTransferFlash = extinctionFlash = pressureFront = infectionField = 0.0f;
        for (auto& a : arcs) a = GeneArc {};
        for (auto& n : noteGroup) n = -1;
        noteActiveCount = 0;
        rng.seed (colonySeed);
        fitnessAccum = 0.0;
        archive.clear();
        elites.clear();
        stagnation.reset();
        analyser.reset();
        noiseLockSeconds = stuckSeconds = 0.0f;
        userIntent = 0.0f;
        for (auto& n : noveltyOf) n = 0.5f;
        noveltyCursor = 0;
        initNiches();
    }

    void Colony::setWorld (const WorldSeed& w)
    {
        world = w;
        // cells store a pointer into this object, so re-point rather than
        // assuming the vector never moved.
        for (auto& c : cells) c.setWorld (&world);

        // A new world means new rules; the old archive describes a different
        // instrument and would drag the new one toward the old one's sounds.
        archive.clear();
        elites.clear();
        stagnation.reset();
        initNiches();

        for (auto& c : cells)
            if (c.alive) c.mod.configure (c.genome, world, rng);
    }

    // ---------------------------------------------------------------------

    SourceMaterial* Colony::adoptSource (SourceMaterial* incoming)
    {
        if (incoming == nullptr) return nullptr;
        SourceMaterial* retired = src.release();
        src.reset (incoming);
        return retired;
    }

    int Colony::findFreeSlot()
    {
        for (int i = 0; i < (int) cells.size(); ++i)
            if (! cells[(size_t) i].alive) return i;

        // no free slot: recycle the weakest non-preserved cell
        int   worst = -1;
        float worstScore = 1.0e9f;
        for (int i = 0; i < (int) cells.size(); ++i)
        {
            const auto& c = cells[(size_t) i];
            if (c.preserved) continue;
            const float score = c.fitness * 0.5f + c.energy * 0.5f
                              + (c.stage == LifeStage::dying ? -1.0f : 0.0f);
            if (score < worstScore) { worstScore = score; worst = i; }
        }
        return worst;
    }

    void Colony::germinateFromSource (float initialPop, float dGrain, float dSpec, float dRes)
    {
        clearAll();
        rng.seed (colonySeed);
        generationCount = 1;
        nextFamilyId = 1;
        nextGroupId  = 1;

        // derive a baseline genome from the source features so "divergence"
        // has a meaningful origin.
        baselineGenome.randomise (rng);
        baselineGenome.set (Trait::brightness,  src->brightness);
        baselineGenome.set (Trait::noiseColour, src->noisiness);
        baselineGenome.set (Trait::decayShape,  src->decayRate);
        baselineGenome.set (Trait::formant,     clamp01 ((src->formantHz - 150.0f) / 4000.0f));
        baselineGenome.set (Trait::pitch,       clamp01 (std::log2 (src->fundamentalHz / 55.0f) / 6.0f));

        const float total = juce::jmax (0.01f, dGrain + dSpec + dRes);
        const float pGrain = dGrain / total;
        const float pSpec  = dSpec  / total;

        const int target = juce::jlimit (3, maxCellCount,
                             (int) std::round (initialPop * (float) maxCellCount * 0.9f) + 3);

        initNiches();

        /*  Founders come from the *islands*, not from one baseline genome.

            The old code built every founder as baselineGenome + mutate, and
            baselineGenome was derived deterministically from the source's
            spectral features - so two runs on the same sample started from the
            same point and, pulled by a shared attractor, ended at the same
            place. Now each founder is a mutated copy of one island's target,
            and the islands are independently randomised per run.            */
        for (int i = 0; i < target; ++i)
        {
            const int slot = findFreeSlot();
            if (slot < 0) break;

            // Species mix is per-world as well as per-parameter, so one world
            // is a grain colony and another is mostly resonators.
            const float wG = pGrain * (0.5f + world.speciesMix[0]);
            const float wS = pSpec  * (0.5f + world.speciesMix[1]);
            const float wR = (1.0f - pGrain - pSpec) * (0.5f + world.speciesMix[2]);
            const float tot = juce::jmax (1.0e-4f, wG + wS + wR);

            const float r = rng.nextFloat() * tot;
            Species sp = r < wG ? Species::grain
                       : r < wG + wS ? Species::spectral
                                     : Species::resonator;

            const int niche = rng.intRange (0, nicheCount);
            Genome g = niches[niche].target;
            g.mutate (rng, 0.75f, 0.45f, 0.05f);   // spread the founders out

            // the source still colours the founders, it just no longer defines
            // the single point they all start from
            if (src != nullptr && src->valid && rng.chance (0.5f))
            {
                g.set (Trait::brightness,
                       clamp01 (0.5f * g.get (Trait::brightness) + 0.5f * src->brightness));
                g.set (Trait::noiseColour,
                       clamp01 (juce::jmin (world.noiseCeiling,
                                            0.6f * g.get (Trait::noiseColour) + 0.4f * src->noisiness)));
            }

            // give each species a characteristic bias
            if (sp == Species::grain)     g.set (Trait::duration, rng.range (0.05f, 0.4f));
            if (sp == Species::spectral)  g.set (Trait::density,  rng.range (0.3f, 0.9f));
            if (sp == Species::resonator) g.set (Trait::resonance, rng.range (0.45f, 0.9f));
            g.enforceMovementFloor();

            cells[(size_t) slot].germinate (sp, g, nextFamilyId++, niche, 1, rng);
            cells[(size_t) slot].birthTick = tickCounter;
            cells[(size_t) slot].x = clamp01 (niches[niche].cx + rng.bipolar() * 0.12f);
            cells[(size_t) slot].y = clamp01 (niches[niche].cy + rng.bipolar() * 0.12f);
        }
        nextGroupId = juce::jmax (nextGroupId, nicheCount + 1);
    }

    void Colony::reseedRandom (float initialPop)
    {
        clearAll();
        generationCount = 1;
        baselineGenome.randomise (rng);
        const int target = juce::jlimit (3, maxCellCount,
                             (int) std::round (initialPop * (float) maxCellCount * 0.8f) + 3);
        initNiches();
        for (int i = 0; i < target; ++i)
        {
            const int slot = findFreeSlot();
            if (slot < 0) break;
            Genome g; g.randomise (rng);
            const int niche = rng.intRange (0, nicheCount);
            const Species sp = (Species) rng.intRange (0, numSpecies);
            cells[(size_t) slot].germinate (sp, g, nextFamilyId++, niche, 1, rng);
            cells[(size_t) slot].x = clamp01 (niches[niche].cx + rng.bipolar() * 0.14f);
            cells[(size_t) slot].y = clamp01 (niches[niche].cy + rng.bipolar() * 0.14f);
        }
        nextGroupId = juce::jmax (nextGroupId, nicheCount + 1);
    }

    void Colony::clearAll()
    {
        for (auto& c : cells) { c.alive = false; c.stage = LifeStage::dead; }
        for (auto& n : noteGroup) n = -1;
        noteActiveCount = 0;
    }

    // ---------------------------------------------------------------------

    int Colony::population() const
    {
        int p = 0;
        for (const auto& c : cells) if (c.alive) ++p;
        return p;
    }

    bool Colony::inScope (const Cell& c, ScopeLevel lvl, int id) const
    {
        switch (lvl)
        {
            case ScopeLevel::colony:  return true;
            case ScopeLevel::species: return (int) c.species == id;
            case ScopeLevel::family:  return c.familyId == id;
            case ScopeLevel::cell:    return (&c - cells.data()) == id;
        }
        return false;
    }

    Genome Colony::selectionTargetGenome() const
    {
        Genome t = baselineGenome;
        auto set = [&] (Trait tr, float v) { t.set (tr, clamp01 (v)); };
        set (Trait::brightness,  0.5f + 0.5f * env.selBrightness);
        set (Trait::density,     0.5f + 0.5f * env.selDensity);
        set (Trait::noiseColour, 0.5f - 0.5f * env.selHarmonicity);
        set (Trait::aggression,  0.5f + 0.5f * env.selAggression);
        // divergence: push mutability up when divergence is wanted
        set (Trait::mutability,  0.3f + 0.6f * env.selDivergence);
        return t;
    }

    // ---------------------------------------------------------------------
    //  Niches - the island model
    // ---------------------------------------------------------------------

    void Colony::initNiches()
    {
        nicheCount = juce::jlimit (2, maxNiches, world.nicheCount);

        for (int i = 0; i < nicheCount; ++i)
        {
            auto& n = niches[i];

            // Each island starts from its *own* randomised genome, not from a
            // perturbed copy of one baseline. That single change is most of
            // why two runs no longer converge on the same timbre.
            n.target.randomise (rng);

            // Give each island a distinct character along the axes the ear
            // actually separates, so they cannot all be the same sound.
            const float t = (float) i / (float) juce::jmax (1, nicheCount - 1);
            n.target.set (Trait::pitch,       clamp01 (0.2f + 0.6f * t + rng.bipolar() * 0.12f));
            n.target.set (Trait::brightness,  clamp01 (rng.range (0.1f, 0.9f)));
            n.target.set (Trait::density,     clamp01 (rng.range (0.1f, 0.9f)));
            n.target.set (Trait::noiseColour, clamp01 (rng.range (0.0f, world.noiseCeiling)));
            n.target.set (Trait::lfoRateCentre, clamp01 (rng.nextFloat()));
            n.target.set (Trait::lfoDepth,    clamp01 (rng.range (0.3f, 1.0f)));

            const float ang = juce::MathConstants<float>::twoPi * t + rng.bipolar() * 0.4f;
            const float rad = 0.18f + 0.22f * rng.nextFloat();
            n.cx = clamp01 (0.5f + std::cos (ang) * rad);
            n.cy = clamp01 (0.5f + std::sin (ang) * rad);
            n.hue = std::fmod (t + rng.nextFloat() * 0.15f, 1.0f);
            n.vitality = 1.0f;
        }
    }

    void Colony::updateNiches (double dt)
    {
        // Every island's target performs its own slow random walk. Nothing
        // pulls them toward each other, so the colony keeps several different
        // "correct answers" alive at once instead of one.
        const float step = (float) dt * world.nicheDrift * 0.06f;

        for (int i = 0; i < nicheCount; ++i)
        {
            auto& n = niches[i];
            for (int k = 0; k < numTraits; ++k)
            {
                auto& gene = n.target.raw()[k];
                if (gene.locked) continue;
                gene.value = clamp01 (gene.value + rng.gaussian() * step);
            }

            // Occasionally an island jumps somewhere new entirely. This is the
            // cheapest guard against every island slowly wandering into the
            // same region by chance.
            if (rng.chance ((float) dt * 0.012f))
            {
                const int trait = rng.intRange (0, numTraits);
                if (! n.target.raw()[trait].locked)
                    n.target.raw()[trait].value = rng.nextFloat();
                n.hue = std::fmod (n.hue + rng.range (0.1f, 0.4f), 1.0f);
            }

            n.target.enforceMovementFloor();

            // Islands drift around the chamber too, so the visualiser shows
            // populations physically separating.
            n.cx = clamp01 (n.cx + rng.bipolar() * step * 1.5f);
            n.cy = clamp01 (n.cy + rng.bipolar() * step * 1.5f);
        }
    }

    int Colony::nicheOf (const Cell& c) const
    {
        const int n = juce::jmax (1, nicheCount);
        const int id = c.speciesGroupId;
        return ((id % n) + n) % n;
    }

    // ---------------------------------------------------------------------
    //  Fitness: appeal x novelty x the user's wishes, divided by crowding
    // ---------------------------------------------------------------------

    float Colony::genomeAppeal (const Genome& g) const
    {
        /*  A cheap stand-in for "would this sound pleasant", evaluated in gene
            space so it can be applied to unborn children. The measured
            roughness of the colony's actual output (see noiseGuard) corrects
            this estimate at the colony level; here we only need something
            monotonic enough to steer mutation away from hiss and toward tone. */

        const float noise   = g.get (Trait::noiseColour);
        const float density = g.get (Trait::density);
        const float res     = g.get (Trait::resonance);
        const float bright  = g.get (Trait::brightness);

        // 1. Tonalness. The single biggest factor, and the one the brief cares
        //    about: noisy genomes are penalised hard above the world ceiling.
        float appeal = 1.0f - noise * 0.85f;
        if (noise > world.noiseCeiling)
            appeal -= (noise - world.noiseCeiling) * 2.0f;

        // 2. Roughness proxy. Dense *and* inharmonic *and* resonant is the
        //    combination that beats and buzzes; any one of them alone is fine.
        const bool inharmonicWorld = world.partials == PartialSet::golden
                                  || world.partials == PartialSet::stretched
                                  || world.partials == PartialSet::formantic;
        const float roughProxy = density * res * (inharmonicWorld ? 1.0f : 0.55f);
        appeal -= roughProxy * 0.45f;

        // 3. Brightness comfort - a gaussian around this world's preferred
        //    register, so neither mud nor shrillness scores well.
        const float db = (bright - world.brightBias) / 0.34f;
        appeal += 0.30f * std::exp (-db * db);

        // 4. Movement is pleasant. A still sound is not appealing however
        //    consonant it is, which keeps this term from fighting novelty.
        appeal += 0.18f * g.get (Trait::lfoDepth);

        return clamp01 (appeal * 0.75f);
    }

    Behaviour Colony::colonyMeanBehaviour() const
    {
        Behaviour m;
        float acc[Behaviour::dims] = {};
        int n = 0;
        for (const auto& c : cells)
        {
            if (! c.alive) continue;
            const auto b = Behaviour::fromGenome (c.genome);
            for (int i = 0; i < Behaviour::dims; ++i) acc[i] += b.at (i);
            ++n;
        }
        if (n == 0) return m;
        m.brightness = acc[0] / (float) n;
        m.density    = acc[1] / (float) n;
        m.noise      = acc[2] / (float) n;
        m.pitch      = acc[3] / (float) n;
        m.movement   = acc[4] / (float) n;
        m.attack     = acc[5] / (float) n;
        return m;
    }

    void Colony::evaluateFitness (double dt)
    {
        const auto& D = analyser.current();

        // ---- 1. behaviours for every living cell -------------------------
        int living[EngineSnapshot::maxCells];
        int liveCount = 0;
        for (int i = 0; i < (int) cells.size() && liveCount < EngineSnapshot::maxCells; ++i)
        {
            if (! cells[(size_t) i].alive) continue;
            behaviourOf[liveCount] = Behaviour::fromGenome (cells[(size_t) i].genome);
            living[liveCount] = i;
            ++liveCount;
        }
        if (liveCount == 0) { lastAvgFitness = 0.5f; return; }

        // ---- 2. novelty, amortised ----------------------------------------
        // The k-NN query is the only non-trivial cost here, so only a handful
        // of cells are re-scored per tick and the rest keep their last value.
        const int perTick = juce::jlimit (2, 8, liveCount / 6 + 2);
        for (int n = 0; n < perTick; ++n)
        {
            const int idx = noveltyCursor % liveCount;
            noveltyCursor = (noveltyCursor + 1) % juce::jmax (1, liveCount);
            noveltyOf[idx] = archive.noveltyOf (behaviourOf[idx]);
        }

        // ---- 3. how badly does the colony need to be pushed toward beauty --
        // When the measured output is already appealing, novelty is allowed to
        // dominate and the colony explores. When it has drifted toward noise,
        // the appeal weight rises and pulls it back. That is the whole
        // "gravitate toward appealing sounds unless the input is negative"
        // requirement, expressed as one sliding weight.
        const float distress = clamp01 (1.0f - D.appeal) * clamp01 (D.flatness * 1.4f);
        float wAppeal  = world.wAppeal  + 0.55f * distress;
        float wNovelty = world.wNovelty * (1.0f - 0.5f * distress);
        float wUser    = world.wUser;

        // Negative user intent inverts the appeal term: if the player is
        // actively damaging the sound, ugliness is what they asked for.
        if (userIntent < 0.0f) wAppeal *= (1.0f + userIntent);   // userIntent in -1..0

        const float wTot = wAppeal + wNovelty + wUser + 1.0e-6f;
        wAppeal /= wTot; wNovelty /= wTot; wUser /= wTot;

        // ---- 4. score every cell -------------------------------------------
        float sum = 0.0f, novSum = 0.0f, appSum = 0.0f;

        for (int a = 0; a < liveCount; ++a)
        {
            Cell& c = cells[(size_t) living[a]];
            const Behaviour& b = behaviourOf[a];

            const float appeal  = genomeAppeal (c.genome);
            const float novelty = noveltyOf[a];

            // --- fitness sharing -------------------------------------------
            // Count how many neighbours share this cell's patch of behaviour
            // space. A crowd divides its worth; an empty niche keeps all of
            // it. This is what actively *punishes* the colony for converging.
            float crowd = 0.0f;
            for (int j = 0; j < liveCount; ++j)
            {
                if (j == a) continue;
                const float d = b.distanceTo (behaviourOf[j]);
                if (d < 0.18f) crowd += 1.0f - d / 0.18f;    // triangular kernel
            }
            const float share = 1.0f / (1.0f + world.crowdingPenalty * crowd * 0.5f);

            // --- the user's wish --------------------------------------------
            float user = 0.5f;
            if (std::fabs (userIntent) > 0.01f)
            {
                const float dx = c.x - userX, dy = c.y - userY;
                const float dist = std::sqrt (dx * dx + dy * dy);
                const float reach = dist < userRadius ? 1.0f - dist / userRadius : 0.0f;
                user = clamp01 (0.5f + userIntent * reach * 0.5f);
            }

            // --- weak pull toward this cell's own island --------------------
            // Deliberately weak and deliberately per-niche. The old code
            // applied a strong pull toward one colony-wide target, which is
            // precisely the force that flattened every run into one sound.
            const int   ni = nicheOf (c);
            const float toNiche = 1.0f - c.genome.distanceTo (niches[ni].target);

            float fit = wAppeal * appeal
                      + wNovelty * novelty
                      + wUser * user;
            fit = fit * 0.82f + 0.18f * toNiche;
            fit *= share;

            // Cells the player asked to keep are always fit enough to breed.
            if (c.preserved) fit = juce::jmax (fit, 0.75f);

            c.fitness = clamp01 (0.08f + 0.92f * fit);
            sum += c.fitness;
            novSum += novelty;
            appSum += appeal;
        }

        lastAvgFitness = sum / (float) liveCount;
        lastNovelty    = novSum / (float) liveCount;
        lastAppeal     = appSum / (float) liveCount;

        // ---- 5. archive maintenance ----------------------------------------
        // Offer a couple of cells per tick to the novelty archive and the
        // MAP-Elites grid. Only genuinely new behaviours are stored, otherwise
        // a static colony would fill the archive with copies of itself and
        // novelty would read as zero for everything, including new arrivals.
        for (int n = 0; n < 2; ++n)
        {
            const int a = rng.intRange (0, liveCount);
            const Cell& c = cells[(size_t) living[a]];
            if (archive.addIfNovel (behaviourOf[a]))
            {
                const float quality = genomeAppeal (c.genome) * 0.6f + noveltyOf[a] * 0.4f;
                elites.consider (c.genome, behaviourOf[a], quality, (int) c.species);
            }
            else
            {
                const float quality = genomeAppeal (c.genome) * 0.6f + noveltyOf[a] * 0.4f;
                elites.consider (c.genome, behaviourOf[a], quality, (int) c.species);
            }
        }

        stagnation.update (lastDiversity, lastNovelty, (float) dt);
    }

    // ---------------------------------------------------------------------
    //  Rescue operations
    // ---------------------------------------------------------------------

    void Colony::injectElite (bool preferDistant)
    {
        const int slot = findFreeSlot();
        if (slot < 0) return;

        const EliteGrid::Elite* e = nullptr;
        if (preferDistant)
        {
            // Re-seed from the *furthest* stored behaviour rather than a
            // random one: a rescue should move the colony somewhere new, not
            // put back something it already sounds like.
            e = elites.mostDistantElite (colonyMeanBehaviour());
        }
        if (e == nullptr) e = elites.randomElite (rng);

        Genome g;
        if (e != nullptr)
        {
            g = e->genome;
            g.mutate (rng, 0.35f, 0.4f, 0.0f);
        }
        else
        {
            // Nothing archived yet - a fresh random genome, and crucially NOT
            // blended back toward the baseline the way the old immigrant code
            // did. Blending an immigrant 40% toward the thing you are trying
            // to escape makes the anti-convergence mechanism converge too.
            g.randomise (rng);
        }
        g.enforceMovementFloor();

        const Species sp = e != nullptr ? (Species) juce::jlimit (0, numSpecies - 1, e->species)
                                        : (Species) rng.intRange (0, numSpecies);

        const int niche = rng.intRange (0, nicheCount);
        cells[(size_t) slot].germinate (sp, g, nextFamilyId++, niche, generationCount, rng);
        cells[(size_t) slot].birthTick = tickCounter;
        cells[(size_t) slot].visualPulse = 1.0f;
        cells[(size_t) slot].x = clamp01 (niches[niche].cx + rng.bipolar() * 0.1f);
        cells[(size_t) slot].y = clamp01 (niches[niche].cy + rng.bipolar() * 0.1f);
        geneTransferFlash = juce::jmax (geneTransferFlash, 0.7f);
    }

    void Colony::mutationStorm()
    {
        /*  Hypermutation. When novelty and diversity have both flat-lined the
            population is sitting in a local optimum that ordinary mutation
            rates cannot climb out of, so we temporarily stop being gentle.  */
        for (auto& c : cells)
        {
            if (! c.alive || c.preserved) continue;
            c.genome.mutate (rng, 0.85f, 0.75f, 0.35f);
            c.visualPulse = 1.0f;

            // Scatter a third of the colony into different islands as well -
            // moving genes between demes is what island models do to escape.
            if (rng.chance (0.33f))
                c.speciesGroupId = rng.intRange (0, nicheCount);

            c.mod.configure (c.genome, world, rng);
        }

        // And nudge the islands themselves, so the storm has somewhere to go.
        for (int i = 0; i < nicheCount; ++i)
        {
            niches[i].target.mutate (rng, 0.5f, 0.6f, 0.2f);
            niches[i].hue = std::fmod (niches[i].hue + rng.range (0.15f, 0.5f), 1.0f);
        }

        injectElite (true);
        stagnation.beginStorm();
        pressureFront = 1.0f;
        geneTransferFlash = 1.0f;
    }

    // ---------------------------------------------------------------------
    //  Homeostasis: the two failure modes the brief rules out
    // ---------------------------------------------------------------------

    void Colony::noiseGuard (double dt)
    {
        const auto& D = analyser.current();

        if (D.noiseLocked) noiseLockSeconds += (float) dt;
        else               noiseLockSeconds = juce::jmax (0.0f, noiseLockSeconds - (float) dt * 2.0f);

        /*  Deliberately slow to engage.

            The player is supposed to notice the colour draining, notice the
            score has stopped, and fix it themselves by removing elements. So
            for the first several seconds of a noise lock the engine does
            nothing at all. Only if the player ignores it does the guard step
            in, and even then it works gradually - it is a backstop against a
            colony that can never recover, not an autopilot.                 */
        if (noiseLockSeconds < 8.0f) return;

        const float strength = clamp01 ((noiseLockSeconds - 8.0f) / 10.0f);

        // 1. Pull the survivors toward tone, hardest on the worst offenders.
        for (auto& c : cells)
        {
            if (! c.alive || c.preserved) continue;
            auto& noiseGene = c.genome.raw()[(int) Trait::noiseColour];
            if (! noiseGene.locked)
                noiseGene.value = clamp01 (noiseGene.value
                                           - (float) dt * strength * 0.25f * (0.3f + noiseGene.value));

            auto& densGene = c.genome.raw()[(int) Trait::density];
            if (! densGene.locked && densGene.value > 0.65f)
                densGene.value = clamp01 (densGene.value - (float) dt * strength * 0.12f);
        }

        // 2. Cull the noisiest cell now and then. Removing an element is
        //    exactly what we are asking the player to do, so the automatic
        //    version does the same thing rather than something cleverer.
        if (rng.chance ((float) dt * strength * 1.2f))
        {
            int worst = -1;
            float worstScore = -1.0f;
            for (int i = 0; i < (int) cells.size(); ++i)
            {
                const auto& c = cells[(size_t) i];
                if (! c.alive || c.preserved) continue;
                const float s = c.genome.get (Trait::noiseColour) * 0.7f
                              + c.genome.get (Trait::density) * 0.3f;
                if (s > worstScore) { worstScore = s; worst = i; }
            }
            if (worst >= 0) { killSlot (worst, true); extinctionFlash = 1.0f; }
        }
    }

    void Colony::boredomDrive (double dt)
    {
        const auto& D = analyser.current();

        if (D.stuck) stuckSeconds += (float) dt;
        else         stuckSeconds = juce::jmax (0.0f, stuckSeconds - (float) dt * 3.0f);

        // "The sound should always be varying to some degree and always
        // evolving." A colony that has genuinely stopped moving gets pushed,
        // and unlike the noise guard this one engages quickly, because a drone
        // that never changes is boring immediately rather than eventually.
        if (stuckSeconds < 3.0f) return;

        const float urgency = clamp01 ((stuckSeconds - 3.0f) / 6.0f);

        for (auto& c : cells)
        {
            if (! c.alive || c.preserved) continue;
            if (! rng.chance ((float) dt * urgency * 1.5f)) continue;

            // Widen the modulation rather than changing the timbre: the sound
            // the player built is preserved, it just starts moving again.
            auto bump = [&] (Trait t, float amount)
            {
                auto& gene = c.genome.raw()[(int) t];
                if (! gene.locked)
                    gene.value = clamp01 (gene.value + amount);
            };
            bump (Trait::lfoDepth,      rng.range (0.05f, 0.25f));
            bump (Trait::lfoRateSpread, rng.range (0.05f, 0.2f));
            bump (Trait::drift,         rng.range (0.02f, 0.15f));
            c.mod.configure (c.genome, world, rng);
            c.visualPulse = 1.0f;
        }

        if (stuckSeconds > 8.0f)
        {
            injectElite (true);
            stuckSeconds = 0.0f;
        }
    }

    // ---------------------------------------------------------------------
    //  User interaction
    // ---------------------------------------------------------------------

    void Colony::userPressure (float nx, float ny, float radius, float amount)
    {
        userX = clamp01 (nx);
        userY = clamp01 (ny);
        userRadius = juce::jlimit (0.05f, 1.2f, radius);
        userIntent = juce::jlimit (-1.0f, 1.0f, userIntent * 0.5f + amount);
    }

    void Colony::mutateAt (float nx, float ny, float radius, float strength)
    {
        userPressure (nx, ny, radius, juce::jlimit (0.0f, 1.0f, strength));
        strength = juce::jlimit (0.0f, 1.0f, strength);

        int touched = 0;
        for (int i = 0; i < (int) cells.size(); ++i)
        {
            auto& c = cells[(size_t) i];
            if (! c.alive) continue;

            const float dx = c.x - nx, dy = c.y - ny;
            const float dist = std::sqrt (dx * dx + dy * dy);
            if (dist > radius) continue;

            // Falls off from the centre, so a click has an obvious epicentre.
            const float reach = 1.0f - dist / radius;
            const float amt = strength * reach;

            c.genome.mutate (rng, 0.35f + 0.55f * amt, 0.3f + 0.6f * amt, 0.1f * amt);
            c.genome.enforceMovementFloor();
            c.mod.configure (c.genome, world, rng);
            c.visualPulse = 1.0f;
            c.energy = clamp01 (c.energy + amt * 0.25f);
            ++touched;

            // Push the cell physically away from the click, so the wave the
            // player sees and the mutation they cause are the same event.
            if (dist > 1.0e-4f)
            {
                c.x = clamp01 (c.x + dx / dist * amt * 0.06f);
                c.y = clamp01 (c.y + dy / dist * amt * 0.06f);
            }
        }

        // A strong click also seeds something new at the epicentre.
        if (strength > 0.55f && rng.chance (0.5f + 0.5f * strength))
        {
            const int slot = findFreeSlot();
            if (slot >= 0)
            {
                Genome g;
                const auto* e = elites.randomElite (rng);
                if (e != nullptr && rng.chance (0.5f)) g = e->genome;
                else                                   g.randomise (rng);
                g.mutate (rng, 0.5f, 0.5f, 0.1f);
                g.enforceMovementFloor();

                const int niche = rng.intRange (0, nicheCount);
                cells[(size_t) slot].germinate ((Species) rng.intRange (0, numSpecies),
                                                g, nextFamilyId++, niche, generationCount, rng);
                cells[(size_t) slot].x = clamp01 (nx + rng.bipolar() * 0.04f);
                cells[(size_t) slot].y = clamp01 (ny + rng.bipolar() * 0.04f);
                cells[(size_t) slot].birthTick = tickCounter;
                cells[(size_t) slot].visualPulse = 1.0f;
            }
        }

        if (touched > 0) pressureFront = 1.0f;
    }

    void Colony::subtractAt (float nx, float ny, float radius, float strength)
    {
        userPressure (nx, ny, radius, -juce::jlimit (0.0f, 1.0f, strength));
        strength = juce::jlimit (0.0f, 1.0f, strength);

        /*  The destructive half of the interaction. Where mutateAt adds and
            complicates, this strips: it takes partials away, quietens,
            simplifies and ultimately kills. It is also the player's cure for a
            noise lock, so it must reliably reduce the things that make a
            colony noisy - density and noise colour first, life last.        */

        for (int i = 0; i < (int) cells.size(); ++i)
        {
            auto& c = cells[(size_t) i];
            if (! c.alive || c.preserved) continue;

            const float dx = c.x - nx, dy = c.y - ny;
            const float dist = std::sqrt (dx * dx + dy * dy);
            if (dist > radius) continue;

            const float reach = 1.0f - dist / radius;
            const float amt = strength * reach;

            auto strip = [&] (Trait t, float by)
            {
                auto& gene = c.genome.raw()[(int) t];
                if (! gene.locked) gene.value = clamp01 (gene.value - by);
            };

            strip (Trait::noiseColour, amt * 0.55f);      // the noise cure
            strip (Trait::density,     amt * 0.40f);
            strip (Trait::resonance,   amt * 0.22f);
            strip (Trait::aggression,  amt * 0.30f);

            c.energy = clamp01 (c.energy - amt * 0.45f);
            c.health = clamp01 (c.health - amt * 0.35f);
            c.visualPulse = 1.0f;

            // Enough damage kills. Weak cells die first, which is what makes
            // repeated right-clicking feel like carving rather than deleting.
            if (c.energy < 0.05f || rng.chance (amt * 0.35f))
                killSlot (i, true);
            else
                c.mod.configure (c.genome, world, rng);
        }

        extinctionFlash = juce::jmax (extinctionFlash, 0.5f + 0.5f * strength);
    }

    float Colony::genomeVariance() const
    {
        float mean[numTraits] = {};
        int n = 0;
        for (const auto& c : cells)
        {
            if (! c.alive) continue;
            for (int i = 0; i < numTraits; ++i) mean[i] += c.genome.raw()[i].value;
            ++n;
        }
        if (n < 2) return 0.0f;
        for (auto& m : mean) m /= (float) n;
        float var = 0.0f;
        for (const auto& c : cells)
        {
            if (! c.alive) continue;
            for (int i = 0; i < numTraits; ++i)
            {
                const float d = c.genome.raw()[i].value - mean[i];
                var += d * d;
            }
        }
        return clamp01 (std::sqrt (var / (float) (n * numTraits)) * 2.5f);
    }

    Cell* Colony::spawnChild (const Cell& parent)
    {
        const int slot = findFreeSlot();
        if (slot < 0) return nullptr;

        Cell& child = cells[(size_t) slot];

        Genome g = parent.genome;

        // maybe recombine with a symbiotic partner
        if (parent.linkTo >= 0 && parent.linkTo < (int) cells.size()
            && cells[(size_t) parent.linkTo].alive && rng.chance (0.5f))
        {
            g = Genome::recombine (parent.genome, cells[(size_t) parent.linkTo].genome, rng);
        }

        const float rate  = clamp01 (env.mutation * (0.4f + 0.6f * parent.genome.get (Trait::mutability)));
        const float depth = clamp01 (env.mutationDepth + 0.4f * env.temperature * (1.0f - env.stability));
        g.mutate (rng, rate, depth, env.radiation);

        // temperature = constant micro-jitter regardless of the mutation dice
        if (env.temperature > 0.01f)
            for (auto& gene : g.raw())
                if (! gene.locked)
                    gene.value = clamp01 (gene.value + rng.bipolar()
                                          * env.temperature * (1.0f - env.stability) * 0.03f);

        g.enforceMovementFloor();

        // Migration between islands: rare, and the whole point of an island
        // model. A child occasionally founds a lineage in a different niche.
        int childNiche = parent.speciesGroupId;
        if (rng.chance (world.migrationRate * 0.08f))
            childNiche = rng.intRange (0, nicheCount);

        child.germinate (parent.species, g, parent.familyId, childNiche,
                         parent.generation + 1, rng);
        child.voiceGroup = parent.voiceGroup;
        child.x = clamp01 (parent.x + rng.bipolar() * 0.06f);
        child.y = clamp01 (parent.y + rng.bipolar() * 0.06f);
        child.birthTick = tickCounter;
        child.visualPulse = 1.0f;

        // infections can be inherited
        if (parent.infection != Infection::none && rng.chance (0.6f))
        {
            child.infection = parent.infection;
            child.infectionLoad = parent.infectionLoad * 0.5f;
        }

        pushArc (parent.x, parent.y, child.x, child.y, 0 /*divide*/);
        return &child;
    }

    void Colony::killSlot (int slot, bool violent)
    {
        if (slot < 0 || slot >= (int) cells.size()) return;
        auto& c = cells[(size_t) slot];
        if (! c.alive || c.preserved) return;
        c.stage = LifeStage::dying;
        if (violent) { c.energy *= 0.2f; c.health = 0.0f; extinctionFlash = 1.0f; }
    }

    void Colony::pushArc (float x1, float y1, float x2, float y2, int kind)
    {
        arcs[arcHead] = GeneArc { x1, y1, x2, y2, 1.0f, kind };
        arcHead = (arcHead + 1) % maxArcs;
    }

    // ---------------------------------------------------------------------

    void Colony::ecologyTick (double dt)
    {
        const float stress = env.stress();
        // The world's own tempo multiplies the metabolism control, so one run
        // lives fast and another unfolds over minutes from the same settings.
        const float metabRate = (0.3f + 1.7f * env.metabolism) * world.lifeTempo;
        const double lifeDt = dt * metabRate;

        // 1) lifecycle for every cell
        int alive = 0, dyingThisTick = 0;
        for (auto& c : cells)
        {
            if (! c.alive) continue;
            const bool wasAlive = c.stage != LifeStage::dead;
            c.updateLifecycle (lifeDt, stress, env.nutrients, env.metabolism, env.lifespan, rng);
            if (! c.alive && wasAlive) { ++dyingThisTick; }
            if (c.alive) ++alive;
        }
        if (dyingThisTick > 0) extinctionFlash = juce::jmax (extinctionFlash, 0.3f + 0.1f * (float) dyingThisTick);

        // 2) islands wander, then fitness is scored against them
        updateNiches (dt);

        fitnessAccum += dt;
        if (fitnessAccum >= fitnessInterval)
        {
            evaluateFitness (fitnessAccum);
            fitnessAccum = 0.0;
        }

        if (! env.explore)  // Preserve mode: freeze everything below
        {
            lastDiversity = genomeVariance();
            geneTransferFlash *= std::exp (-(float) dt * 2.0f);
            extinctionFlash   *= std::exp (-(float) dt * 2.0f);
            pressureFront      *= std::exp (-(float) dt * 1.5f);
            infectionField     *= std::exp (-(float) dt * 1.0f);
            return;
        }

        const int pop = alive;
        const float capacity = 0.15f + 0.85f * env.nutrients;   // fraction of maxCellCount
        const int   softCap  = juce::jmax (3, (int) (capacity * (float) maxCellCount));

        // 3) reproduction
        const float fertilityGate = env.fertility * env.nutrients;
        for (int i = 0; i < (int) cells.size(); ++i)
        {
            auto& c = cells[(size_t) i];
            if (! c.alive) continue;
            if (c.stage != LifeStage::reproducing && c.stage != LifeStage::mature) continue;
            if (pop >= maxCellCount) break;

            float pReproduce = (float) dt * 0.9f
                             * fertilityGate
                             * c.genome.get (Trait::reproRate)
                             * (0.4f + 1.2f * c.fitness)
                             * (c.stage == LifeStage::reproducing ? 1.0f : 0.35f);

            if (pop > softCap) pReproduce *= 0.15f;   // crowded: breeding slows

            if (rng.chance (pReproduce))
            {
                if (auto* child = spawnChild (c))
                {
                    c.energy *= 0.6f;               // reproduction costs energy
                    c.visualPulse = 1.0f;
                    juce::ignoreUnused (child);
                }
            }
        }

        // 4) competition & apoptosis culling
        for (int i = 0; i < (int) cells.size(); ++i)
        {
            auto& c = cells[(size_t) i];
            if (! c.alive || c.preserved) continue;

            // programmed death pressure
            if (rng.chance ((float) dt * env.apoptosis * 0.5f * (1.2f - c.fitness)))
                killSlot (i, false);

            // pairwise competition for frequency territory (near pitch gene)
            if (env.competition > 0.01f && rng.chance ((float) dt * env.competition * 2.0f))
            {
                const int j = rng.intRange (0, (int) cells.size());
                if (j != i && cells[(size_t) j].alive && ! cells[(size_t) j].preserved)
                {
                    auto& o = cells[(size_t) j];
                    const float pitchGap = std::abs (c.genome.get (Trait::pitch) - o.genome.get (Trait::pitch));
                    if (pitchGap < 0.08f)   // same territory -> the fitter one wins energy
                    {
                        Cell& winner = c.fitness >= o.fitness ? c : o;
                        Cell& loser  = c.fitness >= o.fitness ? o : c;
                        const float bite = env.competition * 0.2f
                                         * (0.5f + 0.5f * winner.genome.get (Trait::aggression));
                        loser.energy  = clamp01 (loser.energy - bite);
                        winner.energy = clamp01 (winner.energy + bite * 0.5f);
                        if (loser.energy < 0.02f) killSlot ((int) (&loser - cells.data()), true);
                    }
                }
            }
        }

        // hard population cap: cull weakest
        {
            int p = population();
            while (p > maxCellCount)
            {
                int worst = -1; float ws = 1.0e9f;
                for (int i = 0; i < (int) cells.size(); ++i)
                {
                    auto& c = cells[(size_t) i];
                    if (! c.alive || c.preserved) continue;
                    const float s = c.fitness + c.energy;
                    if (s < ws) { ws = s; worst = i; }
                }
                if (worst < 0) break;
                cells[(size_t) worst].kill();
                --p;
            }
        }

        // 5) symbiosis & cross-species gene transfer
        if (env.symbiosis > 0.01f && rng.chance ((float) dt * env.symbiosis * 3.0f))
        {
            const int a = rng.intRange (0, (int) cells.size());
            const int b = rng.intRange (0, (int) cells.size());
            if (a != b && cells[(size_t) a].alive && cells[(size_t) b].alive)
            {
                auto& ca = cells[(size_t) a];
                auto& cb = cells[(size_t) b];
                if (dist2 (ca.x, ca.y, cb.x, cb.y) < 0.05f
                    && (ca.genome.get (Trait::symbiosisAffinity) + cb.genome.get (Trait::symbiosisAffinity)) > 0.7f)
                {
                    ca.linkTo = b; cb.linkTo = a;
                    // transfer one random unlocked trait donor->recipient
                    const int trait = rng.intRange (0, numTraits);
                    if (! cb.genome.raw()[trait].locked)
                    {
                        const float donor = ca.genome.raw()[trait].value;
                        cb.genome.raw()[trait].value =
                            clamp01 (0.5f * cb.genome.raw()[trait].value + 0.5f * donor);
                        cb.visualPulse = 1.0f; ca.visualPulse = 1.0f;
                        geneTransferFlash = 1.0f;
                        pushArc (ca.x, ca.y, cb.x, cb.y, 1 /*gene transfer*/);
                    }
                }
            }
        }

        // 6) infection spread
        infectionField *= std::exp (-(float) dt * 0.6f);
        for (int i = 0; i < (int) cells.size(); ++i)
        {
            auto& c = cells[(size_t) i];
            if (! c.alive || c.infection == Infection::none || c.infectionLoad < 0.2f) continue;
            infectionField = juce::jmax (infectionField, 0.6f);
            if (rng.chance ((float) dt * (0.4f + 1.6f * c.infectionLoad)))
            {
                const int j = rng.intRange (0, (int) cells.size());
                auto& o = cells[(size_t) j];
                if (j != i && o.alive && o.infection == Infection::none && ! o.preserved
                    && dist2 (c.x, c.y, o.x, o.y) < 0.06f
                    && rng.chance (1.0f - o.genome.get (Trait::infectionResist)))
                {
                    o.infection = c.infection;
                    o.infectionLoad = 0.1f;
                    o.visualPulse = 1.0f;
                    pushArc (c.x, c.y, o.x, o.y, 2 /*infection*/);
                }
            }
        }

        // 7) migration
        if (env.migration > 0.01f && rng.chance ((float) dt * env.migration * 2.0f))
        {
            const int i = rng.intRange (0, (int) cells.size());
            auto& c = cells[(size_t) i];
            if (c.alive && ! c.preserved)
            {
                c.genome.set (Trait::spatialPos, rng.nextFloat());
                c.x = c.genome.get (Trait::spatialPos);
                if (rng.chance (0.2f)) c.speciesGroupId = rng.intRange (1, nextGroupId + 1);
            }
        }

        // 8) selection drift - toward each cell's OWN island, never toward
        //    one colony-wide optimum. The old code pulled every cell toward a
        //    single target genome every tick, which is the force that made
        //    every run converge on the same timbre regardless of its seed.
        if (env.selection > 0.01f)
        {
            // Capped well below the old 0.25 coefficient: selection now nudges,
            // it does not herd. Novelty and fitness sharing do the steering.
            const float amt = (float) dt * env.selection * 0.11f;
            for (auto& c : cells)
            {
                if (! c.alive || c.preserved) continue;
                const Genome& tgt = niches[nicheOf (c)].target;
                // fitter cells drift less; unfit cells move toward their island
                c.genome.driftToward (tgt, amt * (1.2f - c.fitness));
            }
            pressureFront = 1.0f;
        }

        // 8b) dispersal - a small outward push away from the colony mean.
        //     Selection is an attractive force; without a repulsive one the
        //     population still slowly balls up. This is the repulsive one.
        if (world.crowdingPenalty > 0.01f && rng.chance ((float) dt * 3.0f))
        {
            const Behaviour mean = colonyMeanBehaviour();
            const int i = rng.intRange (0, (int) cells.size());
            auto& c = cells[(size_t) i];
            if (c.alive && ! c.preserved)
            {
                const Behaviour b = Behaviour::fromGenome (c.genome);
                if (b.distanceTo (mean) < 0.12f)
                {
                    // too close to the average - shove one trait somewhere new
                    const int trait = rng.intRange (0, numTraits);
                    auto& gene = c.genome.raw()[trait];
                    if (! gene.locked)
                    {
                        gene.value = clamp01 (gene.value + rng.bipolar() * 0.35f
                                              * world.crowdingPenalty);
                        c.mod.configure (c.genome, world, rng);
                        c.visualPulse = 0.6f;
                    }
                }
            }
        }

        // 9) diversity maintenance: re-seed from the MAP-Elites archive.
        //    The old version blended the immigrant 60/40 back toward the
        //    baseline genome - which meant the mechanism meant to preserve
        //    diversity was itself pulling the colony home. Elites are stored
        //    per behaviour bin, so re-seeding from the most *distant* one
        //    moves the colony somewhere it has not been recently.
        lastDiversity = genomeVariance();
        if (lastDiversity < env.diversity * 0.4f && population() < maxCellCount
            && rng.chance ((float) dt * 0.5f))
        {
            injectElite (true);
        }

        // 9b) stagnation storm - hypermutation when novelty and diversity have
        //     both flat-lined for long enough that ordinary mutation rates
        //     cannot climb out of the local optimum.
        if (stagnation.shouldStorm())
            mutationStorm();

        // 9c) the two homeostatic guards
        noiseGuard (dt);
        boredomDrive (dt);

        // the user's intent fades, so a click is a push and not a new regime
        userIntent *= std::exp (-(float) dt * 0.6f);

        // decay visual fields
        geneTransferFlash *= std::exp (-(float) dt * 2.0f);
        extinctionFlash   *= std::exp (-(float) dt * 2.0f);
        pressureFront      *= std::exp (-(float) dt * 1.5f);
        sparkleField      *= std::exp (-(float) dt * 1.1f);
        catalystField     *= std::exp (-(float) dt * 0.9f);
        // the red flash fades fast - "briefly", as asked
        radiationFlash    *= std::exp (-(float) dt * 2.6f);
        thermalField      *= std::exp (-(float) dt * 0.45f);
        for (auto& a : arcs) if (a.life > 0.0f) a.life -= (float) dt * 1.2f;

        // generation accounting: a "generation" is a fixed span of colony time
        tickAccumSec += dt;
        if (tickAccumSec > 2.0)
        {
            tickAccumSec -= 2.0;
            ++generationCount;
        }
    }

    // ---------------------------------------------------------------------

    void Colony::process (juce::AudioBuffer<float>& out,
                          const juce::AudioBuffer<float>* liveIn)
    {
        const int nS = out.getNumSamples();
        if (nS <= 0) return;

        // effect / hybrid: fold the live input into the source ring so grain
        // cells granulate it directly, and let its energy feed the colony.
        if (liveIn != nullptr && src != nullptr && src->numSamples() > 64)
        {
            const int sn = src->numSamples();
            float* sd = src->mono.getWritePointer (0);
            if (liveWritePos >= sn) liveWritePos = 0;
            const int chans = juce::jmax (1, liveIn->getNumChannels());
            float blockRms = 0.0f;
            for (int n = 0; n < nS; ++n)
            {
                float s = 0.0f;
                for (int c = 0; c < chans; ++c) s += liveIn->getReadPointer (c)[n];
                s /= (float) chans;
                blockRms += s * s;
                sd[liveWritePos] = 0.5f * sd[liveWritePos] + 0.5f * s;   // blend, don't stomp
                liveWritePos = (liveWritePos + 1) % sn;
            }
            blockRms = std::sqrt (blockRms / (float) juce::jmax (1, nS));
            // live loudness tops up nutrients a touch
            env.nutrients = clamp01 (env.nutrients * 0.995f + juce::jmin (0.4f, blockRms * 2.0f) * 0.005f);
        }

        int done = 0;
        while (done < nS)
        {
            const int chunk = juce::jmin (nS - done, maxBlockSize);
            const double dt = (double) chunk / sr;

            // run the ecology at the start of every chunk
            tickCounter++;
            ecologyTick (dt);

            // render this chunk
            juce::AudioBuffer<float> outSlice (out.getArrayOfWritePointers(),
                                               out.getNumChannels(), done, chunk);
            excBuffer.clear (0, chunk);
            juce::AudioBuffer<float> excSlice (excBuffer.getArrayOfWritePointers(),
                                               excBuffer.getNumChannels(), 0, chunk);

            const float stress = env.stress();

            // Advance every cell's modulation bank once per chunk. This is the
            // control-rate tick for the six LFO lanes, the drift walk and the
            // decaying gesture effects.
            for (auto& c : cells)
                if (c.alive) c.advanceModulation ((float) dt);

            const bool isolate = isolation.active;
            for (auto& c : cells)
            {
                if (! c.alive) continue;
                if (isolate && ! inScope (c, isolation.level, isolation.id))
                    continue;
                c.extGain = speciesGain[juce::jlimit (0, numSpecies - 1, (int) c.species)];
                c.renderAdd (outSlice, excSlice, *src, stress);
            }

            done += chunk;
        }

        // Measure what we just produced. Everything downstream - the noise
        // guard, the boredom drive, the score rate, the colour of the
        // visualiser - reads these numbers rather than guessing.
        analyser.process (out);

        // master safety / metering
        float rms = 0.0f;
        for (int ch = 0; ch < out.getNumChannels(); ++ch)
        {
            float* d = out.getWritePointer (ch);
            for (int n = 0; n < nS; ++n)
            {
                float v = d[n] * masterGain;
                if (! std::isfinite (v)) v = 0.0f;
                // gentle limiter
                if (v >  1.4f) v =  1.4f;
                if (v < -1.4f) v = -1.4f;
                v = std::tanh (v * 0.8f) * 1.15f;
                d[n] = v;
                rms += v * v;
            }
        }
        rms = std::sqrt (rms / (float) juce::jmax (1, nS * juce::jmax (1, out.getNumChannels())));
        outRmsSmoothed = 0.9f * outRmsSmoothed + 0.1f * rms;
    }

    // ---------------------------------------------------------------------

    void Colony::noteOn (int midiNote, float velocity)
    {
        midiNote = juce::jlimit (0, 127, midiNote);
        const int group = nextGroupId++;
        noteGroup[midiNote] = group;
        ++noteActiveCount;

        const float noteSemi = (float) (midiNote - 60);
        const int burst = juce::jlimit (2, 10,
                            (int) (3 + velocity * 6.0f * (0.5f + env.nutrients)));

        for (int i = 0; i < burst; ++i)
        {
            const int slot = findFreeSlot();
            if (slot < 0) break;

            // build the child genome: blend the colony's current average with
            // the seed baseline according to `memory`.
            Genome g = baselineGenome;
            // pick a living donor to inherit "colony history" from
            int donor = -1;
            for (int t = 0; t < 24; ++t)
            {
                const int cand = rng.intRange (0, (int) cells.size());
                if (cells[(size_t) cand].alive) { donor = cand; break; }
            }
            if (donor >= 0)
                g = Genome::blend (baselineGenome, cells[(size_t) donor].genome, env.memory);

            g.mutate (rng, env.mutation * 0.5f, env.mutationDepth, env.radiation);
            // transpose to the note
            g.set (Trait::pitch, clamp01 (g.get (Trait::pitch) + noteSemi / 48.0f));

            const Species sp = (Species) (i % numSpecies);
            cells[(size_t) slot].germinate (sp, g, nextFamilyId++, group, generationCount + 1, rng);
            cells[(size_t) slot].voiceGroup = group;
            cells[(size_t) slot].energy = 0.25f + 0.5f * velocity;
            cells[(size_t) slot].birthTick = tickCounter;
        }
    }

    void Colony::noteOff (int midiNote)
    {
        midiNote = juce::jlimit (0, 127, midiNote);
        const int group = noteGroup[midiNote];
        if (group < 0) return;
        noteGroup[midiNote] = -1;
        if (noteActiveCount > 0) --noteActiveCount;

        for (auto& c : cells)
            if (c.alive && c.voiceGroup == group && ! c.preserved)
                c.stage = LifeStage::dying;
    }

    void Colony::allNotesOff()
    {
        for (auto& n : noteGroup) n = -1;
        noteActiveCount = 0;
        for (auto& c : cells)
            if (c.alive && c.voiceGroup >= 0 && ! c.preserved)
                c.stage = LifeStage::dying;
    }

    // ---------------------------------------------------------------------
    //  GUI commands
    // ---------------------------------------------------------------------

    void Colony::mutateScope (ScopeLevel lvl, int id)
    {
        for (auto& c : cells)
            if (c.alive && inScope (c, lvl, id) && ! c.preserved)
            {
                c.genome.mutate (rng, juce::jmax (0.4f, env.mutation), juce::jmax (0.4f, env.mutationDepth), env.radiation);
                c.visualPulse = 1.0f;
            }
    }

    void Colony::applySelectionBurst (ScopeLevel lvl, int id, float amount)
    {
        const Genome tgt = selectionTargetGenome();
        for (auto& c : cells)
            if (c.alive && inScope (c, lvl, id) && ! c.preserved)
                c.genome.driftToward (tgt, clamp01 (amount));
        pressureFront = 1.0f;
    }

    void Colony::lockTraits (ScopeLevel lvl, int id, uint32_t mask, bool locked)
    {
        for (auto& c : cells)
        {
            if (! c.alive || ! inScope (c, lvl, id)) continue;
            for (int i = 0; i < numTraits; ++i)
                if ((mask >> i) & 1u) c.genome.raw()[i].locked = locked;
        }
        // also lock the baseline so future spawns respect it
        for (int i = 0; i < numTraits; ++i)
            if ((mask >> i) & 1u) baselineGenome.raw()[i].locked = locked;
    }

    void Colony::setGeneValue (ScopeLevel lvl, int id, int trait, float value)
    {
        if (trait < 0 || trait >= numTraits) return;
        for (auto& c : cells)
            if (c.alive && inScope (c, lvl, id))
                c.genome.set ((Trait) trait, clamp01 (value));
        if (lvl == ScopeLevel::colony)
            baselineGenome.set ((Trait) trait, clamp01 (value));
    }

    void Colony::setGeneDominance (ScopeLevel lvl, int id, int trait, int dominance)
    {
        if (trait < 0 || trait >= numTraits) return;
        for (auto& c : cells)
            if (c.alive && inScope (c, lvl, id))
                c.genome.raw()[trait].dominance = (Dominance) juce::jlimit (0, 3, dominance);
    }

    void Colony::transferGenes (int fromSpecies, int toSpecies, uint32_t traitMask)
    {
        // compute the donor species' average per masked trait, then blend it
        // into every recipient-species cell.
        float avg[numTraits] = {};
        int   n = 0;
        for (auto& c : cells)
            if (c.alive && (int) c.species == fromSpecies)
            {
                for (int i = 0; i < numTraits; ++i) avg[i] += c.genome.raw()[i].value;
                ++n;
            }
        if (n == 0) return;
        for (auto& v : avg) v /= (float) n;

        for (auto& c : cells)
        {
            if (! c.alive || (int) c.species != toSpecies) continue;
            for (int i = 0; i < numTraits; ++i)
            {
                if (! ((traitMask >> i) & 1u)) continue;
                if (c.genome.raw()[i].locked) continue;
                c.genome.raw()[i].value = clamp01 (0.35f * c.genome.raw()[i].value + 0.65f * avg[i]);
            }
            c.visualPulse = 1.0f;
        }
        geneTransferFlash = 1.0f;
    }

    void Colony::infectScope (ScopeLevel lvl, int id, Infection type)
    {
        bool any = false;
        for (auto& c : cells)
            if (c.alive && inScope (c, lvl, id) && ! c.preserved)
            {
                c.infection = type;
                c.infectionLoad = juce::jmax (c.infectionLoad, 0.25f);
                c.visualPulse = 1.0f;
                any = true;
            }
        if (any) infectionField = 1.0f;
    }

    void Colony::cureScope (ScopeLevel lvl, int id)
    {
        for (auto& c : cells)
            if (c.alive && inScope (c, lvl, id))
            {
                c.infection = Infection::none;
                c.infectionLoad = 0.0f;
            }
    }

    void Colony::apoptosisScope (ScopeLevel lvl, int id, float fraction)
    {
        fraction = clamp01 (fraction);
        for (int i = 0; i < (int) cells.size(); ++i)
        {
            auto& c = cells[(size_t) i];
            if (c.alive && inScope (c, lvl, id) && ! c.preserved && rng.chance (fraction))
                killSlot (i, true);
        }
        extinctionFlash = 1.0f;
    }

    void Colony::isolateScope (ScopeLevel lvl, int id)
    {
        isolation.active = true;
        isolation.level  = lvl;
        isolation.id     = id;
    }
    void Colony::clearIsolation() { isolation.active = false; }

    void Colony::muteScope (ScopeLevel lvl, int id, bool mute)
    {
        for (auto& c : cells)
            if (c.alive && inScope (c, lvl, id)) c.muted = mute;
    }

    void Colony::preserveScope (ScopeLevel lvl, int id, bool preserve)
    {
        for (auto& c : cells)
            if (c.alive && inScope (c, lvl, id)) c.preserved = preserve;
    }

    void Colony::eliminateScope (ScopeLevel lvl, int id)
    {
        for (int i = 0; i < (int) cells.size(); ++i)
            if (cells[(size_t) i].alive && inScope (cells[(size_t) i], lvl, id))
                killSlot (i, true);
        extinctionFlash = 1.0f;
    }

    void Colony::injectGenome (const Genome& g, Species sp, int count)
    {
        for (int k = 0; k < count; ++k)
        {
            const int slot = findFreeSlot();
            if (slot < 0) break;
            Genome gg = g;
            if (k > 0) gg.mutate (rng, 0.5f, env.mutationDepth, env.radiation);
            cells[(size_t) slot].germinate (sp, gg, nextFamilyId++, nextGroupId, generationCount + 1, rng);
            cells[(size_t) slot].visualPulse = 1.0f;
        }
        ++nextGroupId;
    }

    void Colony::injectOrganism (const OrganismState& o, bool asMemory)
    {
        const int group = nextGroupId++;
        for (int i = 0; i < o.cellCount && i < OrganismState::maxCells; ++i)
        {
            const auto& cd = o.cells[i];
            if (! cd.alive) continue;
            const int slot = findFreeSlot();
            if (slot < 0) break;

            Genome g = cd.genome.to();
            if (asMemory)
                g = Genome::blend (g, baselineGenome, 1.0f - env.memory);

            cells[(size_t) slot].germinate ((Species) cd.species, g, nextFamilyId++,
                                            group, generationCount + 1, rng);
            cells[(size_t) slot].energy = juce::jmax (0.3f, cd.energy);
            cells[(size_t) slot].visualPulse = 1.0f;
        }
    }

    // ---------------------------------------------------------------------

    void Colony::restoreOrganism (const OrganismState& o)
    {
        clearAll();
        colonySeed = o.seed;
        rng.seed (o.seed);
        generationCount = o.generation;
        env = o.env;
        baselineGenome = o.baseline.to();

        int maxFam = 1, maxGrp = 1;
        const int count = juce::jlimit (0, (int) cells.size(), o.cellCount);
        for (int i = 0; i < count; ++i)
        {
            const auto& cd = o.cells[i];
            Cell& c = cells[(size_t) i];
            c.prepare (sr);
            c.species        = (Species) cd.species;
            c.genome         = cd.genome.to();
            c.familyId       = cd.familyId;
            c.speciesGroupId = cd.speciesGroupId;
            c.generation     = cd.generation;
            c.voiceGroup     = cd.voiceGroup;
            c.stage          = (LifeStage) juce::jlimit (0, (int) LifeStage::dead, (int) cd.stage);
            c.alive          = cd.alive != 0;
            c.preserved      = cd.preserved != 0;
            c.muted          = cd.muted != 0;
            c.ageSec         = cd.ageSec;
            c.lifeSec        = cd.lifeSec;
            c.energy         = cd.energy;
            c.health         = cd.health;
            c.x = cd.x; c.y = cd.y;
            c.infection      = (Infection) juce::jlimit (0, (int) Infection::count - 1, (int) cd.infection);
            c.infectionLoad  = cd.infectionLoad;
            c.stageGain      = c.alive ? 0.5f * c.energy : 0.0f;
            maxFam = juce::jmax (maxFam, cd.familyId + 1);
            maxGrp = juce::jmax (maxGrp, cd.speciesGroupId + 1);
        }
        nextFamilyId = maxFam;
        nextGroupId  = maxGrp;
    }

    OrganismState Colony::captureOrganism() const
    {
        OrganismState o;
        o.seed = colonySeed;
        o.generation = generationCount;
        o.env = env;
        o.baseline = GenomeData::from (baselineGenome);
        o.avgFitness = lastAvgFitness;
        o.diversity  = lastDiversity;

        int idx = 0;
        for (const auto& c : cells)
        {
            if (idx >= OrganismState::maxCells) break;
            if (! c.alive) continue;
            CellData cd;
            cd.species        = (uint8_t) c.species;
            cd.stage          = (uint8_t) c.stage;
            cd.alive          = 1;
            cd.preserved      = c.preserved ? 1 : 0;
            cd.muted          = c.muted ? 1 : 0;
            cd.infection      = (uint8_t) c.infection;
            cd.familyId       = c.familyId;
            cd.speciesGroupId = c.speciesGroupId;
            cd.generation     = c.generation;
            cd.voiceGroup     = c.voiceGroup;
            cd.ageSec         = (float) c.ageSec;
            cd.lifeSec        = (float) c.lifeSec;
            cd.energy         = c.energy;
            cd.health         = c.health;
            cd.x = c.x; cd.y = c.y;
            cd.infectionLoad  = c.infectionLoad;
            cd.genome         = GenomeData::from (c.genome);
            o.cells[idx++] = cd;
            if ((int) c.species < numSpecies) o.popBySpecies[(int) c.species]++;
        }
        o.cellCount = idx;
        return o;
    }

    // ---------------------------------------------------------------------


    // ---------------------------------------------------------------------
    //  The gesture buttons
    //
    //  Enzyme, catalyst, heat and water all re-roll their amount *and* their
    //  effect on every press - pressing one twice in a row must not do the
    //  same thing twice. What is fixed is the shape of the distribution each
    //  one draws from, not the outcome.
    // ---------------------------------------------------------------------

    int Colony::livingSlots (int* out, int maxOut) const
    {
        int n = 0;
        for (int i = 0; i < (int) cells.size() && n < maxOut; ++i)
            if (cells[(size_t) i].alive) out[n++] = i;
        return n;
    }

    void Colony::addEnzyme()
    {
        int live[EngineSnapshot::maxCells];
        const int n = livingSlots (live, EngineSnapshot::maxCells);
        if (n == 0) return;

        // Amount is re-rolled every press, with a long tail: most doses are
        // modest, occasionally one is drastic.
        const float dose = std::pow (rng.nextFloat(), 1.7f) * 0.9f + 0.1f;
        const int   reach = juce::jlimit (1, n, (int) (n * (0.2f + 0.6f * dose)) + 1);

        // An enzyme digests, so the distribution is weighted toward removal
        // that *improves* things. The 8% tail is the price of using it.
        const float roll = rng.nextFloat();
        enum { trimNoise, simplify, consume, misfire } effect =
              roll < 0.58f ? trimNoise
            : roll < 0.80f ? simplify
            : roll < 0.92f ? consume
                           : misfire;

        for (int k = 0; k < reach; ++k)
        {
            Cell& c = cells[(size_t) live[rng.intRange (0, n)]];
            if (! c.alive) continue;

            c.addSparkle (0.35f + 0.65f * dose);
            c.visualPulse = 1.0f;

            auto strip = [&] (Trait t, float by)
            {
                auto& g = c.genome.raw()[(int) t];
                if (! g.locked) g.value = clamp01 (g.value - by);
            };

            switch (effect)
            {
                case trimNoise:
                    // the common case: eat the hiss and the clutter
                    strip (Trait::noiseColour, dose * rng.range (0.15f, 0.45f));
                    strip (Trait::density,     dose * rng.range (0.05f, 0.30f));
                    strip (Trait::aggression,  dose * rng.range (0.05f, 0.25f));
                    break;

                case simplify:
                    // pull a random trait toward neutral - less character,
                    // but also less of whatever was fighting
                    {
                        const int t = rng.intRange (0, numTraits);
                        auto& g = c.genome.raw()[t];
                        if (! g.locked)
                            g.value = clamp01 (g.value + (0.5f - g.value) * dose * 0.6f);
                    }
                    break;

                case consume:
                    // digest a whole cell, weakest first
                    if (! c.preserved && c.fitness < 0.5f)
                        killSlot ((int) (&c - cells.data()), false);
                    break;

                case misfire:
                    // it ate something the sound needed
                    strip (Trait::lfoDepth,   dose * rng.range (0.1f, 0.35f));
                    strip (Trait::brightness, dose * rng.range (0.1f, 0.3f));
                    break;
            }

            c.genome.enforceMovementFloor();
            c.mod.configure (c.genome, world, rng);
        }

        sparkleField = juce::jmax (sparkleField, 0.5f + 0.5f * dose);
        lastGesture = effect == misfire ? -1 : 1;
    }

    void Colony::addCatalyst()
    {
        int live[EngineSnapshot::maxCells];
        const int n = livingSlots (live, EngineSnapshot::maxCells);
        if (n == 0) return;

        const float dose = 0.25f + 0.75f * rng.nextFloat();
        // The wobble rate is part of the roll, so one catalyst shivers and the
        // next one warbles.
        const float rate = rng.range (4.0f, 16.0f);
        const int   reach = juce::jlimit (1, n, (int) (n * (0.3f + 0.6f * dose)) + 1);

        // What it quietly removes on the way out is also rolled.
        const Trait victims[] = { Trait::density, Trait::resonance, Trait::noiseColour,
                                  Trait::brightness, Trait::formant, Trait::envRelease };
        const Trait victim = victims[rng.intRange (0, 6)];

        for (int k = 0; k < reach; ++k)
        {
            Cell& c = cells[(size_t) live[rng.intRange (0, n)]];
            if (! c.alive) continue;

            c.applyCatalyst (dose, rate);
            c.visualPulse = 1.0f;

            // The pitch excursion is the loud part; this is the actual effect,
            // and it is deliberately small enough to be easy to miss.
            auto& g = c.genome.raw()[(int) victim];
            if (! g.locked)
                g.value = clamp01 (g.value - dose * rng.range (0.03f, 0.12f));

            c.genome.enforceMovementFloor();
        }

        catalystField = juce::jmax (catalystField, 0.6f + 0.4f * dose);
        lastGesture = 1;
    }

    void Colony::addHeat (float direction)
    {
        /*  Heat speeds one thing up; water slows the same one thing down.
            Which of the three it catches is the roll - the button does not say
            in advance whether it will take the oscillator, a modulation lane
            or the loop.                                                      */
        int live[EngineSnapshot::maxCells];
        const int n = livingSlots (live, EngineSnapshot::maxCells);
        if (n == 0) return;

        const float amount = (0.06f + 0.22f * rng.nextFloat()) * direction;
        const int   pick   = rng.intRange (0, 3);
        const int   reach  = juce::jlimit (1, n, (int) (n * rng.range (0.35f, 1.0f)) + 1);

        for (int k = 0; k < reach; ++k)
        {
            Cell& c = cells[(size_t) live[rng.intRange (0, n)]];
            if (! c.alive) continue;

            auto nudge = [&] (Trait t, float by)
            {
                auto& g = c.genome.raw()[(int) t];
                if (! g.locked) g.value = clamp01 (g.value + by);
            };

            switch (pick)
            {
                case 0:   // the oscillator: register
                    nudge (Trait::pitch, amount * 0.8f);
                    break;

                case 1:   // a modulation lane: the wobble itself
                    nudge (Trait::lfoRateCentre, amount);
                    nudge (Trait::vibrato, amount * 0.4f);
                    break;

                default:  // the loop: grain rate and the tempo of life
                    nudge (Trait::metabolism, amount * 0.9f);
                    nudge (Trait::duration, -amount * 0.7f);   // shorter grain = faster
                    break;
            }

            c.genome.enforceMovementFloor();
            c.mod.configure (c.genome, world, rng);
            c.visualPulse = 0.8f;
        }

        // The whole colony's clock leans with it, subtly.
        world.lifeTempo = juce::jlimit (0.2f, 4.0f, world.lifeTempo * (1.0f + amount * 0.5f));

        thermalField = juce::jlimit (-1.0f, 1.0f, thermalField + direction * 0.7f);
        lastGesture = 1;
    }

    int Colony::radiate()
    {
        /*  The only gesture with fixed, stated odds, because it is the only one
            that can cost the player the whole run. 5% fatal, 10% a gift, and
            the rest is a shrug - plus a Geiger counter you keep hearing for a
            few seconds after the dice have already landed.                   */
        for (auto& c : cells)
            if (c.alive) c.addGeiger (rng.range (0.5f, 1.0f));

        radiationFlash = 1.0f;

        const float roll = rng.nextFloat();

        if (roll < 0.05f)
        {
            // Catastrophe. Almost everything dies; the score resets.
            for (int i = 0; i < (int) cells.size(); ++i)
            {
                auto& c = cells[(size_t) i];
                if (! c.alive || c.preserved) continue;
                if (rng.chance (0.88f)) killSlot (i, true);
            }
            extinctionFlash = 1.0f;
            lastRadiation = -1;
            lastGesture = -1;
            return -1;
        }

        if (roll < 0.15f)
        {
            // A gift: a genuinely new, strong organism plus a beneficial
            // mutation across the survivors. What it adds is random.
            injectElite (true);
            injectElite (false);

            const Trait boons[] = { Trait::lfoDepth, Trait::brightness, Trait::resonance,
                                    Trait::lfoRateSpread, Trait::drift, Trait::envRelease,
                                    Trait::symbiosisAffinity };
            const Trait boon = boons[rng.intRange (0, 7)];
            const float amt = rng.range (0.12f, 0.4f);

            for (auto& c : cells)
            {
                if (! c.alive) continue;
                auto& g = c.genome.raw()[(int) boon];
                if (! g.locked) g.value = clamp01 (g.value + amt);
                // radiation that helps also cleans, otherwise the "gift" is
                // frequently just more noise
                auto& nz = c.genome.raw()[(int) Trait::noiseColour];
                if (! nz.locked) nz.value = clamp01 (nz.value - amt * 0.4f);
                c.genome.enforceMovementFloor();
                c.mod.configure (c.genome, world, rng);
                c.visualPulse = 1.0f;
            }

            geneTransferFlash = 1.0f;
            lastRadiation = 1;
            lastGesture = 1;
            return 1;
        }

        // The usual case: a small scatter of point mutations and nothing else.
        for (auto& c : cells)
        {
            if (! c.alive || c.preserved) continue;
            if (! rng.chance (0.4f)) continue;
            c.genome.mutate (rng, 0.25f, 0.35f, 0.5f);
            c.genome.enforceMovementFloor();
            c.mod.configure (c.genome, world, rng);
        }
        lastRadiation = 0;
        lastGesture = 0;
        return 0;
    }


    void Colony::knobGesture (int which, float amount, float speed)
    {
        /*  These are not parameters. Turning one does not set a value; it
            applies a gesture whose result depends on the direction, on how
            hard it was turned, and on a roll. Up tends to add and enrich,
            down tends to thin and strip, but neither is guaranteed - the
            brief asks for knobs that can help or hurt, so about one turn in
            six does the opposite of what the direction suggests.          */

        int live[EngineSnapshot::maxCells];
        const int n = livingSlots (live, EngineSnapshot::maxCells);
        if (n == 0) return;

        speed = juce::jlimit (0.0f, 1.0f, speed);
        const float dir = amount >= 0.0f ? 1.0f : -1.0f;
        float mag = juce::jlimit (0.0f, 1.0f, std::fabs (amount)) * (0.35f + 0.65f * speed);

        // The contrary roll. A fast turn is more likely to misbehave, which
        // makes hurrying genuinely risky rather than merely less precise.
        const bool contrary = rng.chance (0.10f + 0.14f * speed);
        const float sign = contrary ? -dir : dir;

        // How much of the colony the gesture catches is also rolled, and a
        // faster turn reaches further.
        const int reach = juce::jlimit (1, n,
                            (int) (n * rng.range (0.2f, 0.45f + 0.5f * speed)) + 1);

        for (int k = 0; k < reach; ++k)
        {
            Cell& c = cells[(size_t) live[rng.intRange (0, n)]];
            if (! c.alive || c.preserved) continue;

            auto nudge = [&] (Trait t, float by)
            {
                auto& g = c.genome.raw()[(int) t];
                if (! g.locked) g.value = clamp01 (g.value + by);
            };

            // Per-cell jitter, so a gesture spreads the colony out rather than
            // moving every caught cell by the same amount.
            const float per = mag * rng.range (0.4f, 1.0f);

            switch (which)
            {
                case 0:   // PITCH
                    nudge (Trait::pitch, sign * per * 0.28f);
                    // a turn also loosens or tightens how tightly the cell tracks
                    nudge (Trait::jitter, sign * per * 0.08f);
                    if (rng.chance (0.25f))
                        nudge (Trait::formant, sign * per * 0.2f);
                    break;

                case 1:   // LFO
                    nudge (Trait::lfoRateCentre, sign * per * 0.3f);
                    nudge (Trait::lfoDepth,      sign * per * 0.22f);
                    if (rng.chance (0.4f))
                        nudge (Trait::lfoRateSpread, sign * per * 0.25f);
                    if (rng.chance (0.3f))
                        nudge (Trait::vibrato, sign * per * 0.2f);
                    break;

                default:  // OSC
                    nudge (Trait::density,    sign * per * 0.3f);
                    nudge (Trait::brightness, sign * per * 0.24f);
                    if (rng.chance (0.35f))
                        nudge (Trait::resonance, sign * per * 0.2f);
                    // turning down also thins the noise, which is why the OSC
                    // knob doubles as a crude clean-up tool
                    if (sign < 0.0f)
                        nudge (Trait::noiseColour, -per * 0.25f);
                    break;
            }

            c.genome.enforceMovementFloor();
            c.mod.configure (c.genome, world, rng);
            c.visualPulse = juce::jmax (c.visualPulse, 0.5f + 0.5f * per);
        }

        // A hard turn leaves a mark on the ecology itself, not just the cells.
        if (speed > 0.6f && rng.chance (0.3f))
        {
            const int ni = rng.intRange (0, nicheCount);
            niches[ni].target.mutate (rng, 0.4f, 0.5f * mag, 0.1f);
            niches[ni].hue = std::fmod (niches[ni].hue + 0.12f * dir + 1.0f, 1.0f);
        }

        pressureFront = juce::jmax (pressureFront, 0.6f);
        lastGesture = contrary ? -1 : 1;
    }

    void Colony::writeSnapshot (EngineSnapshot& s) const
    {
        int idx = 0;
        int popS[numSpecies] = { 0, 0, 0 };
        for (int i = 0; i < (int) cells.size() && idx < EngineSnapshot::maxCells; ++i)
        {
            const auto& c = cells[(size_t) i];
            if (! c.alive) continue;
            CellView v;
            v.species        = (int8_t) c.species;
            v.stage          = (int8_t) c.stage;
            v.infectionType  = (int8_t) c.infection;
            v.preserved      = c.preserved;
            v.muted          = c.muted;
            v.x = c.x; v.y = c.y;
            v.radius = c.visualRadius();
            v.energy = c.energy;
            v.infection = c.infectionLoad;
            v.pulse = c.visualPulse;
            v.fitness = c.fitness;
            v.familyId = c.familyId;
            v.speciesGroupId = c.speciesGroupId;
            v.generation = c.generation;
            v.linkTo = c.linkTo;
            s.cells[idx++] = v;
            if ((int) c.species < numSpecies) popS[(int) c.species]++;
        }
        s.count = idx;
        s.population = idx;
        for (int i = 0; i < numSpecies; ++i) s.popBySpecies[i] = popS[i];
        s.generation = generationCount;
        s.seed = colonySeed;
        s.avgFitness = lastAvgFitness;
        s.diversity = lastDiversity;
        s.outputRms = outRmsSmoothed;
        s.nutrientField = env.nutrients;
        s.mutationField = env.mutation;
        s.temperatureField = env.temperature;
        s.pressureFront = pressureFront;
        s.geneTransferFlash = geneTransferFlash;
        s.extinctionFlash = extinctionFlash;
        s.infectionField = infectionField;
        s.preservedMode = ! env.explore;

        int a = 0;
        for (const auto& arc : arcs)
        {
            if (arc.life <= 0.0f || a >= EngineSnapshot::maxArcs) continue;
            s.arcs[a++] = arc;
        }
        s.arcCount = a;

        // ---- measured audio ------------------------------------------------
        const auto& D = analyser.current();
        s.flatness   = D.flatness;
        s.centroid   = D.centroid;
        s.flux       = D.flux;
        s.roughness  = D.roughness;
        s.tonalness  = D.tonalness;
        s.variety    = D.variety;
        s.appeal     = D.appeal;
        s.greyness   = D.greyness;
        s.noiseLocked = D.noiseLocked;
        s.stuck       = D.stuck;

        // ---- evolution telemetry -------------------------------------------
        s.sparkleField   = sparkleField;
        s.catalystField  = catalystField;
        s.radiationFlash = radiationFlash;
        s.thermalField   = thermalField;
        s.lastRadiation  = lastRadiation;

        s.novelty    = lastNovelty;
        s.coverage   = elites.coverage();
        s.stagnation = stagnation.stagnation();
        s.nicheCount = nicheCount;

        // ---- identity --------------------------------------------------------
        std::memcpy (s.worldName, world.name, sizeof (s.worldName));
        s.worldName[sizeof (s.worldName) - 1] = 0;
        s.worldHue = (float) ((world.seed >> 11) & 0xFFFF) / 65535.0f;

        for (int i = 0; i < EngineSnapshot::maxNiches; ++i)
        {
            const bool live = i < nicheCount;
            s.nicheX[i]   = live ? niches[i].cx : 0.5f;
            s.nicheY[i]   = live ? niches[i].cy : 0.5f;
            s.nicheHue[i] = live ? niches[i].hue : 0.0f;
        }

        const float* spec = analyser.spectrum();
        const int step = DescriptorAnalyser::spectrumBins / EngineSnapshot::spectrumBins;
        for (int i = 0; i < EngineSnapshot::spectrumBins; ++i)
        {
            float m = 0.0f;
            for (int k = 0; k < step; ++k)
            {
                const float v = spec[i * step + k];
                if (v > m) m = v;
            }
            s.spectrum[i] = m;
        }
    }
}
