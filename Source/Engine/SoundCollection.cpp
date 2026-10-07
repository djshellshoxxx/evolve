// MUTAGEN
// Copyright © 2026 Sheldon Davidson.
// Licensed under AGPL-3.0-or-later. See LICENSE.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "SoundCollection.h"

namespace mutagen
{
    namespace
    {
        juce::File appFolder()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                .getChildFile ("MUTAGEN");
        }

        juce::String safeName (const juce::String& label)
        {
            auto s = label.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789 -")
                         .trim().replaceCharacter (' ', '-');
            while (s.contains ("--")) s = s.replace ("--", "-");
            return s.isEmpty() ? juce::String ("specimen") : s.substring (0, 40);
        }
    }

    SoundCollection::SoundCollection() { rescan(); }

    juce::File SoundCollection::collectionFolder()
    {
        return appFolder().getChildFile ("Collected Sounds");
    }

    void SoundCollection::rescan()
    {
        items.clear();
        const auto files = collectionFolder().findChildFiles (juce::File::findFiles, false, "*.wav");
        for (const auto& f : files)
        {
            items.add ({ f, f.getFileNameWithoutExtension().fromFirstOccurrenceOf ("-", false, false) });
            nextIndex = juce::jmax (nextIndex, f.getFileName().getIntValue() + 1);
        }
        std::sort (items.begin(), items.end(),
                   [] (const Entry& a, const Entry& b) { return a.file.getFileName() < b.file.getFileName(); });
    }

    float SoundCollection::prepare (juce::AudioBuffer<float>& b, double sampleRate)
    {
        const int n = b.getNumSamples();
        if (n <= 0) return 0.0f;

        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            auto* d = b.getWritePointer (ch);
            double mean = 0.0;
            for (int i = 0; i < n; ++i) mean += d[i];
            mean /= n;
            for (int i = 0; i < n; ++i) d[i] = juce::jlimit (-4.0f, 4.0f, d[i] - (float) mean);
        }

        const float peak = b.getMagnitude (0, n);
        if (! (peak > 1.0e-4f)) return 0.0f;   // silence (or NaN) is not a specimen

        b.applyGain (juce::Decibels::decibelsToGain (-1.0f) / peak);

        const int fade = juce::jmin (n / 4, (int) (sampleRate * 0.010));
        if (fade > 1)
        {
            b.applyGainRamp (0, fade, 0.0f, 1.0f);
            b.applyGainRamp (n - fade, fade, 1.0f, 0.0f);
        }
        return peak;
    }

    bool SoundCollection::writeWav (const juce::AudioBuffer<float>& b, double sampleRate,
                                    const juce::File& destination)
    {
        destination.getParentDirectory().createDirectory();
        destination.deleteFile();

        std::unique_ptr<juce::OutputStream> out = destination.createOutputStream();
        if (out == nullptr) return false;

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (out.get(), sampleRate, (unsigned int) b.getNumChannels(), 24, {}, 0));
        if (writer == nullptr) return false;
        out.release(); // the writer owns the stream now

        return writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }

    bool SoundCollection::collect (const juce::AudioBuffer<float>& source, double sampleRate,
                                   const juce::String& label)
    {
        if (source.getNumSamples() < 64 || sampleRate <= 0.0) return false;

        juce::AudioBuffer<float> copy (source);
        if (prepare (copy, sampleRate) <= 0.0f) return false;

        const auto name = juce::String (nextIndex).paddedLeft ('0', 4) + "-" + safeName (label) + ".wav";
        const auto file = collectionFolder().getChildFile (name);
        if (! writeWav (copy, sampleRate, file)) return false;

        ++nextIndex;
        items.add ({ file, label });
        return true;
    }

    int SoundCollection::exportTo (const juce::File& folder) const
    {
        if (! folder.createDirectory()) return 0;
        int copied = 0;
        for (const auto& e : items)
            if (e.file.existsAsFile() && e.file.copyFileTo (folder.getChildFile (e.file.getFileName())))
                ++copied;
        return copied;
    }

    namespace storyio
    {
        static juce::File file() { return appFolder().getChildFile ("resonance-acts.json"); }

        story::Progress load()
        {
            story::Progress p;
            const auto v = juce::JSON::parse (file());
            if (auto* o = v.getDynamicObject())
            {
                p.lexicon = (std::uint64_t) o->getProperty ("lexicon").toString().getLargeIntValue();
                p.collected = (int) o->getProperty ("collected");
                p.quizzesCorrect = (int) o->getProperty ("quizzesCorrect");
                p.quizzesAsked = (int) o->getProperty ("quizzesAsked");
                p.highestActSeen = juce::jlimit (0, story::actCount, (int) o->getProperty ("highestActSeen"));
                p.runsPlayed = (int) o->getProperty ("runsPlayed");
                p.twistsSeen = (std::uint32_t) (int) o->getProperty ("twistsSeen");
            }
            return p;
        }

        void save (const story::Progress& p)
        {
            auto* o = new juce::DynamicObject();
            // 64-bit mask as a string: JSON numbers are doubles and would lose bits.
            o->setProperty ("lexicon", juce::String ((juce::int64) p.lexicon));
            o->setProperty ("collected", p.collected);
            o->setProperty ("quizzesCorrect", p.quizzesCorrect);
            o->setProperty ("quizzesAsked", p.quizzesAsked);
            o->setProperty ("highestActSeen", p.highestActSeen);
            o->setProperty ("runsPlayed", p.runsPlayed);
            o->setProperty ("twistsSeen", (int) p.twistsSeen);
            file().getParentDirectory().createDirectory();
            file().replaceWithText (juce::JSON::toString (juce::var (o)));
        }
    }
}
