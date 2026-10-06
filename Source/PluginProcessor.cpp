// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Engine/OrganismSerialization.h"
#include "Engine/Entropy.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <vector>

using namespace mutagen;

MutagenProcessor::MutagenProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), false)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MUTAGEN", params::createLayout())
{
    /*  Seed from harvested entropy rather than from the clock alone.

        A clock-seeded PRNG gives every instance the same statistical
        character, which is part of why two runs used to feel related. The
        pool mixes std::random_device, timer jitter and - once audio starts
        flowing - the noise floor of whatever is plugged into the input.     */
    auto& entropy = globalEntropy();
    entropy.addEvent ((uint64_t) (uintptr_t) this);
    colony.setSeed (entropy.nextSeed());

    // And a fresh world: the tuning, the partial palette, the tempo of life
    // and the modulation topology are all re-rolled per instance.
    colony.setWorld (WorldSeed::fromSeed (entropy.nextSeed()));

    history.clear();

    // The MIDI map addresses parameters by index into this processor's
    // parameter list, so it has to be built after the APVTS layout exists.
    midiLearn.prepare (*this);
}

MutagenProcessor::~MutagenProcessor()
{
    delete pendingSource.exchange (nullptr);
    delete retiredSource.exchange (nullptr);
}

// ---------------------------------------------------------------------------

bool MutagenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;

    const auto& in = layouts.getMainInputChannelSet();
    return in.isDisabled()
        || in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo();
}

void MutagenProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate;
    blockSize    = samplesPerBlock;

    colony.prepare (sampleRate, samplesPerBlock, params::maxCellsFor (cpuQuality()));
    postChain.prepare (sampleRate, samplesPerBlock, 2);
    sourcePool.prepare (sampleRate);
    mic.prepare (sampleRate, samplesPerBlock);

    dryScratch.setSize (2, samplesPerBlock);
    for (auto& replay : hauntedReplayBuffers)
    {
        replay.setSize (1, juce::jmax (1, (int) (sampleRate * 12.0)), false, true, false);
        replay.clear();
    }
    hauntedReplayReady.store (-1);
    hauntedReplayActive.store (-1);
    hauntedReplayPos = 0;
    hauntedReplayDelay = 0;

    tripDelayBuffer.setSize (2, juce::jmax (1, (int) (sampleRate * 8.0)), false, true, false);
    tripDelayBuffer.clear();
    tripDelayWrite = 0;
    tripDelayReadPhase = 0.0;
    temporaryGateSamplesLeft = 0;
    temporaryGateActive.store (false);
    tripDelaySamplesLeft = 0;
    tripDelayActiveFlag.store (false);

    captureRing.setSize (1, juce::jmax (1, (int) (sampleRate * 12.0)));
    captureRing.clear();
    captureWritePos = 0;
    captureRingFilled = false;

    for (auto& s : snapshots) s = EngineSnapshot {};
    for (auto& s : fullState) s = OrganismState {};

    // germinate an initial colony so the plugin makes sound immediately
    colony.germinateFromSource (*p (params::initialPopulation),
                                *p (params::distGrain),
                                *p (params::distSpectral),
                                *p (params::distResonator));

    lastCapturedGeneration = 0;
    lastCaptureTime = 0.0;
    hostTimeSeconds = 0.0;

    if (history.size() == 0)
    {
        history.clear();
        history.addNode (colony.captureOrganism(), -1, "germination");
    }
}

void MutagenProcessor::releaseResources() {}

// ---------------------------------------------------------------------------

params::CpuQuality MutagenProcessor::cpuQuality() const
{
    return (params::CpuQuality) juce::jlimit (0, 2,
        (int) *p (params::cpuQuality));
}
params::PluginRole MutagenProcessor::pluginRole() const
{
    return (params::PluginRole) juce::jlimit (0, 2,
        (int) *p (params::pluginRole));
}

// ---------------------------------------------------------------------------

Environment MutagenProcessor::buildEnvironment() const
{
    Environment e;
    e.nutrients   = *p (params::nutrients);
    e.mutation    = *p (params::mutation);
    e.selection   = *p (params::selection);
    e.metabolism  = *p (params::metabolism);
    e.stability   = *p (params::stability);

    e.fertility     = *p (params::fertility);
    e.mutationDepth = *p (params::mutationDepth);
    e.radiation     = *p (params::radiation);
    e.temperature   = *p (params::temperature);
    e.competition   = *p (params::competition);
    e.symbiosis     = *p (params::symbiosis);
    e.lifespan      = *p (params::lifespan);
    e.apoptosis     = *p (params::apoptosis);
    e.diversity     = *p (params::diversity);
    e.migration     = *p (params::migration);

    e.selBrightness  = *p (params::selBrightness);
    e.selDensity     = *p (params::selDensity);
    e.selHarmonicity = *p (params::selHarmonicity);
    e.selAggression  = *p (params::selAggression);
    e.selDivergence  = *p (params::selDivergence);

    e.explore = *p (params::exploreMode) > 0.5f;
    e.memory  = *p (params::memory);

    // ---- performance macros fold on top ----
    const float mGrowth   = *p (params::macroGrowth);
    const float mMutation = *p (params::macroMutation);
    const float mStress   = *p (params::macroStress);
    const float mDensity  = *p (params::macroDensity);
    const float mMovement = *p (params::macroMovement);
    const float mDecay    = *p (params::macroDecay);
    const float xyStab    = *p (params::xyStability);
    const float xyRepro   = *p (params::xyRepro);

    auto mix = [] (float base, float macro, float weight)
    { return juce::jlimit (0.0f, 1.0f, base * (1.0f - weight) + macro * weight); };

    e.nutrients   = mix (e.nutrients, mGrowth, 0.5f);
    e.fertility   = mix (e.fertility, 0.5f * mGrowth + 0.5f * xyRepro, 0.5f);
    e.mutation    = mix (e.mutation, mMutation, 0.6f);
    e.mutationDepth = mix (e.mutationDepth, mMutation, 0.4f);
    e.temperature = mix (e.temperature, mStress, 0.5f);
    e.radiation   = mix (e.radiation, mStress * mStress, 0.4f);
    e.apoptosis   = mix (e.apoptosis, 0.5f * mStress + 0.5f * mDecay, 0.4f);
    e.competition = mix (e.competition, mDensity < 0.5f ? (1.0f - mDensity) : e.competition, 0.3f);
    e.selDensity  = juce::jlimit (-1.0f, 1.0f, e.selDensity + (mDensity - 0.5f) * 1.4f);
    e.lifespan    = mix (e.lifespan, 1.0f - mDecay, 0.4f);
    e.migration   = mix (e.migration, mMovement, 0.5f);
    e.stability   = mix (e.stability, xyStab, 0.6f);

    // ---- LFO 4 modulates the ecology (mutation by default) ----
    const float lfoDepth = postChain.environmentLfoDepth();
    if (lfoDepth > 1.0e-4f)
    {
        const float m = postChain.environmentLfoValue() * lfoDepth; // -depth..depth
        switch ((params::EnvLfoDest) postChain.environmentLfoDest())
        {
            case params::EnvLfoDest::mutation:      e.mutation      = juce::jlimit (0.0f, 1.0f, e.mutation      + m); break;
            case params::EnvLfoDest::mutationDepth: e.mutationDepth = juce::jlimit (0.0f, 1.0f, e.mutationDepth + m); break;
            case params::EnvLfoDest::nutrients:     e.nutrients     = juce::jlimit (0.0f, 1.0f, e.nutrients     + m); break;
            case params::EnvLfoDest::selection:     e.selection     = juce::jlimit (0.0f, 1.0f, e.selection     + m); break;
            case params::EnvLfoDest::radiation:     e.radiation     = juce::jlimit (0.0f, 1.0f, e.radiation     + m); break;
            case params::EnvLfoDest::temperature:   e.temperature   = juce::jlimit (0.0f, 1.0f, e.temperature   + m); break;
            case params::EnvLfoDest::fertility:     e.fertility     = juce::jlimit (0.0f, 1.0f, e.fertility     + m); break;
        }
    }

    // ---- mod wheel routing into the ecology ----
    if (*p (params::midiReactive) > 0.5f)
    {
        const float mw = modWheelNorm * *p (params::modWheelAmount);
        switch ((params::ModDest) (int) *p (params::modWheelDest))
        {
            case params::ModDest::mutation:  e.mutation  = juce::jlimit (0.0f, 1.0f, e.mutation  + mw); break;
            case params::ModDest::nutrients: e.nutrients = juce::jlimit (0.0f, 1.0f, e.nutrients + mw); break;
            default: break;
        }
    }

    return e;
}

void MutagenProcessor::updateEnvironmentFromParameters()
{
    colony.setEnvironment (buildEnvironment());

    const float body  = *p (params::macroBody);
    const float voice = *p (params::macroVoice);
    // Body lifts resonators, Voice lifts spectral, both gently duck the others.
    const float grainG = 1.0f - 0.35f * (body - 0.5f) - 0.35f * (voice - 0.5f);
    const float specG  = 0.8f + 0.9f * voice;
    const float resG   = 0.8f + 0.9f * body;
    colony.setSpeciesEmphasis (juce::jlimit (0.3f, 1.6f, grainG),
                               juce::jlimit (0.3f, 1.7f, specG),
                               juce::jlimit (0.3f, 1.7f, resG));

    colony.setMasterGain (1.0f);   // final trim now lives in the post chain
    colony.setActiveCap (params::maxCellsFor (cpuQuality()));
    exploringFlag.store (*p (params::exploreMode) > 0.5f);

    PostParams ppp;
    buildPostParams (ppp);
    postChain.setParams (ppp);
}

void MutagenProcessor::buildPostParams (PostParams& q) const
{
    auto f = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    auto b = [&f] (const juce::String& id) { return f (id) > 0.5f; };

    for (int i = 0; i < params::numOscillators; ++i)
    {
        auto& O = q.osc[(size_t) i];
        O.on    = b (params::oscParam (i, "on"));
        O.wave  = (int) f (params::oscParam (i, "wave"));
        O.tune  = f (params::oscParam (i, "tune"));
        O.fine  = f (params::oscParam (i, "fine"));
        O.level = f (params::oscParam (i, "level"));
        O.pan   = f (params::oscParam (i, "pan"));
    }
    q.oscLevel    = f (params::oscLevel);
    q.oscKeytrack = b (params::oscKeytrack);
    q.oscFreeHz   = f (params::oscFreeHz);
    q.oscSpread   = f (params::oscSpread);
    q.oscBlend    = f (params::oscBlend);

    q.synthA = f (params::synthAttack);
    q.synthD = f (params::synthDecay);
    q.synthS = f (params::synthSustain);
    q.synthR = f (params::synthRelease);
    q.synthDrone = b (params::synthDrone);

    q.filterOn   = b (params::filterOn);
    q.filterType = (int) f (params::filterType);
    q.cutoff     = f (params::filterCutoff);
    q.res        = f (params::filterRes);
    q.drive      = f (params::filterDrive);

    for (int i = 0; i < params::numLfos; ++i)
    {
        auto& Lo = q.lfo[(size_t) i];
        Lo.sync  = b (params::lfoParam (i, "sync"));
        Lo.rateHz = f (params::lfoParam (i, "rate"));
        Lo.div   = (int) f (params::lfoParam (i, "div"));
        Lo.depth = f (params::lfoParam (i, "depth"));
        Lo.shape = (int) f (params::lfoParam (i, "shape"));
        Lo.phase = f (params::lfoParam (i, "phase"));
    }
    q.envLfoDest = (int) f (params::lfoParam (params::envLfoIndex, "dest"));

    q.eqOn   = b (params::eqOn);
    q.eqLowF = f (params::eqLowFreq);  q.eqLowG = f (params::eqLowGain);
    q.eqMidF = f (params::eqMidFreq);  q.eqMidG = f (params::eqMidGain);  q.eqMidQ = f (params::eqMidQ);
    q.eqHighF = f (params::eqHighFreq); q.eqHighG = f (params::eqHighGain);

    q.gatorOn      = b (params::gatorOn);
    q.gatorSync    = b (params::gatorSync);
    q.gatorRateHz  = f (params::gatorRate);
    q.gatorDiv     = (int) f (params::gatorDiv);
    q.gatorLength  = (int) f (params::gatorLength);
    q.gatorAttack  = f (params::gatorAttack);
    q.gatorRelease = f (params::gatorRelease);
    q.gatorDepth   = f (params::gatorDepth);
    for (int s = 0; s < params::gatorSteps; ++s)
        q.gatorPattern[(size_t) s] = b (params::gatorStepParam (s));

    q.glitchOn      = b (params::glitchOn);
    q.glitchAmount  = f (params::glitchAmount);
    q.glitchSync    = b (params::glitchSync);
    q.glitchRateHz  = f (params::glitchRate);
    q.glitchDiv     = (int) f (params::glitchDiv);
    q.glitchRepeat  = f (params::glitchRepeat);
    q.glitchReverse = f (params::glitchReverse);
    q.glitchCrush   = f (params::glitchCrush);
    q.glitchTape    = f (params::glitchTape);
    q.glitchMix     = f (params::glitchMix);

    q.midiReactive    = b (params::midiReactive);
    q.lfoKeyRetrigger = b (params::lfoKeyRetrigger);
    q.gatorRetrigger  = b (params::gatorRetrigger);
    q.bendRange       = f (params::midiBendRange);
    q.velToSynth      = f (params::velToSynth);
    q.velToFilter     = f (params::velToFilter);

    q.masterGain = f (params::masterGain);

    q.pitchBend    = pitchBendNorm;
    q.lastNote     = lastMidiNote;
    q.heldNotes    = heldNoteCount;

    // mod-wheel routing into the post chain
    if (q.midiReactive)
    {
        const float mw = modWheelNorm * f (params::modWheelAmount);
        switch ((params::ModDest) (int) f (params::modWheelDest))
        {
            case params::ModDest::filterCutoff:
                q.cutoff = juce::jlimit (20.0f, 20000.0f, q.cutoff * std::pow (2.0f, mw * 4.0f)); break;
            case params::ModDest::synthBlend:
                q.oscBlend = juce::jlimit (-1.0f, 1.0f, q.oscBlend + mw); break;
            case params::ModDest::lfoDepth:
                for (auto& lo : q.lfo) lo.depth = juce::jlimit (0.0f, 1.0f, lo.depth + mw); break;
            case params::ModDest::gatorDepth_:
                q.gatorDepth = juce::jlimit (0.0f, 1.0f, q.gatorDepth + mw); break;
            default: break;
        }
    }
}

void MutagenProcessor::resetEverything()
{
    // 1) every parameter back to its default
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    // 2) wipe message-thread state
    history.clear();
    breedingLab.clear();
    organismName = "MUTAGEN";

    // MIDI mappings are settings too, so RESET forgets them. The randomiser's
    // history goes with them: after a reset, the next RANDOM should randomise
    // from defaults rather than reset a second time.
    midiLearn.clearAll();
    presets.clearRandomHistory();

    // 3) source back to the factory primitive
    loadPrimitiveSource (params::SourceMode::primitiveTone, 3.0f);

    // 4) regrow the colony from a fresh seed on the audio thread
    EngineCommand c;
    c.type = CommandType::hardReset;
    c.u64  = 0x9E3779B97F4A7C15ULL
           ^ ((uint64_t) juce::Time::getHighResolutionTicks() * 0xD1B54A32D192ED03ULL);
    pushCommand (c);

    lastCapturedGeneration = 0;
    lastCaptureTime = 0.0;
    hostTimeSeconds = 0.0;
}

void MutagenProcessor::noteUserGesture (float nx, float ny, juce::uint64 extra)
{
    auto& e = globalEntropy();
    const auto qx = (uint64_t) (juce::jlimit (0.0f, 1.0f, nx) * 65535.0f);
    const auto qy = (uint64_t) (juce::jlimit (0.0f, 1.0f, ny) * 65535.0f);
    e.addEvent ((qx << 32) ^ (qy << 8) ^ (uint64_t) extra);
    e.addTimingJitter();
}

void MutagenProcessor::rollNewWorld()
{
    EngineCommand c;
    c.type = CommandType::newWorld;
    c.u64  = globalEntropy().nextSeed();
    pushCommand (c);
}

// ---------------------------------------------------------------------------

bool MutagenProcessor::pushCommand (const EngineCommand& c)
{
    const int head = cmdHead.load (std::memory_order_relaxed);
    const int next = (head + 1) % kCmdRing;
    if (next == cmdTail.load (std::memory_order_acquire))
        return false; // full
    cmdRing[(size_t) head] = c;
    cmdHead.store (next, std::memory_order_release);
    return true;
}

void MutagenProcessor::pullCommands()
{
    int tail = cmdTail.load (std::memory_order_relaxed);
    const int head = cmdHead.load (std::memory_order_acquire);
    while (tail != head)
    {
        applyCommand (cmdRing[(size_t) tail]);
        tail = (tail + 1) % kCmdRing;
    }
    cmdTail.store (tail, std::memory_order_release);
}

int MutagenProcessor::stageGenomePayload (const Genome& g)
{
    const int idx = payloadWrite.fetch_add (1) % kPayloads;
    genomePayloads[(size_t) idx] = g;
    return idx;
}
int MutagenProcessor::stageOrganismPayload (const OrganismState& o)
{
    const int idx = payloadWrite.fetch_add (1) % kPayloads;
    organismPayloads[(size_t) idx] = o;
    return idx;
}

void MutagenProcessor::applyCommand (const EngineCommand& c)
{
    const auto scope = c.scope;
    const int  id    = c.scopeId;

    switch (c.type)
    {
        case CommandType::none: break;

        case CommandType::germinate:
            colony.germinateFromSource (*p (params::initialPopulation), *p (params::distGrain),
                                        *p (params::distSpectral), *p (params::distResonator));
            captureRequest.store (true);
            break;

        case CommandType::reseedRandom:
            colony.reseedRandom (*p (params::initialPopulation));
            captureRequest.store (true);
            break;

        case CommandType::clearColony:
            colony.clearAll();
            break;

        case CommandType::mutateNow:
            colony.mutateScope (scope, id);
            captureRequest.store (true);
            break;

        case CommandType::applySelection:
            // Selection targets are host parameters. A GUI steering gesture can
            // update those parameters and queue this command in the same audio
            // block, so refresh the colony's environment before applying the
            // one-shot burst rather than steering with last block's targets.
            colony.setEnvironment (buildEnvironment());
            colony.applySelectionBurst (scope, id, c.fa > 0.0f ? c.fa : 0.4f);
            break;

        case CommandType::lockTraits:   colony.lockTraits (scope, id, c.traitMask, true);  break;
        case CommandType::unlockTraits: colony.lockTraits (scope, id, c.traitMask, false); break;

        case CommandType::setGeneValue:     colony.setGeneValue (scope, id, c.ia, c.fa); break;
        case CommandType::setGeneDominance: colony.setGeneDominance (scope, id, c.ia, c.ib); break;

        case CommandType::transferGenes:
            colony.transferGenes (c.ia, c.ib, c.traitMask != 0 ? c.traitMask : 0xFFFFFFFFu);
            break;

        case CommandType::infect:
            colony.infectScope (scope, id, (Infection) juce::jlimit (1, (int) Infection::count - 1, c.ia));
            break;

        case CommandType::cure: colony.cureScope (scope, id); break;

        case CommandType::apoptosis:
            colony.apoptosisScope (scope, id, c.fa > 0.0f ? c.fa : 0.3f);
            captureRequest.store (true);
            break;

        case CommandType::isolate:   colony.isolateScope (scope, id); break;
        case CommandType::unisolate: colony.clearIsolation(); break;
        case CommandType::muteScope:     colony.muteScope (scope, id, c.ib != 0); break;
        case CommandType::preserveScope: colony.preserveScope (scope, id, c.ib != 0); break;
        case CommandType::eliminateScope:
            colony.eliminateScope (scope, id);
            captureRequest.store (true);
            break;

        case CommandType::injectGenome:
            if (c.payloadIndex >= 0 && c.payloadIndex < kPayloads)
                colony.injectGenome (genomePayloads[(size_t) c.payloadIndex],
                                     (Species) juce::jlimit (0, numSpecies - 1, c.ia),
                                     juce::jlimit (1, 24, c.ib));
            captureRequest.store (true);
            break;

        case CommandType::restoreOrganism:
            if (c.payloadIndex >= 0 && c.payloadIndex < kPayloads)
            {
                colony.restoreOrganism (organismPayloads[(size_t) c.payloadIndex]);
                captureRequest.store (true);
            }
            break;

        case CommandType::breedInject:
            if (c.payloadIndex >= 0 && c.payloadIndex < kPayloads)
            {
                colony.injectOrganism (organismPayloads[(size_t) c.payloadIndex], true);
                captureRequest.store (true);
            }
            break;

        case CommandType::captureGeneration:
            captureRequest.store (true);
            break;

        case CommandType::noteBurst:
            colony.noteOn (c.ia, c.fa > 0.0f ? c.fa : 0.8f);
            break;

        case CommandType::hardReset:
            colony.allNotesOff();
            colony.clearAll();
            colony.setSeed (c.u64 != 0 ? c.u64 : 0x1234ABCDULL);
            // A reset is a new run, so it gets new rules too - otherwise every
            // reset would rediscover the same instrument.
            colony.setWorld (WorldSeed::fromSeed (c.u64 ^ 0xD1B54A32D192ED03ULL));
            colony.reset();
            colony.germinateFromSource (*p (params::initialPopulation), *p (params::distGrain),
                                        *p (params::distSpectral), *p (params::distResonator));
            captureRequest.store (true);
            break;

        // ---- the game layer ---------------------------------------------
        case CommandType::mutateAt:
            colony.mutateAt (c.fa, c.fb, c.fc > 0.0f ? c.fc : 0.18f,
                             c.fd > 0.0f ? c.fd : 0.6f);
            break;

        case CommandType::subtractAt:
            colony.subtractAt (c.fa, c.fb, c.fc > 0.0f ? c.fc : 0.18f,
                               c.fd > 0.0f ? c.fd : 0.6f);
            break;

        case CommandType::addEnzyme:   colony.addEnzyme(); break;
        case CommandType::addCatalyst: colony.addCatalyst(); break;
        case CommandType::addHeat:     colony.addHeat (c.fa >= 0.0f ? 1.0f : -1.0f); break;

        case CommandType::radiate:
            radiationOutcome.store (colony.radiate());
            radiationStamp.fetch_add (1);
            break;

        case CommandType::newWorld:
            colony.setWorld (WorldSeed::fromSeed (c.u64));
            captureRequest.store (true);
            break;

        case CommandType::knobGesture:
            colony.knobGesture (c.ia, c.fa, c.fb);
            break;
    }
}

// ---------------------------------------------------------------------------

void MutagenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int totalOut   = getTotalNumOutputChannels();
    const int totalIn    = getTotalNumInputChannels();

    // adopt any freshly analysed source material
    if (auto* incoming = pendingSource.exchange (nullptr))
    {
        SourceMaterial* old = colony.adoptSource (incoming);
        SourceMaterial* expected = nullptr;
        if (! retiredSource.compare_exchange_strong (expected, old))
            delete old; // message thread was slow to clean up; rare
    }

    pullCommands();

    // ---- merge on-screen keyboard, then translate MIDI ----
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const int note = m.getNoteNumber();
            const float vel = m.getFloatVelocity();
            colony.noteOn (note, vel);
            postChain.noteOn (note, vel);
            lastMidiNote = note;
            heldNoteCount = juce::jmin (128, heldNoteCount + 1);
        }
        else if (m.isNoteOff())
        {
            colony.noteOff (m.getNoteNumber());
            postChain.noteOff (m.getNoteNumber());
            heldNoteCount = juce::jmax (0, heldNoteCount - 1);
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            colony.allNotesOff();
            postChain.allNotesOff();
            heldNoteCount = 0;
        }
        else if (m.isPitchWheel())
        {
            pitchBendNorm = juce::jlimit (-1.0f, 1.0f, (m.getPitchWheelValue() - 8192) / 8192.0f);
        }
        else if (m.isController())
        {
            const int cc = m.getControllerNumber();

            if (cc == 1)
                modWheelNorm = m.getControllerValue() / 127.0f;

            // Learned mappings are handled after the mod wheel, not instead
            // of it: mapping something to CC1 should not silently disable the
            // mod wheel's own routing.
            midiLearn.handleController (cc, m.getControllerValue());
        }
    }
    midi.clear();

    updateEnvironmentFromParameters();

    // ---- host transport, for tempo-synced LFOs / gator / glitch ----
    TransportInfo transport;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())            transport.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition())    transport.ppqPosition = *ppq;
            transport.isPlaying = pos->getIsPlaying();
        }
    }

    const auto role = pluginRole();
    const bool wantDry = (role != params::PluginRole::instrument) && totalIn > 0;

    // stash the dry input (also used to granulate live audio inside the colony)
    dryScratch.setSize (2, numSamples, false, false, true);
    dryScratch.clear();
    if (totalIn > 0)
    {
        for (int ch = 0; ch < juce::jmin (2, totalIn); ++ch)
            dryScratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        if (totalIn == 1) dryScratch.copyFrom (1, 0, buffer, 0, 0, numSamples);
    }

    // feed the live-capture ring (input if present, otherwise the output later)
    if (totalIn > 0)
    {
        // The radio-noise tap. Whatever is on the input - an untuned radio, an
        // SDR, a hissing preamp, a mic in a quiet room - its converter noise
        // floor is a physical entropy source, and feedAudio() is wait-free.
        globalEntropy().feedAudio (buffer.getReadPointer (0), numSamples);

        // Microphone capture and the feedback guard both see the raw input.
        mic.process (buffer.getReadPointer (0), numSamples);

        const float* src0 = buffer.getReadPointer (0);
        const int rn = captureRing.getNumSamples();
        float* rd = captureRing.getWritePointer (0);
        for (int n = 0; n < numSamples; ++n)
        {
            rd[captureWritePos] = src0[n];
            if (++captureWritePos >= rn) { captureWritePos = 0; captureRingFilled = true; }
        }
    }

    // clear the buffer; the colony renders the wet signal into it
    for (int ch = 0; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const juce::AudioBuffer<float>* liveIn =
        (role != params::PluginRole::instrument && totalIn > 0) ? &dryScratch : nullptr;

    colony.process (buffer, liveIn);

    // ---- post-colony rack: subtractive synth, filter+LFOs, EQ, gator,
    //      glitch, tremolo, auto-pan, output trim ----
    postChain.process (buffer, transport);

    // ---- hidden laboratory voice ---------------------------------------
    // New recipes are latched without allocation or locks.
    if (const auto stamp = hauntedSoundStamp.load (std::memory_order_acquire);
        stamp != hauntedSoundSeen)
    {
        hauntedSoundSeen = stamp;
        const int recipe = hauntedSoundRecipe.load (std::memory_order_relaxed);
        const float intensity = hauntedSoundIntensity.load (std::memory_order_relaxed);
        hauntedSoundTotal = hauntedSoundRemaining =
            juce::jmax (1, (int) (sampleRateHz * (1.6 + (recipe % 7) * 0.32) * intensity));
        hauntedSoundReverseLatched = hauntedSoundReverse.load (std::memory_order_relaxed);
        hauntedPhaseA = hauntedPhaseB = hauntedSoundReverseLatched
            ? juce::MathConstants<double>::twoPi : 0.0;
        hauntedNoise ^= (uint32_t) recipe * 0x9e3779b9u + 0x7f4a7c15u;
    }

    if (hauntedSoundRemaining > 0)
    {
        const int recipe = hauntedSoundRecipe.load (std::memory_order_relaxed);
        const float intensity = juce::jlimit (0.15f, 1.25f,
            hauntedSoundIntensity.load (std::memory_order_relaxed));
        double fA = 43.0 + (double) ((recipe * 71) % 620);
        double fB = 67.0 + (double) ((recipe * 113) % 980);
        if (recipe == 49999)
        {
            const float lifeForPitch = hauntedSoundTotal > 0
                ? 1.0f - (float) hauntedSoundRemaining / (float) hauntedSoundTotal : 0.0f;
            const float local = std::fmod (lifeForPitch * 4.0f, 1.0f);
            fA = 420.0 + std::sin (local * juce::MathConstants<float>::pi) * 520.0;
            fB = 760.0 + local * 240.0;
        }
        const int chans = juce::jmin (2, totalOut);

        for (int n = 0; n < numSamples && hauntedSoundRemaining > 0; ++n)
        {
            const float life = 1.0f - (float) hauntedSoundRemaining
                                      / (float) juce::jmax (1, hauntedSoundTotal);
            float env = std::sin (juce::MathConstants<float>::pi
                                      * juce::jlimit (0.0f, 1.0f, life));

            // Recipe 49999 is the corner-sprinkle cat event: four separated
            // rising/falling formant-like cries inside one allocation-free voice.
            if (recipe == 49999)
            {
                const float four = life * 4.0f;
                const float local = four - std::floor (four);
                env = std::pow (std::sin (juce::MathConstants<float>::pi * local), 1.7f);
            }
            hauntedNoise ^= hauntedNoise << 13;
            hauntedNoise ^= hauntedNoise >> 17;
            hauntedNoise ^= hauntedNoise << 5;
            const float noise = ((hauntedNoise & 0xffffu) / 32767.5f - 1.0f);

            const double mod = std::sin (hauntedPhaseB) * (0.4 + (recipe % 5) * 0.18);
            float v = (float) std::sin (hauntedPhaseA + mod) * 0.13f
                    + (float) std::sin (hauntedPhaseB * 0.503) * 0.055f
                    + noise * 0.018f;
            v *= env * intensity;

            const double dir = hauntedSoundReverseLatched ? -1.0 : 1.0;
            hauntedPhaseA += dir * juce::MathConstants<double>::twoPi * fA / sampleRateHz;
            hauntedPhaseB += dir * juce::MathConstants<double>::twoPi * fB / sampleRateHz;
            if (hauntedPhaseA > juce::MathConstants<double>::twoPi) hauntedPhaseA -= juce::MathConstants<double>::twoPi;
            if (hauntedPhaseB > juce::MathConstants<double>::twoPi) hauntedPhaseB -= juce::MathConstants<double>::twoPi;
            if (hauntedPhaseA < 0.0) hauntedPhaseA += juce::MathConstants<double>::twoPi;
            if (hauntedPhaseB < 0.0) hauntedPhaseB += juce::MathConstants<double>::twoPi;

            for (int ch = 0; ch < chans; ++ch)
            {
                const float pan = ch == 0 ? (0.7f + 0.3f * std::sin (life * 19.0f))
                                          : (0.7f - 0.3f * std::sin (life * 19.0f));
                buffer.addSample (ch, n, v * pan);
            }
            --hauntedSoundRemaining;
        }
    }

    // Adopt a completed reverse/stretch buffer, then leave a deliberate
    // one-second silent gap before playback so the microphone capture cannot
    // immediately feed its own output back into the room.
    if (hauntedReplayActive.load (std::memory_order_relaxed) < 0)
    {
        const int ready = hauntedReplayReady.exchange (-1, std::memory_order_acq_rel);
        if (ready >= 0)
        {
            hauntedReplayActive.store (ready, std::memory_order_release);
            hauntedReplayPos = 0;
            hauntedReplayDelay = (int) sampleRateHz;
        }
    }

    if (const int slot = hauntedReplayActive.load (std::memory_order_acquire); slot >= 0)
    {
        if (hauntedReplayDelay > 0)
        {
            hauntedReplayDelay -= juce::jmin (hauntedReplayDelay, numSamples);
        }
        else
        {
            auto& replay = hauntedReplayBuffers[(size_t) slot];
            const int length = hauntedReplayLength[(size_t) slot];
            const int room = juce::jmin (numSamples, length - hauntedReplayPos);
            if (room > 0)
            {
                for (int ch = 0; ch < juce::jmin (2, totalOut); ++ch)
                    buffer.addFrom (ch, 0, replay, 0, hauntedReplayPos, room, 0.72f);
                hauntedReplayPos += room;
            }
            if (hauntedReplayPos >= length)
                hauntedReplayActive.store (-1, std::memory_order_release);
        }
    }

    // ---- temporary collision / mini-game gator -----------------------
    if (const auto stamp = temporaryGateStamp.load (std::memory_order_acquire);
        stamp != temporaryGateSeen)
    {
        temporaryGateSeen = stamp;
        const int duration = temporaryGateDurationSeconds.load (std::memory_order_relaxed);
        const int fade = temporaryGateFadeSeconds.load (std::memory_order_relaxed);
        temporaryGateSamplesLeft = (int64_t) (sampleRateHz * duration);
        temporaryGateFadeSamples = (int64_t) (sampleRateHz * fade);
        temporaryGatePhase = 0.0;
        temporaryGateActive.store (true, std::memory_order_release);
    }

    if (temporaryGateSamplesLeft > 0)
    {
        const int bpm = juce::jlimit (40, 320, temporaryGateBpm.load (std::memory_order_relaxed));
        const double beatHz = bpm / 60.0;
        const double gateHz = beatHz * 2.0; // eighth-note on/off pulse
        const int64_t fadeStart = temporaryGateFadeSamples;
        for (int n = 0; n < numSamples; ++n)
        {
            if (temporaryGateSamplesLeft <= 0) break;
            double phase01 = temporaryGatePhase / juce::MathConstants<double>::twoPi;
            phase01 -= std::floor (phase01);
            const float hard = phase01 < 0.5 ? 1.0f : 0.0f;
            float depth = 1.0f;
            if (temporaryGateSamplesLeft < fadeStart && fadeStart > 0)
                depth = (float) temporaryGateSamplesLeft / (float) fadeStart;
            const float gain = 1.0f - depth * (1.0f - hard);
            for (int ch = 0; ch < juce::jmin (2, totalOut); ++ch)
                buffer.setSample (ch, n, buffer.getSample (ch, n) * gain);
            temporaryGatePhase += juce::MathConstants<double>::twoPi * gateHz / sampleRateHz;
            if (temporaryGatePhase > juce::MathConstants<double>::twoPi)
                temporaryGatePhase -= juce::MathConstants<double>::twoPi;
            --temporaryGateSamplesLeft;
        }
        if (temporaryGateSamplesLeft <= 0)
            temporaryGateActive.store (false, std::memory_order_release);
    }

    // ---- Trip Delay / temporary all-sound time stretch -----------------
    if (const auto stamp = tripDelayStamp.load (std::memory_order_acquire);
        stamp != tripDelaySeen)
    {
        tripDelaySeen = stamp;
        tripDelaySamplesLeft = (int64_t) (sampleRateHz
            * tripDelayDurationSeconds.load (std::memory_order_relaxed));
        tripDelayReadPhase = 0.0;
        tripDelayActiveFlag.store (true, std::memory_order_release);
    }

    if (tripDelaySamplesLeft > 0 && tripDelayBuffer.getNumSamples() > 2)
    {
        const int len = tripDelayBuffer.getNumSamples();
        const int delaySamples = juce::jmin (len - 2, (int) (sampleRateHz * 2.75));
        for (int n = 0; n < numSamples; ++n)
        {
            if (tripDelaySamplesLeft <= 0) break;
            const int read = (tripDelayWrite - delaySamples + len) % len;
            const int read2 = (read + 1) % len;
            const float frac = (float) tripDelayReadPhase;
            for (int ch = 0; ch < juce::jmin (2, totalOut); ++ch)
            {
                const float input = buffer.getSample (ch, n);
                const float delayed = juce::jmap (frac,
                    tripDelayBuffer.getSample (ch, read),
                    tripDelayBuffer.getSample (ch, read2));
                tripDelayBuffer.setSample (ch, tripDelayWrite,
                    juce::jlimit (-1.0f, 1.0f, input + delayed * 0.48f));
                buffer.setSample (ch, n, input * 0.78f + delayed * 0.58f);
            }
            if (++tripDelayWrite >= len) tripDelayWrite = 0;
            tripDelayReadPhase += 0.965; // slight time-stretch drift
            if (tripDelayReadPhase >= 1.0) tripDelayReadPhase -= 1.0;
            --tripDelaySamplesLeft;
        }
        if (tripDelaySamplesLeft <= 0)
            tripDelayActiveFlag.store (false, std::memory_order_release);
    }

    // dry / wet blend for effect + hybrid roles
    if (wantDry)
    {
        const float wet = *p (params::dryWet);
        const float dry = 1.0f - wet;
        for (int ch = 0; ch < juce::jmin (totalOut, 2); ++ch)
        {
            buffer.applyGain (ch, 0, numSamples, wet);
            buffer.addFrom (ch, 0, dryScratch, ch, 0, numSamples, dry);
        }
    }

    // capture the output into the ring if there was no input to capture
    if (totalIn == 0)
    {
        const float* o0 = buffer.getReadPointer (0);
        const int rn = captureRing.getNumSamples();
        float* rd = captureRing.getWritePointer (0);
        for (int n = 0; n < numSamples; ++n)
        {
            rd[captureWritePos] = o0[n];
            if (++captureWritePos >= rn) { captureWritePos = 0; captureRingFilled = true; }
        }
    }

    // ---- microphone feedback guard, last thing before the output leaves ----
    // Muting while capturing is the primary protection: an open loop cannot
    // howl. The notch and duck below are for live-monitoring mode and for the
    // case where the room is already ringing before capture starts.
    mic.protectOutput (buffer);

    // ---- publish visual snapshot ----
    colony.writeSnapshot (snapshots[(size_t) snapWrite]);
    snapPublished.store (snapWrite, std::memory_order_release);
    snapWrite = (snapWrite + 1) % 3;

    // ---- history / full-state captures ----
    hostTimeSeconds += (double) numSamples / sampleRateHz;

    const int gen = colony.generation();
    const bool genAdvanced = gen > lastCapturedGeneration;
    const bool timeOk = (hostTimeSeconds - lastCaptureTime) > 3.5;
    bool doCapture = captureRequest.exchange (false);
    if (genAdvanced && timeOk && colony.isExploring())
        doCapture = true;

    if (doCapture)
    {
        const auto scope = historyFifo.write (1);
        if (scope.blockSize1 > 0)
        {
            const int i = scope.startIndex1;
            historyStates[(size_t) i] = colony.captureOrganism();
            historyGenNum[(size_t) i] = gen;
            historyWasGen[(size_t) i] = genAdvanced;
        }
        lastCapturedGeneration = gen;
        lastCaptureTime = hostTimeSeconds;
    }

    // ---- publish full organism state ~2x/sec for the editor ----
    fullStateClock += (double) numSamples / sampleRateHz;
    if (fullStateClock > 0.5)
    {
        fullStateClock = 0.0;
        fullState[(size_t) fullStateWrite] = colony.captureOrganism();
        fullStatePublished.store (fullStateWrite, std::memory_order_release);
        fullStateWrite ^= 1;
        fullStateStamp.fetch_add (1);
    }
}

// ---------------------------------------------------------------------------

void MutagenProcessor::copyLatestSnapshot (EngineSnapshot& dest) const
{
    const int idx = snapPublished.load (std::memory_order_acquire);
    dest = snapshots[(size_t) idx];
}

OrganismState MutagenProcessor::latestOrganism() const
{
    const int idx = fullStatePublished.load (std::memory_order_acquire);
    return fullState[(size_t) idx];
}

int MutagenProcessor::pumpHistory()
{
    // clean up any retired source material handed back by the audio thread
    if (auto* r = retiredSource.exchange (nullptr))
        delete r;

    int filed = 0;
    const auto ready = historyFifo.getNumReady();
    if (ready > 0)
    {
        const auto scope = historyFifo.read (ready);
        auto handleRange = [&] (int start, int size)
        {
            for (int k = 0; k < size; ++k)
            {
                const int i = start + k;
                int parent = history.consumeBranchParent();
                if (parent < 0) parent = history.currentId();
                const bool wasGen = historyWasGen[(size_t) i];
                const juce::String label = wasGen
                    ? juce::String ("gen ") + juce::String (historyGenNum[(size_t) i])
                    : juce::String ("mark");
                history.addNode (historyStates[(size_t) i], parent, label, ! wasGen);
                ++filed;
            }
        };
        handleRange (scope.startIndex1, scope.blockSize1);
        handleRange (scope.startIndex2, scope.blockSize2);
    }
    return filed;
}

// ---------------------------------------------------------------------------
//  Source loading (message thread)
// ---------------------------------------------------------------------------

void MutagenProcessor::loadSourceFromBuffer (const juce::AudioBuffer<float>& buf, double sr,
                                             float transientSensitivity)
{
    auto* sm = new SourceMaterial (analyzer.analyse (buf, sr, transientSensitivity));
    if (auto* stale = pendingSource.exchange (sm))
        delete stale;
}

void MutagenProcessor::loadPrimitiveSource (params::SourceMode mode, float lengthSeconds)
{
    auto* sm = new SourceMaterial (analyzer.makePrimitive (mode, sampleRateHz, lengthSeconds));
    if (auto* stale = pendingSource.exchange (sm))
        delete stale;
}

bool MutagenProcessor::loadSourceFromFile (const juce::File& file)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) return false;

    const int len = (int) juce::jmin ((juce::int64) (reader->sampleRate * 20.0),
                                      reader->lengthInSamples);
    if (len < 64) return false;

    juce::AudioBuffer<float> tmp ((int) reader->numChannels, len);
    reader->read (&tmp, 0, len, 0, true, true);
    loadSourceFromBuffer (tmp, reader->sampleRate, *p (params::transientSens));
    return true;
}

void MutagenProcessor::copyRecentOutput (juce::AudioBuffer<float>& dst, float seconds) const
{
    const int rn = captureRing.getNumSamples();
    const int want = juce::jlimit (1, rn, (int) (seconds * sampleRateHz));
    dst.setSize (1, want, false, false, true);
    const int head = captureWritePos;
    const int start = ((head - want) % rn + rn) % rn;
    for (int n = 0; n < want; ++n)
        dst.setSample (0, n, captureRing.getSample (0, (start + n) % rn));
}

void MutagenProcessor::captureLiveToSource (float seconds, float transientSensitivity)
{
    const int rn = captureRing.getNumSamples();
    const int want = juce::jlimit (1024, rn, (int) (seconds * sampleRateHz));
    juce::AudioBuffer<float> tmp (1, want);
    const int start = ((captureWritePos - want) % rn + rn) % rn;
    for (int n = 0; n < want; ++n)
        tmp.setSample (0, n, captureRing.getSample (0, (start + n) % rn));
    loadSourceFromBuffer (tmp, sampleRateHz, transientSensitivity);
}

// ---------------------------------------------------------------------------
//  State
// ---------------------------------------------------------------------------

void MutagenProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("MUTAGEN_STATE");
    root.setProperty ("version", 1, nullptr);

    root.appendChild (apvts.copyState(), nullptr);

    // current organism (frozen genome / population / seed / env / history)
    const auto organism = latestOrganism();
    root.setProperty ("organism", organismToBase64 (organism), nullptr);
    root.setProperty ("seed", juce::String (colony.seed()), nullptr);
    root.setProperty ("name", organismName, nullptr);

    root.appendChild (history.toValueTree(), nullptr);

    // MIDI mappings travel with the plugin state: they belong to this
    // instance in this session, not to the machine.
    root.appendChild (midiLearn.toValueTree(), nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void MutagenProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;

    juce::ValueTree root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("MUTAGEN_STATE")) return;

    if (auto apvtsChild = root.getChildWithName (apvts.state.getType()); apvtsChild.isValid())
        apvts.replaceState (apvtsChild);

    if (auto evo = root.getChildWithName ("EVOLUTION"); evo.isValid())
        history.fromValueTree (evo);

    if (auto midiMap = root.getChildWithName ("MIDIMAP"); midiMap.isValid())
        midiLearn.fromValueTree (midiMap);
    else
        midiLearn.clearAll();

    const juce::String seedStr = root.getProperty ("seed").toString();
    if (seedStr.isNotEmpty())
        colony.setSeed ((uint64_t) seedStr.getLargeIntValue());

    organismName = root.getProperty ("name", "MUTAGEN").toString();

    OrganismState o;
    if (organismFromBase64 (root.getProperty ("organism").toString(), o) && o.cellCount > 0)
    {
        // apply on the audio thread
        EngineCommand c;
        c.type = CommandType::restoreOrganism;
        c.payloadIndex = stageOrganismPayload (o);
        pushCommand (c);
    }
}

// ---------------------------------------------------------------------------
//  Ingestion (message thread)
// ---------------------------------------------------------------------------

bool MutagenProcessor::digestFile (const juce::File& file)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) return false;

    const int len = (int) juce::jmin ((juce::int64) (reader->sampleRate * 30.0),
                                      reader->lengthInSamples);
    if (len < 64) return false;

    juce::AudioBuffer<float> tmp ((int) reader->numChannels, len);
    reader->read (&tmp, 0, len, 0, true, true);

    digestBuffer (tmp, reader->sampleRate, file.getFileNameWithoutExtension());
    return true;
}

void MutagenProcessor::digestBuffer (const juce::AudioBuffer<float>& buf, double rate,
                                     const juce::String& name)
{
    // The file's own bytes are an entropy observation too - the player chose
    // this sample, at this moment, out of everything on their disk.
    globalEntropy().addEvent ((uint64_t) buf.getNumSamples()
                              ^ ((uint64_t) name.hashCode64()));

    sourcePool.digest (buf, rate, name);

    // Re-analyse the whole digest, not just the new sample, so the colony's
    // features describe the chimera it now has to work with.
    loadSourceFromBuffer (sourcePool.buffer(), sourcePool.rate(),
                          *p (params::transientSens));
}

// ---------------------------------------------------------------------------
//  Microphone
// ---------------------------------------------------------------------------

void MutagenProcessor::armMic (bool shouldArm)
{
    mic.arm (shouldArm);
}

void MutagenProcessor::startMicCapture (float seconds)
{
    mic.startCapture (seconds);
}

bool MutagenProcessor::pollMicCapture()
{
    if (hauntedMicMode)
        return false;
    if (! mic.consumeReady()) return false;

    const int n = mic.capturedLength();
    if (n < 512) return false;

    juce::AudioBuffer<float> tmp (1, n);
    tmp.copyFrom (0, 0, mic.captured(), 0, 0, n);
    digestBuffer (tmp, sampleRateHz, "mic");
    return true;
}

void MutagenProcessor::triggerHauntedSound (int recipe, float intensity)
{
    hauntedSoundRecipe.store (juce::jmax (0, recipe), std::memory_order_relaxed);
    hauntedSoundIntensity.store (juce::jlimit (0.1f, 1.25f, intensity),
                                 std::memory_order_relaxed);
    hauntedSoundReverse.store (false, std::memory_order_relaxed);
    hauntedSoundStamp.fetch_add (1, std::memory_order_release);
}

void MutagenProcessor::triggerSkillSound (int recipe, bool reverse)
{
    hauntedSoundRecipe.store (juce::jmax (0, recipe), std::memory_order_relaxed);
    hauntedSoundIntensity.store (0.86f, std::memory_order_relaxed);
    hauntedSoundReverse.store (reverse, std::memory_order_relaxed);
    hauntedSoundStamp.fetch_add (1, std::memory_order_release);
}

void MutagenProcessor::triggerTemporaryGator (int bpm, int durationSeconds, int fadeSeconds)
{
    temporaryGateBpm.store (juce::jlimit (40, 320, bpm), std::memory_order_relaxed);
    temporaryGateDurationSeconds.store (juce::jlimit (1, 600, durationSeconds), std::memory_order_relaxed);
    temporaryGateFadeSeconds.store (juce::jlimit (1, 60, fadeSeconds), std::memory_order_relaxed);
    temporaryGateStamp.fetch_add (1, std::memory_order_release);
}

void MutagenProcessor::triggerTripDelay (int durationSeconds)
{
    tripDelayDurationSeconds.store (juce::jlimit (1, 600, durationSeconds), std::memory_order_relaxed);
    tripDelayStamp.fetch_add (1, std::memory_order_release);
}

bool MutagenProcessor::startHauntedMicCapture (float seconds)
{
    if (mic.currentState() == MicInput::State::capturing)
        return false;

    hauntedMicMode = true;
    hauntedMicRestoreArmed = mic.isArmed();
    hauntedMicRestoreMonitoring = mic.liveMonitoring();
    mic.setLiveMonitoring (false);
    mic.arm (true);
    mic.startCapture (juce::jlimit (1.0f, 5.0f, seconds));

    if (mic.currentState() != MicInput::State::capturing)
    {
        hauntedMicMode = false;
        mic.setLiveMonitoring (hauntedMicRestoreMonitoring);
        mic.arm (hauntedMicRestoreArmed);
        return false;
    }

    return true;
}

void MutagenProcessor::cancelHauntedMicCapture()
{
    hauntedMicMode = false;
    mic.cancel();
    mic.setLiveMonitoring (hauntedMicRestoreMonitoring);
    mic.arm (hauntedMicRestoreArmed);
}

bool MutagenProcessor::pollHauntedMicCapture()
{
    if (! hauntedMicMode || ! mic.consumeReady())
        return false;

    hauntedMicMode = false;
    const int n = mic.capturedLength();
    if (n < 512)
    {
        mic.setLiveMonitoring (hauntedMicRestoreMonitoring);
        mic.arm (hauntedMicRestoreArmed);
        return false;
    }

    const int active = hauntedReplayActive.load (std::memory_order_acquire);
    const int slot = active == 0 ? 1 : 0;
    auto& out = hauntedReplayBuffers[(size_t) slot];
    out.clear();

    // Reverse + granular overlap/add stretch. 1.65x is long enough to sound
    // unreal while still preserving recognisable fragments of the capture.
    constexpr float stretch = 1.65f;
    constexpr int grain = 1024;
    constexpr int hopIn = grain / 2;
    const int hopOut = juce::roundToInt ((float) hopIn * stretch);
    const int maxOut = out.getNumSamples();
    std::vector<float> norm ((size_t) maxOut, 0.0f);
    float* dst = out.getWritePointer (0);
    const float* src = mic.captured().getReadPointer (0);

    int outStart = 0;
    for (int inStart = 0; inStart + grain < n && outStart + grain < maxOut;
         inStart += hopIn, outStart += hopOut)
    {
        for (int k = 0; k < grain; ++k)
        {
            const int reverseIndex = n - 1 - (inStart + k);
            if (reverseIndex < 0) break;
            const float phase = (float) k / (float) (grain - 1);
            const float w = 0.5f - 0.5f * std::cos (
                juce::MathConstants<float>::twoPi * phase);
            const int oi = outStart + k;
            dst[oi] += src[reverseIndex] * w;
            norm[(size_t) oi] += w;
        }
    }

    int length = juce::jmin (maxOut, outStart + grain);
    for (int i = 0; i < length; ++i)
        if (norm[(size_t) i] > 1.0e-4f)
            dst[i] /= norm[(size_t) i];

    const float mag = out.getMagnitude (0, 0, length);
    if (mag > 0.9f)
        out.applyGain (0, 0, length, 0.9f / mag);

    hauntedReplayLength[(size_t) slot] = length;
    hauntedReplayReady.store (slot, std::memory_order_release);

    mic.setLiveMonitoring (hauntedMicRestoreMonitoring);
    mic.arm (hauntedMicRestoreArmed);
    return true;
}

float MutagenProcessor::entropyTapLevel() const { return globalEntropy().audioTapLevel(); }
bool  MutagenProcessor::entropyTapLive() const  { return globalEntropy().hasLiveTap(); }

// ---------------------------------------------------------------------------

juce::AudioProcessorEditor* MutagenProcessor::createEditor()
{
    return new MutagenEditor (*this);
}

// ---------------------------------------------------------------------------

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MutagenProcessor();
}
