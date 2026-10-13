#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
namespace mutagen::audioImport
{
// These are the readers compiled into this build, including platform-native readers.
inline const juce::String& wildcard()
{
    static const juce::String filter = [] { juce::AudioFormatManager formats; formats.registerBasicFormats(); return formats.getWildcardForAllFormats(); }();
    return filter;
}
inline bool accepts (const juce::String& filename)
{
    for (const auto& pattern : juce::StringArray::fromTokens (wildcard(), ";", ""))
        if (juce::File (filename).getFileName().matchesWildcard (pattern, true)) return true;
    return false;
}
}
