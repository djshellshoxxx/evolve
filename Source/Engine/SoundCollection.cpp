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
        if (! (peak > 0.003f)) return 0.0f;    // below -50 dBFS (or NaN) is not a specimen

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

        // Another instance may share the folder: never overwrite a specimen.
        juce::File file;
        do
        {
            file = collectionFolder().getChildFile (juce::String (nextIndex).paddedLeft ('0', 4)
                                                    + "-" + safeName (label) + ".wav");
            if (file.exists()) ++nextIndex;
        } while (file.exists());
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
                p.secrets = (std::uint64_t) o->getProperty ("secrets").toString().getLargeIntValue();
                p.introSeen = (bool) o->getProperty ("introSeen");
                p.plotFloor = juce::jlimit (1, story::actCount, (int) o->getProperty ("plotFloor"));
                p.knowledge = (std::uint64_t) (juce::int64) o->getProperty ("knowledge");
                p.factSeed = (std::uint64_t) o->getProperty ("factSeed").toString().getLargeIntValue();
                p.factCounter = (std::uint32_t) (juce::int64) o->getProperty ("factCounter");
                p.factSeen = o->getProperty ("factSeen").toString().toStdString();
                p.chanceSeen = (std::uint32_t) (juce::int64) o->getProperty ("chanceSeen");
                p.endingsSeen = (std::uint32_t) (juce::int64) o->getProperty ("endingsSeen");
                p.playerName = o->getProperty ("playerName").toString().substring (0, 40).toStdString();
            }
            return p;
        }

        void save (const story::Progress& mine)
        {
            // Merge with what is on disk, so two open instances never erase
            // each other's progress.
            const auto disk = load();
            story::Progress p = mine;
            p.lexicon |= disk.lexicon;
            p.twistsSeen |= disk.twistsSeen;
            p.secrets |= disk.secrets;
            p.chanceSeen |= disk.chanceSeen;
            p.knowledge = juce::jmax (p.knowledge, disk.knowledge);
            if (p.factSeed == 0)
            {
                p.factSeed = disk.factSeed; p.factCounter = disk.factCounter; p.factSeen = disk.factSeen;
            }
            else if (p.factSeed == disk.factSeed)
            {
                // Same deck: never move backwards, and keep every fact either instance has shown.
                p.factCounter = juce::jmax (p.factCounter, disk.factCounter);
                juce::MemoryOutputStream a, b;
                if (juce::Base64::convertFromBase64 (a, juce::String (p.factSeen))
                    && juce::Base64::convertFromBase64 (b, juce::String (disk.factSeen)))
                {
                    std::vector<std::uint8_t> merged (juce::jmax (a.getDataSize(), b.getDataSize()), (std::uint8_t) 0);
                    const auto* pa = static_cast<const std::uint8_t*> (a.getData());
                    const auto* pb = static_cast<const std::uint8_t*> (b.getData());
                    for (std::size_t i = 0; i < merged.size(); ++i)
                        merged[i] = (std::uint8_t) ((i < a.getDataSize() ? pa[i] : 0) | (i < b.getDataSize() ? pb[i] : 0));
                    p.factSeen = juce::Base64::toBase64 (merged.data(), merged.size()).toStdString();
                }
                else if (p.factSeen.empty())
                    p.factSeen = disk.factSeen;
            }
            p.endingsSeen |= disk.endingsSeen;
            p.introSeen = p.introSeen || disk.introSeen;
            p.plotFloor = juce::jmax (p.plotFloor, disk.plotFloor);
            if (p.playerName.empty()) p.playerName = disk.playerName;
            p.collected = juce::jmax (p.collected, disk.collected);
            p.quizzesCorrect = juce::jmax (p.quizzesCorrect, disk.quizzesCorrect);
            p.quizzesAsked = juce::jmax (p.quizzesAsked, disk.quizzesAsked);
            p.highestActSeen = juce::jmax (p.highestActSeen, disk.highestActSeen);
            p.runsPlayed = juce::jmax (p.runsPlayed, disk.runsPlayed);

            auto* o = new juce::DynamicObject();
            // 64-bit mask as a string: JSON numbers are doubles and would lose bits.
            o->setProperty ("lexicon", juce::String ((juce::int64) p.lexicon));
            o->setProperty ("collected", p.collected);
            o->setProperty ("quizzesCorrect", p.quizzesCorrect);
            o->setProperty ("quizzesAsked", p.quizzesAsked);
            o->setProperty ("highestActSeen", p.highestActSeen);
            o->setProperty ("runsPlayed", p.runsPlayed);
            o->setProperty ("twistsSeen", (int) p.twistsSeen);
            o->setProperty ("secrets", juce::String ((juce::int64) p.secrets));
            o->setProperty ("knowledge", (juce::int64) p.knowledge);
            o->setProperty ("factSeed", juce::String ((juce::int64) p.factSeed));
            o->setProperty ("factCounter", (juce::int64) p.factCounter);
            o->setProperty ("factSeen", juce::String (p.factSeen));
            o->setProperty ("chanceSeen", (juce::int64) p.chanceSeen);
            o->setProperty ("endingsSeen", (juce::int64) p.endingsSeen);
            o->setProperty ("introSeen", p.introSeen);
            o->setProperty ("plotFloor", p.plotFloor);
            o->setProperty ("playerName", juce::String (juce::CharPointer_UTF8 (p.playerName.c_str())));
            file().getParentDirectory().createDirectory();
            file().replaceWithText (juce::JSON::toString (juce::var (o)));
        }
    }
}
