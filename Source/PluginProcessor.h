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

        // engine
        Colony         colony;
        SourceAnalyzer analyzer;
        double sampleRateHz = 44100.0;
        int    blockSize = 512;

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

        // scratch
        juce::AudioBuffer<float> dryScratch;

        double hostTimeSeconds = 0.0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MutagenProcessor)
    };
}
