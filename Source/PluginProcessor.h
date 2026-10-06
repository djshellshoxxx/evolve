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
        juce::AudioBuffer<float> captureRing;
        int  captureWritePos = 0;
        bool captureRingFilled = false;

        std::atomic<bool> exploringFlag { true };

        // RADIATE result, published back to the message thread so the score and
        // the HUD can react to a 5% catastrophe or a 10% gift.
        std::atomic<int>      radiationOutcome { 0 };
        std::atomic<uint64_t> radiationStamp   { 0 };

    public:
        /** Most recent RADIATE outcome (-1 fatal, 0 nothing, +1 gift) and a
            counter that increments each time one lands, so the editor can tell
            a new result from a repeat of the last one. */
        int      lastRadiationOutcome() const { return radiationOutcome.load(); }
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

        // ---- entropy read-out ------------------------------------------
        float entropyTapLevel() const;
        bool  entropyTapLive() const;

    private:

        // scratch
        juce::AudioBuffer<float> dryScratch;

        double hostTimeSeconds = 0.0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MutagenProcessor)
    };
}
