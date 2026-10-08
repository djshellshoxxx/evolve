#include "../Source/Engine/MusicFacts.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

using namespace mutagen::facts;

#define CHECK(condition) do { if (! (condition)) { \
    std::cerr << "CHECK failed at line " << __LINE__ << ": " << #condition << '\n'; \
    return 1; \
} } while (false)

namespace
{
    bool contains (const std::string& text, const char* needle)
    {
        return text.find (needle) != std::string::npos;
    }

    // Spot checks: the ID of a fact is its family offset plus its local index.
    // Offsets follow the family order in MusicFacts.cpp.
    struct Spot { int id; const char* needle; };
}

int main (int argc, char** argv)
{
    // --dump: one fact per line as "id<TAB>text", for tools/factcheck.py.
    if (argc > 1 && std::strcmp (argv[1], "--dump") == 0)
    {
        for (int id = 0; id < kFactCount; ++id)
            std::cout << id << '\t' << factAt (id).text << '\n';
        return 0;
    }

    // ---- exactly 30000 non-empty texts, unique, length bounds, demo validity
    std::unordered_set<std::string> seen;
    int demonstrable = 0;
    for (int id = 0; id < kFactCount; ++id)
    {
        const Fact fact = factAt (id);
        CHECK (! fact.text.empty());
        CHECK (fact.text.size() >= 20);
        CHECK (fact.text.size() <= 200);
        CHECK (seen.insert (fact.text).second);
        CHECK (fact.demonstrable == (fact.demo.kind != Demo::Kind::none));
        CHECK (isDemonstrable (id) == fact.demonstrable);
        if (fact.demonstrable) ++demonstrable;

        const Demo& d = fact.demo;
        if (d.kind != Demo::Kind::none)
        {
            CHECK (d.noteCount >= 0 && d.noteCount <= 6);
            CHECK (d.durationMs >= 0 && d.durationMs <= 10000);
            for (int i = 0; i < 6; ++i)
            {
                if (i < d.noteCount)
                    CHECK (d.notes[(size_t) i] >= 21 && d.notes[(size_t) i] <= 108);
                else
                    CHECK (d.notes[(size_t) i] == -1);
            }
        }
        CHECK (fact.text.find ('?') == std::string::npos);
    }
    CHECK (seen.size() == (size_t) kFactCount);

    // ---- demonstrable share between 25% and 60%
    const double share = (double) demonstrable / kFactCount;
    CHECK (share >= 0.25 && share <= 0.60);

    // ---- determinism
    for (int id : { 0, 1, 777, 4242, 15000, 22901, 29999 })
        CHECK (factAt (id).text == factAt (id).text);

    // ---- Deck: no fact repeats before its own kind is used up, for five seeds.
    // With the exact 1-in-5 cadence the non-demo facts are used up first, after
    // nonDemos * 5 / 4 draws; up to then every draw is a new fact.
    {
        int nonDemos = 0;
        for (int id = 0; id < kFactCount; ++id) nonDemos += factAt (id).demonstrable ? 0 : 1;
        const int safeDraws = nonDemos * 5 / 4 - 5;
        for (uint64_t seed : { 1ull, 99ull, 123456789ull, 0xdeadbeefull, 42ull })
        {
            Deck deck (seed, 0);
            std::vector<bool> visited ((size_t) kFactCount, false);
            for (int i = 0; i < safeDraws; ++i)
            {
                const int id = deck.next();
                CHECK (id >= 0 && id < kFactCount);
                CHECK (! visited[(size_t) id]);
                visited[(size_t) id] = true;
            }
            CHECK (deck.counter() == (uint32_t) safeDraws);
        }
    }

    // ---- Deck: exact 1-in-5 demo cadence over 1000 draws
    {
        Deck deck (7, 0);
        int demosInBlock = 0;
        for (int i = 0; i < 1000; ++i)
        {
            const bool predicted = deck.nextShouldBeDemo();
            const int id = deck.next();
            CHECK (factAt (id).demonstrable == predicted);
            if (predicted) ++demosInBlock;
            if (i % 5 == 4)
            {
                CHECK (demosInBlock == 1);
                demosInBlock = 0;
            }
        }
    }

    // ---- Deck: the cadence survives a class running out (non-demos run out first, near draw 26,900)
    {
        Deck deck (11, 0);
        int demosInBlock = 0;
        for (int i = 0; i < 60000; ++i)
        {
            const bool predicted = deck.nextShouldBeDemo();
            const int id = deck.next();
            CHECK (factAt (id).demonstrable == predicted);
            if (predicted) ++demosInBlock;
            if (i % 5 == 4)
            {
                CHECK (demosInBlock == 1);
                demosInBlock = 0;
            }
        }
    }

    // ---- Deck: a persisted counter resumes the same sequence
    {
        Deck full (5, 0);
        for (int i = 0; i < 300; ++i) full.next();
        Deck resumed (5, 300);
        for (int i = 0; i < 200; ++i)
            CHECK (full.next() == resumed.next());
    }

    // ---- knowledge points: always 1..4, all four occur
    {
        std::set<int> values;
        for (uint32_t c = 0; c < 1000; ++c)
        {
            const int k = knowledgePoints (2026, c);
            CHECK (k >= 1 && k <= 4);
            values.insert (k);
        }
        CHECK (values.size() == 4);
    }

    // ---- SeenSet: round trip and count
    {
        SeenSet set;
        CHECK (set.count() == 0);
        std::set<int> marked;
        for (int id = 0; id < kFactCount; id += 7) { set.mark (id); marked.insert (id); }
        set.mark (29999);
        marked.insert (29999);
        set.mark (7);
        CHECK (set.count() == (int) marked.size());

        const std::string encoded = set.toBase64();
        CHECK (encoded.size() == 5000);
        const SeenSet back = SeenSet::fromBase64 (encoded);
        CHECK (back.count() == set.count());
        for (int id = 0; id < kFactCount; ++id)
            CHECK (back.test (id) == (marked.count (id) == 1));
        CHECK (SeenSet::fromBase64 ("not base64!").count() == 0);
    }

    // ---- spot checks against hand-verified values
    const Spot spots[] = {
        {   48, "440.00 Hz" },            // MIDI 69 at A=440
        {  136, "432.00 Hz" },            // MIDI 69 at A=432
        {  224, "415.00 Hz" },            // MIDI 69 at A=415
        {   39, "261.63 Hz" },            // MIDI 60 at A=440
        {   48, "0.78 m" },               // 440 Hz wavelength at 343 m/s
        {  488, "Piano key 49 of 88 is A4" },
        { 1128, "C E G" },                // C major triad
        { 1258, "Bb Db F Ab" },           // B-flat minor seventh
        { 1998, "F# G# A# B C# D# E#" },  // F-sharp major scale
        {  616, "B." },                   // perfect fifth above E is B
        { 2424, "no sharps or flats" },
        { 2426, "2 sharps" },
        { 2434, "2 flats" },
        { 2448, "C up a perfect fifth is G" },
        { 3183, "500.0 ms" },             // quarter note at 120 BPM
        { 3187, "375.0 ms" },             // dotted eighth at 120 BPM
        { 5005, "523.25 Hz" },            // second harmonic of MIDI 60
        { 6245, "22050.0 Hz" },           // Nyquist of 44.1 kHz
        { 6266, "98.08 dB" },             // 16-bit
        { 6200, "-6.02 dB" },             // inverse square, distance 2
        { 6303, "as Bb." },               // B-flat clarinet, written C
        { 6663, "441.29 Hz" },            // 1 m/s approach, A=440 source
    };
    for (const Spot& spot : spots)
    {
        const std::string text = factAt (spot.id).text;
        if (! contains (text, spot.needle))
        {
            std::cerr << "spot " << spot.id << " text '" << text << "' lacks '" << spot.needle << "'\n";
            return 1;
        }
    }

    // ---- the curated hook is empty for now, so the filler is used
    CHECK (curatedText (0) == nullptr);

    std::cout << "MutagenMusicFactsTest passed: " << kFactCount << " facts, "
              << demonstrable << " demonstrable (" << (int) (share * 100) << "%)\n";
    return 0;
}
