// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "Engine/Colony.h"
#include "Engine/SourceAnalyzer.h"
#include "Engine/EvolutionHistory.h"
#include "Engine/BreedingLab.h"
#include "Engine/OrganismState.h"
#include "Engine/PostChain.h"
#include "Engine/LiveMorph.h"
#include "Engine/Ingest.h"
#include "Engine/MidiLearn.h"
#include "Engine/PresetManager.h"

namespace mutagen
{
    /*  ================================================================
        MutagenProcessor

        Owns the living colony and marshals every interaction between the
        real-time audio thread and the message-thread GUI:

          GUI  --EngineCommand-->  lock-free ring  -->  audio thread
          audio thread  --EngineSnapshot-->  triple buffer  -->  GUI
          audio thread  --OrganismState-->  AbstractFifo  -->  history

        Heavier payloads (a genome, a whole organism, freshly analysed source
        material) travel through dedicated slots referenced by the command.
        ================================================================ */
    class MutagenProcessor : public juce::AudioProcessor
    {
    public:
        MutagenProcessor();
        ~MutagenProcessor() override;

        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override;
        bool isBusesLayoutSupported (const BusesLayout&) const override;
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }

        const juce::String getName() const override { return "MUTAGEN"; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 8.0; }

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return "Colony"; }
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock&) override;
        void setStateInformation (const void*, int sizeInBytes) override;

        // ---- shared with the editor (message thread only) --------------
        juce::AudioProcessorValueTreeState apvts;
        EvolutionHistory history;
        BreedingLab      breedingLab;
        juce::MidiKeyboardState keyboardState;   // on-screen keyboard in the editor

        /** CC -> parameter bindings made by right-clicking a control. Read on
            the audio thread, written from the editor; see MidiLearn. */
        MidiLearn      midiLearn;

        /** The factory bank and the user's own presets. Message thread. */
        PresetManager  presets { apvts };

        /** Latest visual snapshot for the GUI. Safe to call from the message
            thread; copies the most recently published buffer. */
        void copyLatestSnapshot (EngineSnapshot& dest) const;

        /** Queue a command for the audio thread. Returns false if the ring is
            full (extremely unlikely at GUI rates). */
        bool pushCommand (const EngineCommand& c);

        /** Stage a payload and return its slot index (use in EngineCommand). */
        int  stageGenomePayload (const Genome& g);
        int  stageOrganismPayload (const OrganismState& o);

        /** Analyse an arbitrary buffer (or the current capture ring) into fresh
            source material and hand it to the engine. Message thread. */
        void loadSourceFromBuffer (const juce::AudioBuffer<float>& buf, double sr,
                                   float transientSensitivity);
        void loadPrimitiveSource (params::SourceMode mode, float lengthSeconds);
        bool loadSourceFromFile (const juce::File& file);
        void captureLiveToSource (float seconds, float transientSensitivity);

        /** Drain finished OrganismStates into the history tree. Message thread,
            called by the editor's timer. Returns how many were filed. */
        int  pumpHistory();

        /** Snapshot of the current organism for Preserve / Breeding Lab / preset.
            Uses the last full state the audio thread published. */
        OrganismState latestOrganism() const;

        void requestGenerationCapture() { captureRequest.store (true); }

        /** Copy the most recent `seconds` of colony output (mono) for rendering.
            A torn sample at the write head is harmless in a bounce. */
        void copyRecentOutput (juce::AudioBuffer<float>& dst, float seconds) const;

        params::CpuQuality cpuQuality() const;
        params::PluginRole pluginRole() const;
        bool  isExploring() const { return exploringFlag.load(); }

        double currentSampleRate() const { return sampleRateHz; }

        /** Full reset: every parameter to default, history & breeding lab wiped,
            source back to the default primitive, colony regrown from a fresh
            seed. Message thread. */
        void resetEverything();

        juce::String organismName { "MUTAGEN" };

    private:
        void pullCommands();
        void applyCommand (const EngineCommand& c);
        void updateEnvironmentFromParameters();
        Environment buildEnvironment() const;

        void   buildPostParams (PostParams&) const;

        // engine
        Colony         colony;
        SourceAnalyzer analyzer;
        PostChain      postChain;
        SourcePool     sourcePool;
        MicInput       mic;
        double sampleRateHz = 44100.0;
        int    blockSize = 512;

        // live MIDI state for the post-chain / ecology
        int   lastMidiNote = -1;
        float pitchBendNorm = 0.0f;
        float modWheelNorm  = 0.0f;
        int   heldNoteCount = 0;

        // parameter atomics (fetched once per block)
        std::atomic<float>* p (const char* id) const { return apvts.getRawParameterValue (id); }

        // ---- command ring (message -> audio, SPSC) --------------------
        static constexpr int kCmdRing = 512;
        std::array<EngineCommand, kCmdRing> cmdRing;
        std::atomic<int> cmdHead { 0 }, cmdTail { 0 };

        // ---- payload slots -----------------------------------------
        static constexpr int kPayloads = 24;
        std::array<Genome, kPayloads>        genomePayloads;
        std::array<OrganismState, kPayloads> organismPayloads;
        std::atomic<int> payloadWrite { 0 };

        // ---- source hand-off --------------------------------------
        std::atomic<SourceMaterial*> pendingSource { nullptr };
        std::atomic<SourceMaterial*> retiredSource { nullptr };

        // ---- visual snapshot triple buffer -----------------------
        mutable std::array<EngineSnapshot, 3> snapshots;
        std::atomic<int> snapPublished { 0 };
        int snapWrite = 0;

        // ---- history FIFO (audio -> message) --------------------
        static constexpr int kHistoryFifo = 32;
        juce::AbstractFifo historyFifo { kHistoryFifo };
        std::array<OrganismState, kHistoryFifo> historyStates;
        std::array<int,  kHistoryFifo> historyGenNum {};
        std::array<bool, kHistoryFifo> historyWasGen {};
        std::atomic<bool> captureRequest { false };
        int    lastCapturedGeneration = 0;
        double lastCaptureTime = 0.0;
        double fullStateClock = 0.0;

        // ---- full-state publication (audio -> message) --------
        mutable std::array<OrganismState, 2> fullState;
        std::atomic<int> fullStatePublished { 0 };
        int fullStateWrite = 0;
        std::atomic<uint64_t> fullStateStamp { 0 };

        // ---- live capture ring ---------------------------------
        // Two rings: the input (for CAPTURE LIVE) and the final output (for
        // COLLECT, sound export and offline render). They are only resized in
        // prepareToPlay, under ringLock, which the message/render threads hold
        // while copying - so a host re-prepare can never free memory mid-read.
        // Write heads are published once per block.
        juce::AudioBuffer<float> captureRing, outputRing;
        std::atomic<int> captureWritePos { 0 }, outputWritePos { 0 };
        mutable juce::SpinLock ringLock;
        static void writeRing (juce::AudioBuffer<float>&, std::atomic<int>&, const float*, int);
        void readRing (const juce::AudioBuffer<float>&, const std::atomic<int>&,
                       juce::AudioBuffer<float>& dst, float seconds, int minSamples) const;

        std::atomic<bool> exploringFlag { true };

        // RADIATE result, published back to the message thread so the score and
        // the HUD can react to a 5% catastrophe or a 10% gift.
        std::atomic<int>      radiationOutcomes[8] {};   // ring: written by the audio thread only
        std::atomic<uint64_t> radiationStamp   { 0 };

    public:
        /** Most recent RADIATE outcome (-1 fatal, 0 nothing, +1 gift) and a
            counter that increments each time one lands, so the editor can tell
            a new result from a repeat of the last one. */
        int      lastRadiationOutcome() const
        {
            const auto s = radiationStamp.load (std::memory_order_acquire);
            return s == 0 ? 0 : radiationOutcomes[(s - 1) % 8].load (std::memory_order_relaxed);
        }

        /** Reads the oldest radiation result the caller has not seen yet (advance `cursor` from 0).
            Several results in one editor tick are all delivered; only if the editor falls more
            than eight results behind are the oldest dropped. */
        bool nextRadiation (uint64_t& cursor, int& outcome) const
        {
            const auto stamp = radiationStamp.load (std::memory_order_acquire);
            if (cursor >= stamp) return false;
            if (stamp - cursor > 8) cursor = stamp - 8;
            outcome = radiationOutcomes[cursor % 8].load (std::memory_order_relaxed);
            ++cursor;
            return true;
        }
        uint64_t radiationCounter() const { return radiationStamp.load(); }

        /** Re-roll the rules of the run (tuning, palette, tempo, routing). */
        void rollNewWorld();

        /** Fold a user gesture into the entropy pool. Where the player clicked
            and - more usefully - the exact moment they did it are both things
            no algorithm could have predicted, so they are genuine entropy. */
        void noteUserGesture (float nx, float ny, juce::uint64 extra = 0);

        // ---- ingestion -------------------------------------------------
        /** Feed a dropped audio file to the colony. The sample is stitched
            into the digest buffer rather than replacing it, so the colony
            takes some of its form without becoming it. Returns false if the
            file could not be read. Message thread. */
        bool digestFile (const juce::File& file);

        /** Feed an arbitrary buffer (used for microphone captures). */
        void digestBuffer (const juce::AudioBuffer<float>& buf, double rate,
                           const juce::String& name);

        int digestedCount() const { return sourcePool.digestCount(); }
        juce::StringArray digestedNames() const { return sourcePool.eatenNames(); }

        // ---- colony morph: set from the message thread --------------------
        void setMorphScore (float score01) { morphScore01.store (juce::jlimit (0.0f, 1.0f, score01), std::memory_order_relaxed); }
        void setMorphScale (int pitchClassMask, int rootPitchClass)
        {
            morphScaleMask.store (pitchClassMask & 0xfff, std::memory_order_relaxed);
            morphRootPc.store (((rootPitchClass % 12) + 12) % 12, std::memory_order_relaxed);
        }

        // ---- microphone ------------------------------------------------
        void armMic (bool shouldArm);
        void startMicCapture (float seconds);
        bool micArmed() const { return mic.isArmed(); }
        bool micCapturing() const { return mic.currentState() == MicInput::State::capturing; }
        float micLevel() const { return mic.level(); }
        float micHowlFrequency() const { return mic.howlFrequency(); }
        void  setMicLiveMonitoring (bool m) { mic.setLiveMonitoring (m); }

        /** Poll from the editor: a capture finished, or the guard aborted one. */
        bool pollMicCapture();
        bool pollMicAbort() { return mic.consumeAbortFlag(); }

        // ---- hidden laboratory audio -----------------------------------
        void triggerHauntedSound (int recipe, float intensity = 1.0f);
        void triggerSkillSound (int recipe, bool reverse = false);
        void triggerTemporaryGator (int bpm, int durationSeconds, int fadeSeconds = 18);
        void triggerTripDelay (int durationSeconds = 120);
        bool temporaryGatorActive() const { return temporaryGateActive.load(); }
        bool tripDelayActive() const { return tripDelayActiveFlag.load(); }
        bool startHauntedMicCapture (float seconds = 5.0f);
        bool pollHauntedMicCapture();
        void cancelHauntedMicCapture();

        // ---- entropy read-out ------------------------------------------
        float entropyTapLevel() const;
        bool  entropyTapLive() const;

    private:

        // scratch
        juce::AudioBuffer<float> dryScratch;

        // ---- colony morph (effect mode) -----------------------------------
        // Parameter pointers resolved once, so buildPostParams never builds a string on the audio thread.
        std::atomic<float>* oscPtr[params::numOscillators][6] {};
        std::atomic<float>* lfoPtr[params::numLfos][6] {};
        std::atomic<float>* envLfoDestPtr = nullptr;
        std::atomic<float>* gatorStepPtr[params::gatorSteps] {};
        void cachePostParamPointers();

        LiveMorph morph;
        juce::AudioBuffer<float> morphScratch;
        int    morphEventKind = 0;          // audio thread only
        double morphEventAge = 99.0;
        int    morphWarmSamples = 0;        // keeps the morph's buffers fed for a while after it is turned down
        std::atomic<float> morphScore01 { 0.0f };
        std::atomic<int>   morphScaleMask { 0x0ab5 };
        std::atomic<int>   morphRootPc { 0 };

        // Procedural hidden-event voice. Trigger values are atomics because
        // the message thread requests them and the audio thread renders them.
        std::atomic<int> hauntedSoundRecipe { 0 };
        std::atomic<float> hauntedSoundIntensity { 1.0f };
        std::atomic<uint64_t> hauntedSoundStamp { 0 };
        std::atomic<bool> hauntedSoundReverse { false };
        bool hauntedSoundReverseLatched = false;
        uint64_t hauntedSoundSeen = 0;
        double hauntedPhaseA = 0.0, hauntedPhaseB = 0.0;
        int hauntedSoundRemaining = 0, hauntedSoundTotal = 0;
        uint32_t hauntedNoise = 0x31415926u;

        // Two preallocated buffers avoid allocation on the audio thread for
        // reverse/stretch microphone playback.
        std::array<juce::AudioBuffer<float>, 2> hauntedReplayBuffers;
        std::array<int, 2> hauntedReplayLength { 0, 0 };
        std::atomic<int> hauntedReplayReady { -1 };
        std::atomic<int> hauntedReplayActive { -1 };
        int hauntedReplayPos = 0;
        int hauntedReplayDelay = 0;
        bool hauntedMicMode = false;
        bool hauntedMicRestoreArmed = false;
        bool hauntedMicRestoreMonitoring = false;

        std::atomic<int> temporaryGateBpm { 0 };
        std::atomic<int> temporaryGateDurationSeconds { 0 };
        std::atomic<int> temporaryGateFadeSeconds { 18 };
        std::atomic<uint64_t> temporaryGateStamp { 0 };
        std::atomic<bool> temporaryGateActive { false };
        uint64_t temporaryGateSeen = 0;
        int64_t temporaryGateSamplesLeft = 0;
        int64_t temporaryGateFadeSamples = 0;
        double temporaryGatePhase = 0.0;

        std::atomic<int> tripDelayDurationSeconds { 0 };
        std::atomic<uint64_t> tripDelayStamp { 0 };
        std::atomic<bool> tripDelayActiveFlag { false };
        uint64_t tripDelaySeen = 0;
        int64_t tripDelaySamplesLeft = 0;
        juce::AudioBuffer<float> tripDelayBuffer;
        int tripDelayWrite = 0;
        int64_t tripDelayAge = 0;
        double tripDelayReadPhase = 0.0;

        double hostTimeSeconds = 0.0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MutagenProcessor)
    };
}
