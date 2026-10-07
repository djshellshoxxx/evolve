// Specimen jar: the WAV export path must produce clean, correctly levelled,
// readable 24-bit files.
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/Engine/SoundCollection.h"
#include <cmath>
#include <iostream>

#define CHECK(condition) do { if (! (condition)) { \
    std::cerr << "CHECK failed at line " << __LINE__ << ": " << #condition << '\n'; \
    return 1; \
} } while (false)

int main()
{
    using namespace mutagen;
    const double sr = 48000.0;

    // A 220 Hz tone sitting on a DC offset, too quiet to be useful as-is.
    juce::AudioBuffer<float> b (1, 48000);
    for (int i = 0; i < b.getNumSamples(); ++i)
        b.setSample (0, i, 0.2f + 0.1f * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * i / sr));

    const float before = SoundCollection::prepare (b, sr);
    CHECK (before > 0.0f);
    const float target = juce::Decibels::decibelsToGain (-1.0f);
    CHECK (std::abs (b.getMagnitude (0, 0, b.getNumSamples()) - target) < 0.01f);  // -1 dBFS
    CHECK (std::abs (b.getSample (0, 0)) < 1.0e-6f);                               // faded in
    CHECK (std::abs (b.getSample (0, b.getNumSamples() - 1)) < 1.0e-3f);           // faded out
    double mean = 0.0;
    for (int i = 0; i < b.getNumSamples(); ++i) mean += b.getSample (0, i);
    CHECK (std::abs (mean / b.getNumSamples()) < 0.01);                            // DC removed

    // Silence is never collected.
    juce::AudioBuffer<float> quiet (1, 4800);
    quiet.clear();
    CHECK (SoundCollection::prepare (quiet, sr) == 0.0f);

    // Round trip through a real 24-bit WAV.
    const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("mutagen-wav-test").getNonexistentSibling();
    const auto file = tmp.getChildFile ("tone.wav");
    CHECK (SoundCollection::writeWav (b, sr, file));

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (file.createInputStream().release(), true));
    CHECK (reader != nullptr);
    CHECK (reader->bitsPerSample == 24);
    CHECK ((int) reader->lengthInSamples == b.getNumSamples());
    CHECK (std::abs (reader->sampleRate - sr) < 0.5);
    juce::AudioBuffer<float> back (1, (int) reader->lengthInSamples);
    reader->read (&back, 0, back.getNumSamples(), 0, true, false);
    reader.reset();
    float maxErr = 0.0f;
    for (int i = 0; i < back.getNumSamples(); ++i)
        maxErr = std::max (maxErr, std::abs (back.getSample (0, i) - b.getSample (0, i)));
    CHECK (maxErr < 1.0e-5f);   // 24-bit quantisation step is ~1.2e-7

    tmp.deleteRecursively();
    std::cout << "Sound collection: PASS\n";
    return 0;
}
