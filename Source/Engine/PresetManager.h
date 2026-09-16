#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace mutagen
{
    /*  ==================================================================
        PresetManager

        A preset here is the *environment*, not the organism. It carries the
        parameter surface only - the ecology you set up, the selection
        targets, the post-chain - and deliberately not the colony itself.

        That split matters: loading a preset onto a running colony should
        change the world the colony lives in and let it react, not replace
        the thing that has been evolving. Saving the organism as well is
        what a run file (.mutagen) is for.
        ================================================================== */

    struct FactoryPreset
    {
        const char* name;
        const char* category;
        const char* description;
        std::vector<std::pair<const char*, float>> values;   // applied over defaults
    };

    class PresetManager
    {
    public:
        explicit PresetManager (juce::AudioProcessorValueTreeState&);

        static constexpr const char* fileExtension = ".mutagenpreset";
        static constexpr const char* wildcard      = "*.mutagenpreset";

        // ---- the factory bank ------------------------------------------
        static const std::vector<FactoryPreset>& factoryPresets();

        /** Names as they appear in the selector, "Category: Name". */
        juce::StringArray factoryNames() const;

        /** Apply a factory preset by index. Every parameter not named by the
            preset returns to its default first, so presets cannot inherit
            stray values from whatever was loaded before. */
        void loadFactory (int index);

        // ---- user presets ----------------------------------------------
        /** %APPDATA%/MUTAGEN/Presets (or the platform equivalent). */
        static juce::File userPresetDirectory();

        juce::StringArray userPresetNames() const;

        bool saveToFile (const juce::File&, const juce::String& displayName);
        bool loadFromFile (const juce::File&);

        /** Save over the preset that is currently loaded. False when there is
            no current file yet, which is the caller's cue to run Save As. */
        bool saveCurrent();

        juce::File    currentFile() const  { return current; }
        juce::String  currentName() const  { return currentDisplayName; }
        bool          isModified() const   { return modified; }
        void          markModified()       { modified = true; }

        // ---- reset and randomise ---------------------------------------
        /** Every parameter back to its default. */
        void resetToDefaults();

        /** A fresh set of settings every press. The first press randomises
            from wherever you are; every press after that resets first, so a
            second press is a genuinely new world rather than a drift away
            from the previous random one. */
        void randomise();

        /** Forget that randomise() has run, so the next press does not reset
            first. Called when the user loads or resets by other means. */
        void clearRandomHistory() { hasRandomised = false; }

        /** Parameters the randomiser leaves alone: output level, CPU budget,
            plugin role and dry/wet are the user's setup, not the sound. */
        static bool isRandomisable (const juce::String& paramID);

    private:
        void applyPairs (const std::vector<std::pair<const char*, float>>&);
        void setParam (const juce::String& id, float plainValue);

        juce::AudioProcessorValueTreeState& apvts;
        juce::File   current;
        juce::String currentDisplayName { "Init" };
        bool         modified = false;
        bool         hasRandomised = false;
        juce::Random rng;
    };
}
