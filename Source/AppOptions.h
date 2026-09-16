#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace mutagen
{
    /*  ==================================================================
        Options that belong to the installation rather than to the patch.

        Tooltips, the audio/MIDI device choice and the like are properties of
        *this machine and this user*, not of the sound, so they live in a
        settings file instead of in the plugin state. A preset moved between
        machines should not drag someone else's soundcard with it.
        ================================================================== */
    class AppOptions
    {
    public:
        static AppOptions& get()
        {
            static AppOptions instance;
            return instance;
        }

        juce::PropertiesFile& file() { return *props; }

        bool tooltipsEnabled() const { return props->getBoolValue ("tooltips", true); }
        void setTooltipsEnabled (bool b) { props->setValue ("tooltips", b); props->saveIfNeeded(); }

        bool showValueOnHover() const { return props->getBoolValue ("hoverValues", true); }
        void setShowValueOnHover (bool b) { props->setValue ("hoverValues", b); props->saveIfNeeded(); }

        /** Saved standalone audio/MIDI device setup, as an XML string. */
        juce::String audioDeviceState() const { return props->getValue ("audioDeviceState"); }
        void setAudioDeviceState (const juce::String& xml)
        {
            props->setValue ("audioDeviceState", xml);
            props->saveIfNeeded();
        }

        static juce::File settingsDirectory()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("MUTAGEN");
        }

    private:
        AppOptions()
        {
            juce::PropertiesFile::Options o;
            o.applicationName     = "MUTAGEN";
            o.filenameSuffix      = ".settings";
            o.folderName          = "MUTAGEN";
            o.osxLibrarySubFolder = "Application Support";
            o.storageFormat       = juce::PropertiesFile::storeAsXML;

            settingsDirectory().createDirectory();
            props = std::make_unique<juce::PropertiesFile> (
                settingsDirectory().getChildFile ("MUTAGEN.settings"), o);
        }

        std::unique_ptr<juce::PropertiesFile> props;
    };
}
