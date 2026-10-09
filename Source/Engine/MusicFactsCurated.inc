// GENERATED FILE. Do not edit by hand.
// Source: docs/data/curated_facts.json (89 entries). Generator: tools/gen_facts.py.
// Included inside the anonymous namespace of Source/Engine/MusicFacts.cpp.
// Index i is the i-th JSON entry and is served at fact ID 27000 + i.

constexpr int kEmbedCount = 89;

struct EmbedDemo
{
    Demo::Kind kind;
    int noteCount;
    int notes[6];
    int durationMs;
    int fxId;
};

constexpr const char* kEmbedText[kEmbedCount] = {
    "A standard piano has 88 keys, from A0 at about 27.5 Hz up to C8 at about 4186 Hz, a span of more than seven octaves.",
    "Pressing a piano key drives a felt-covered hammer toward a string, and the hammer rebounds so the string can vibrate freely.",
    "A harpsichord plucks its strings with small plectra of quill or leather mounted on upright jacks, rather than striking them with hammers.",
    "A trombone's slide lengthens or shortens the tubing to change pitch, doing the job that valves do on other brass instruments.",
    "An open organ pipe sounds harmonics 1, 2, 3, 4 and so on, while a closed pipe sounds only odd-numbered harmonics 1, 3, 5, 7.",
    "The Hammond organ makes its tones with tonewheels, spinning disks with indentations that pass a magnetic pickup, rather than with pipes or strings.",
    "Adolphe Sax, a Belgian-born instrument maker working in Paris, patented the saxophone in 1846 as a metal single-reed instrument with a conical bore.",
    "The Fender Rhodes electric piano is played by hammers striking steel tines, and each tine has its own electromagnetic pickup.",
    "In 12-tone equal temperament each semitone multiplies frequency by the twelfth root of two, about 1.0595, so twelve semitones make exactly one octave.",
    "A cent is 1/100 of a semitone, so an octave contains 1200 cents, and a frequency ratio r equals 1200 times log2(r) cents.",
    "Twelve perfect fifths overshoot seven octaves by the Pythagorean comma, the ratio 531441/524288, or about 23.46 cents.",
    "A pure 5:4 major third measures about 386 cents, roughly 14 cents narrower than the 400-cent major third of equal temperament.",
    "A note's overtones sit at whole-number multiples of its fundamental, so the second harmonic is an octave up and the third lies a fifth above that.",
    "The ISO 16:1975 standard fixes the note A4 at 440 Hz as the reference for concert pitch.",
    "Each step clockwise around the circle of fifths adds one sharp to the key signature, so C major has none and G major has one, F sharp.",
    "The musical staff is traditionally credited to Guido of Arezzo, an 11th-century monk, who used lines and spaces to show pitch.",
    "The major scale follows the step pattern whole, whole, half, whole, whole, whole, half, which is 2-2-1-2-2-2-1 semitones.",
    "A dot after a note adds half of that note's value, so a dotted half note lasts three beats in 4/4 time.",
    "The syllables ut, re, mi, fa, sol and la come from the opening lines of the Latin hymn Ut queant laxis, each line starting one step higher.",
    "In equal temperament there are only two whole-tone scales, each with six notes and built entirely from whole steps.",
    "A pentatonic scale has five notes per octave, and versions of it appear in music traditions from China to Java.",
    "On a keyboard tuned to equal temperament, C sharp and D flat are the same black key and sound identical, though they are spelled differently.",
    "In Werckmeister III only four perfect fifths are slightly flattened and the rest kept pure, so keys differ in character unlike in equal temperament.",
    "John Chowning developed FM synthesis at Stanford, and Yamaha's DX7 of 1983 brought the technique to a very wide market.",
    "The Karplus-Strong algorithm, published in 1983, makes a plucked-string sound by feeding a short noise buffer through a simple averaging filter.",
    "Sidechain compression turns a bass track down each time a kick drum hits, so the kick cuts through the mix without clashing.",
    "An ADSR envelope shapes a note with attack, decay, sustain and release, where sustain is a held level rather than a length of time.",
    "The Moog ladder filter uses four transistor stages to roll off high frequencies at 24 dB per octave.",
    "Granular synthesis builds sounds from very short grains, usually under 50 ms, and was proposed for computers by Xenakis in 1971 and Roads in 1978.",
    "Convolution reverb measures a real room's impulse response and convolves it with a dry signal, placing that sound inside the measured space.",
    "An LFO is a low-frequency oscillator, usually running from about 0.1 to 5 Hz, that modulates pitch for vibrato or volume for tremolo.",
    "Subtractive synthesis starts with a harmonically rich waveform such as a sawtooth and removes partials with a low-pass filter, since a pure sine wave has no harmonics to filter.",
    "A passing siren sounds higher as it approaches and lower after it passes, because the sound waves bunch up and then spread out.",
    "In open space, doubling the distance from a point source lowers its sound level by about 6 dB, because intensity falls with the square of distance.",
    "A loud sound hides quieter sounds at nearby frequencies, an effect called simultaneous masking that MP3 compression relies on.",
    "Two tones at 440 Hz and 442 Hz produce two beats per second, because the beat rate equals the difference between the two frequencies.",
    "The ear can supply a missing lowest tone: three harmonics at 200, 300 and 400 Hz make listeners hear a 100 Hz pitch that is not physically present.",
    "The stapes, a stirrup-shaped bone in the middle ear, is the smallest bone in the human body and passes vibrations on to the inner ear.",
    "A Helmholtz resonator, such as a bottle, has a pitch set by the size of its cavity and neck, and a larger cavity gives a lower frequency.",
    "On the decibel sound pressure level scale, 0 dB is defined as 20 micropascals, a reference close to the threshold of hearing at 1 kHz.",
    "In the cochlea, high frequencies are detected near the base and low frequencies near the apex, a layout called tonotopic organization.",
    "Wallace Sabine found that a room's reverberation time depends on its volume divided by its total sound absorption, a result from his work on Harvard's Fogg lecture hall.",
    "Equal-loudness curves first measured by Fletcher and Munson in 1933 show that the ear is most sensitive roughly between 1 and 5 kHz.",
    "MIDI messages travel over a serial link at 31,250 bits per second, a rate set when the standard was drafted in the early 1980s.",
    "Roland's TR-808 drum machine, released in 1980, used analog circuits because memory for real drum samples was too expensive at the time.",
    "The Fairlight CMI, launched in 1979 by Kim Ryrie and Peter Vogel, was among the first commercially available digital sampling instruments.",
    "The Red Book standard for compact discs, documented by Sony and Philips in 1980, specifies 16-bit stereo audio sampled at 44.1 kHz.",
    "A 44.1 kHz sample rate can capture frequencies up to 22.05 kHz, just above the 20 kHz limit of typical human hearing.",
    "A Mellotron plays a pre-recorded tape strip under each key, and each strip holds only about eight seconds of sound.",
    "AEG's Magnetophon, shown in 1935, was an early commercial reel-to-reel tape recorder built on Fritz Pfleumer's magnetic tape.",
    "The Linn LM-1 drum machine of 1980 was among the first to use recorded samples of real acoustic drums.",
    "The Minimoog of 1970 put a keyboard, three oscillators, a filter and an amplifier into one portable instrument.",
    "The ondes Martenot, introduced in France by Maurice Martenot in 1928, is played with a keyboard and a ribbon controller that slides the pitch.",
    "MPEG-1 Audio Layer III, the basis of the MP3 format, was published as ISO/IEC 11172-3 in 1993 and relies on auditory masking.",
    "Homer Dudley at Bell Labs developed the vocoder in the late 1930s, analyzing speech with a bank of filters and rebuilding it from electronic tones.",
    "Cooley and Tukey published their fast Fourier transform algorithm in 1965, cutting the cost of a discrete Fourier transform from about N squared to N log N operations.",
    "Roger Shepard described his endless-rising tone illusion in 1964, built from sine waves an octave apart whose loudness fades at the extremes.",
    "Godfried Toussaint's 2005 paper showed that a rhythm algorithm first used for accelerator timing produces many traditional world rhythms.",
    "In 1913 Andrey Markov analyzed vowels and consonants in Pushkin's Eugene Onegin, showing that each letter depended on the one before, the basis of Markov chains.",
    "The YIN pitch estimator of de Cheveigne and Kawahara, published in 2002, refines autocorrelation with a cumulative mean normalized difference function.",
    "In 1807 Joseph Fourier argued that a temperature profile in a bar could be written as a sum of sine waves, an idea that was controversial at first.",
    "The radix-2 Cooley-Tukey FFT works on lengths that are powers of two, splitting the input in halves and combining pairs of values in butterfly operations.",
    "Autocorrelation pitch detection finds the period of a sound as the lag at which the signal best matches a delayed copy of itself.",
    "After a 1928 caravan fire injured two fingers of his left hand, Django Reinhardt rebuilt his guitar playing around his index and middle fingers.",
    "Jimi Hendrix was left-handed but played right-handed Stratocasters that he turned upside down and restrung.",
    "Wendy Carlos's album Switched-On Bach, released in October 1968, performed works by Bach on a Moog synthesizer.",
    "Bach's Well-Tempered Clavier contains 24 preludes and fugues, one pair in every major and minor key, in two books dated 1722 and 1742.",
    "Glenn Gould's 1955 recording of Bach's Goldberg Variations was his debut for Columbia Records, released in January 1956.",
    "Beethoven's Ninth Symphony premiered in Vienna on 7 May 1824, by which time the composer was almost completely deaf.",
    "Miles Davis released Kind of Blue on Columbia Records on 17 August 1959, an album built on modal jazz.",
    "Louis Armstrong's Hot Five made its first recordings in Chicago on 12 November 1925, with Armstrong playing cornet and singing.",
    "Stevie Wonder's 1972 album Music of My Mind was his first to use synthesizers, played through the Moog-based TONTO system.",
    "Hildegard of Bingen's morality play Ordo Virtutum, probably written around 1151, survives with its music and text both attributed to her.",
    "Clara Schumann married Robert Schumann on 12 September 1840 and built a concert career that lasted about 61 years.",
    "Stockhausen's Studie II, premiered in Cologne in October 1954, is made from pure sine tones and was the first electronic work to be notated and published.",
    "The Hang drum was created in Bern, Switzerland, in 2000 and is made from two glued half-shells of nitrided steel.",
    "The steelpan was developed in Trinidad and Tobago in the 1930s, when players began making music from discarded metal objects such as oil drums.",
    "The kora is a West African instrument with usually 21 strings that combines features of a harp and a lute, traditionally played by Mandinka griots.",
    "A sitar has 18 to 21 strings: six or seven played strings over movable frets, and sympathetic strings that resonate without being plucked.",
    "The erhu is a two-stringed Chinese bowed instrument and the most popular member of the huqin family of bowed string instruments.",
    "The shakuhachi is a Japanese end-blown bamboo flute with four finger holes on the front and one thumb hole on the back.",
    "The traditional yidaki, or didgeridoo, of the Yolngu people is made from eucalyptus wood that termites have partly hollowed out.",
    "Javanese gamelan uses two tuning systems, slendro with five pitches per octave and pelog with seven pitches per octave.",
    "The tabla is a pair of hand drums of contrasting size and pitch, with the higher drum called dayan and the lower called bayan, central to Hindustani music.",
    "The Armenian duduk is a double-reed instrument made from aged apricot wood, and UNESCO inscribed it on its intangible heritage list in 2008.",
    "The mbira is a Shona thumb piano from Zimbabwe, with metal keys plucked on a wooden board, often set inside a gourd resonator.",
    "The oud is a fretless Middle Eastern lute with a pear-shaped body and usually eleven strings grouped in six courses.",
    "The guzheng is a plucked Chinese zither whose movable bridges let players adjust the pitch of each string.",
    "The berimbau is a Brazilian single-string musical bow with a gourd resonator, played by striking the steel string with a stick in capoeira.",
};

constexpr const char* kEmbedCategory[kEmbedCount] = {
    "instruments",
    "instruments",
    "instruments",
    "instruments",
    "instruments",
    "instruments",
    "instruments",
    "instruments",
    "music theory",
    "music theory",
    "music theory",
    "music theory",
    "music theory",
    "music theory",
    "music theory",
    "music theory",
    "music theory",
    "music theory",
    "music theory",
    "music theory",
    "music theory",
    "music theory",
    "music theory",
    "sound design and synthesis",
    "sound design and synthesis",
    "sound design and synthesis",
    "sound design and synthesis",
    "sound design and synthesis",
    "sound design and synthesis",
    "sound design and synthesis",
    "sound design and synthesis",
    "sound design and synthesis",
    "acoustics and hearing",
    "acoustics and hearing",
    "acoustics and hearing",
    "acoustics and hearing",
    "acoustics and hearing",
    "acoustics and hearing",
    "acoustics and hearing",
    "acoustics and hearing",
    "acoustics and hearing",
    "acoustics and hearing",
    "acoustics and hearing",
    "music technology history",
    "music technology history",
    "music technology history",
    "music technology history",
    "music technology history",
    "music technology history",
    "music technology history",
    "music technology history",
    "music technology history",
    "music technology history",
    "music technology history",
    "music technology history",
    "algorithms",
    "algorithms",
    "algorithms",
    "algorithms",
    "algorithms",
    "algorithms",
    "algorithms",
    "algorithms",
    "famous musicians and composers",
    "famous musicians and composers",
    "famous musicians and composers",
    "famous musicians and composers",
    "famous musicians and composers",
    "famous musicians and composers",
    "famous musicians and composers",
    "famous musicians and composers",
    "famous musicians and composers",
    "famous musicians and composers",
    "famous musicians and composers",
    "famous musicians and composers",
    "music of the world",
    "music of the world",
    "music of the world",
    "music of the world",
    "music of the world",
    "music of the world",
    "music of the world",
    "music of the world",
    "music of the world",
    "music of the world",
    "music of the world",
    "music of the world",
    "music of the world",
    "music of the world",
};

constexpr EmbedDemo kEmbedDemo[kEmbedCount] = {
    { Demo::Kind::note, 2, { 21, 108, -1, -1, -1, -1 }, 1800, 0 },  // 0 C0001
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 1 C0002
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 2 C0003
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 3 C0004
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 4 C0005
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 5 C0006
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 6 C0007
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 7 C0008
    { Demo::Kind::interval, 2, { 60, 72, -1, -1, -1, -1 }, 1200, 0 },  // 8 C0009
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 9 C0010
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 10 C0011
    { Demo::Kind::interval, 2, { 60, 64, -1, -1, -1, -1 }, 1200, 0 },  // 11 C0012
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 12 C0013
    { Demo::Kind::note, 1, { 69, -1, -1, -1, -1, -1 }, 900, 0 },  // 13 C0014
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 14 C0015
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 15 C0016
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 16 C0017
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 17 C0018
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 18 C0019
    { Demo::Kind::scale, 6, { 60, 62, 64, 66, 68, 70 }, 2400, 13 },  // 19 C0020
    { Demo::Kind::scale, 5, { 60, 62, 64, 67, 69, -1 }, 2400, 10 },  // 20 C0021
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 21 C0022
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 22 C0023
    { Demo::Kind::note, 1, { 60, -1, -1, -1, -1, -1 }, 900, 0 },  // 23 C0024
    { Demo::Kind::note, 1, { 57, -1, -1, -1, -1, -1 }, 900, 0 },  // 24 C0025
    { Demo::Kind::effect, 1, { 45, -1, -1, -1, -1, -1 }, 4000, 1 },  // 25 C0026
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 26 C0027
    { Demo::Kind::effect, 1, { 45, -1, -1, -1, -1, -1 }, 4000, 3 },  // 27 C0028
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 28 C0029
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 29 C0030
    { Demo::Kind::effect, 1, { 69, -1, -1, -1, -1, -1 }, 4000, 0 },  // 30 C0031
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 31 C0032
    { Demo::Kind::dopplerPass, 2, { 69, 70, -1, -1, -1, -1 }, 2400, 8 },  // 32 C0033
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 33 C0034
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 34 C0035
    { Demo::Kind::beats, 2, { 69, 69, -1, -1, -1, -1 }, 4000, 2 },  // 35 C0036
    { Demo::Kind::chord, 3, { 55, 62, 67, -1, -1, -1 }, 1800, 0 },  // 36 C0037
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 37 C0038
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 38 C0039
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 39 C0040
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 40 C0041
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 41 C0042
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 42 C0043
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 43 C0044
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 44 C0045
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 45 C0046
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 46 C0047
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 47 C0048
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 48 C0049
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 49 C0050
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 50 C0051
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 51 C0052
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 52 C0053
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 53 C0054
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 54 C0055
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 55 C0056
    { Demo::Kind::effect, 3, { 45, 57, 69, -1, -1, -1 }, 6000, 2 },  // 56 C0057
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 57 C0058
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 58 C0059
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 59 C0060
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 60 C0061
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 61 C0062
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 62 C0063
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 63 C0064
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 64 C0065
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 65 C0066
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 66 C0067
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 67 C0068
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 68 C0069
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 69 C0070
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 70 C0071
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 71 C0072
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 72 C0073
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 73 C0074
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 74 C0075
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 75 C0076
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 76 C0077
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 77 C0078
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 78 C0079
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 79 C0080
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 80 C0081
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 81 C0082
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 82 C0083
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 83 C0084
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 84 C0085
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 85 C0086
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 86 C0087
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 87 C0088
    { Demo::Kind::none, 0, { -1, -1, -1, -1, -1, -1 }, 0, 0 },  // 88 C0089
};
