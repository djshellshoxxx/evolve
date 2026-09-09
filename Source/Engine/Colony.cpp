#include "Colony.h"
#include <cmath>

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
        for (auto& c : cells) { c.prepare (sr); c.alive = false; }

        excBuffer.setSize (2, maxBlockSize);
        workBuffer.setSize (2, maxBlockSize);

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

        const int grainGroup = nextGroupId++;
        const int specGroup  = nextGroupId++;
        const int resGroup   = nextGroupId++;

        for (int i = 0; i < target; ++i)
        {
            const int slot = findFreeSlot();
            if (slot < 0) break;

            const float r = rng.nextFloat();
            Species sp; int group;
            if      (r < pGrain)         { sp = Species::grain;     group = grainGroup; }
            else if (r < pGrain + pSpec) { sp = Species::spectral;  group = specGroup; }
            else                         { sp = Species::resonator; group = resGroup; }

            Genome g = baselineGenome;
            g.mutate (rng, 0.9f, 0.4f, 0.0f);   // spread the founders out
            // give each species a characteristic bias
            if (sp == Species::grain)     g.set (Trait::duration, rng.range (0.05f, 0.4f));
            if (sp == Species::spectral)  g.set (Trait::density,  rng.range (0.3f, 0.9f));
            if (sp == Species::resonator) g.set (Trait::resonance, rng.range (0.45f, 0.9f));

            cells[(size_t) slot].germinate (sp, g, nextFamilyId++, group, 1, rng);
            cells[(size_t) slot].birthTick = tickCounter;
        }
    }

    void Colony::reseedRandom (float initialPop)
    {
        clearAll();
        generationCount = 1;
        baselineGenome.randomise (rng);
        const int target = juce::jlimit (3, maxCellCount,
                             (int) std::round (initialPop * (float) maxCellCount * 0.8f) + 3);
        for (int i = 0; i < target; ++i)
        {
            const int slot = findFreeSlot();
            if (slot < 0) break;
            Genome g; g.randomise (rng);
            const Species sp = (Species) rng.intRange (0, numSpecies);
            cells[(size_t) slot].germinate (sp, g, nextFamilyId++, nextGroupId, 1, rng);
        }
        ++nextGroupId;
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

    void Colony::evaluateFitness()
    {
        const Genome tgt = selectionTargetGenome();
        float sum = 0.0f; int n = 0;
        for (auto& c : cells)
        {
            if (! c.alive) continue;
            float fit = 1.0f;
            fit -= 0.8f * std::abs (c.genome.get (Trait::brightness)  - tgt.get (Trait::brightness));
            fit -= 0.7f * std::abs (c.genome.get (Trait::density)     - tgt.get (Trait::density));
            fit -= 0.7f * std::abs (c.genome.get (Trait::noiseColour) - tgt.get (Trait::noiseColour));
            fit -= 0.5f * std::abs (c.genome.get (Trait::aggression)  - tgt.get (Trait::aggression));

            const float div = c.genome.distanceTo (baselineGenome);
            fit -= 0.6f * std::abs (div - env.selDivergence);

            fit = clamp01 (0.15f + 0.85f * fit);
            // preserved cells are always "fit enough" to keep breeding
            if (c.preserved) fit = juce::jmax (fit, 0.75f);
            c.fitness = fit;
            sum += fit; ++n;
        }
        lastAvgFitness = n > 0 ? sum / (float) n : 0.5f;
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

        child.germinate (parent.species, g, parent.familyId, parent.speciesGroupId,
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
        const float metabRate = 0.3f + 1.7f * env.metabolism;   // scales lifecycle speed
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

        // 2) fitness under the current selection target
        evaluateFitness();

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

        // 8) selection drift: nudge unlocked genes toward the target
        if (env.selection > 0.01f)
        {
            const Genome tgt = selectionTargetGenome();
            const float amt = (float) dt * env.selection * 0.25f;
            for (auto& c : cells)
            {
                if (! c.alive || c.preserved) continue;
                // fitter cells drift less (they're already good); unfit cells move
                c.genome.driftToward (tgt, amt * (1.2f - c.fitness));
            }
            pressureFront = 1.0f;
        }

        // 9) diversity maintenance: inject an immigrant if the gene pool collapses
        lastDiversity = genomeVariance();
        if (lastDiversity < env.diversity * 0.4f && population() < maxCellCount
            && rng.chance ((float) dt * 0.5f))
        {
            const int slot = findFreeSlot();
            if (slot >= 0)
            {
                Genome g; g.randomise (rng);
                g = Genome::blend (baselineGenome, g, 0.6f);
                const Species sp = (Species) rng.intRange (0, numSpecies);
                cells[(size_t) slot].germinate (sp, g, nextFamilyId++, rng.intRange (1, nextGroupId + 1), generationCount, rng);
                cells[(size_t) slot].visualPulse = 1.0f;
            }
        }

        // decay visual fields
        geneTransferFlash *= std::exp (-(float) dt * 2.0f);
        extinctionFlash   *= std::exp (-(float) dt * 2.0f);
        pressureFront      *= std::exp (-(float) dt * 1.5f);
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
    }
}
