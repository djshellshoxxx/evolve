#!/usr/bin/env python3
"""Generate Source/Engine/MusicFactsCurated.inc from docs/data/curated_facts.json.

The output is a checked-in, generated C++ include. It is included inside the
anonymous namespace of Source/Engine/MusicFacts.cpp and holds, for each curated
index (its position in the JSON array, 0..N-1): the text, the category and a
Demo recipe. Regenerate with:

    python3 tools/gen_facts.py

The script refuses (exit 1, nothing written) any file that tools/lint_curated.py
rejects: fewer than two source domains, text outside 60..190 characters,
non-ASCII text, and so on. It also refuses a demo with no recipe in RECIPES, or a
recipe whose kind disagrees with the JSON 'demo' field.

Demo recipes are hand-mapped by curated id. Scale and speed indices are not
typed in from memory: they are looked up by name in the kScales and kSpeeds
tables of MusicFacts.cpp, and the script refuses if a name is missing.
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lint_curated  # noqa: E402  (same directory; shared validation)

ROOT = os.path.join(HERE, "..")
JSON_PATH = os.path.join(ROOT, "docs", "data", "curated_facts.json")
CPP_PATH = os.path.join(ROOT, "Source", "Engine", "MusicFacts.cpp")
OUT_PATH = os.path.join(ROOT, "Source", "Engine", "MusicFactsCurated.inc")

# Fx values, as declared in MusicFacts.h (enum Fx).
FX = {"gate": 0, "sidechain": 1, "riser": 2, "sweep": 3, "bassDrop": 4,
      "tapeStop": 5, "echoThrow": 6, "reverseSwell": 7}

# id -> (Demo::Kind name, notes, durationMs, fxId, why).
# Kind names must match the JSON 'demo' field. Notes are MIDI numbers (21..108).
# fxId is a scale index, an Fx value, a speed index, or a beat rate, as the kind requires.
RECIPES = {
    "C0001": ("note", [21, 108], 1800, 0, "A0 then C8, the two ends of the keyboard"),
    "C0009": ("interval", [60, 72], 1200, 0, "C4 then C5: an octave"),
    "C0012": ("interval", [60, 64], 1200, 0, "C4 then E4: a major third"),
    "C0014": ("note", [69], 900, 0, "A4 at 440 Hz"),
    "C0020": ("scale", [60, 62, 64, 66, 68, 70], 2400, "whole tone", "C whole-tone scale, first six tones"),
    "C0021": ("scale", [60, 62, 64, 67, 69], 2400, "major pentatonic", "C major pentatonic: C D E G A"),
    "C0024": ("note", [60], 900, 0, "C4 (the FM bell timbre is not reproduced by a note)"),
    "C0025": ("note", [57], 900, 0, "A3, the plucked-string note"),
    "C0026": ("effect", [45], 4000, "sidechain", "bass note A2 ducked by a kick pattern"),
    "C0028": ("effect", [45], 4000, "sweep", "filter sweep on a bass note"),
    "C0031": ("effect", [69], 4000, "gate", "vibrato and tremolo are both amplitude or pitch modulation; the gate chops the tone"),
    "C0033": ("dopplerPass", [69, 70], 2400, 8, "siren: 30 m/s is index 8 of kSpeeds"),
    "C0036": ("beats", [69, 69], 4000, 2, "tones at 440 and 442 Hz: 2 Hz beat (second tone fxId Hz above)"),
    # Approximation: MIDI notes cannot place 200, 300 and 400 Hz exactly. G3, D4, G4
    # (196, 293.7, 392 Hz) are the nearest equal-tempered tones; flagged in the report.
    "C0037": ("chord", [55, 62, 67], 1800, 0, "nearest equal-tempered tones to 200, 300 and 400 Hz"),
    "C0057": ("effect", [45, 57, 69], 6000, "riser", "Shepard tone: octave-spaced tones that seem to rise"),
}


def parse_scales(cpp_text):
    """Scale name -> (index, semitone list) from the kScales table in MusicFacts.cpp."""
    block = re.search(r"kScales\[\d+\]\s*=\s*\{(.*?)\n    \};", cpp_text, re.S)
    if not block:
        raise SystemExit("gen_facts: cannot find kScales in MusicFacts.cpp")
    scales = {}
    for index, m in enumerate(re.finditer(r'\{\s*"([^"]+)",\s*(\d+),\s*\{([^}]*)\}', block.group(1))):
        semis = [int(x) for x in m.group(3).split(",") if x.strip()]
        scales[m.group(1)] = (index, semis[: int(m.group(2))])
    return scales


def parse_speeds(cpp_text):
    m = re.search(r"kSpeeds\[\d+\]\s*=\s*\{([^}]*)\}", cpp_text)
    if not m:
        raise SystemExit("gen_facts: cannot find kSpeeds in MusicFacts.cpp")
    return [int(float(x)) for x in m.group(1).split(",") if x.strip()]


def c_escape(text):
    """A C string literal body for an ASCII text. '?' is escaped to dodge trigraphs."""
    out = []
    for ch in text:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "?":
            out.append("\\?")
        elif 32 <= ord(ch) <= 126:
            out.append(ch)
        else:
            raise SystemExit("gen_facts: cannot embed character %r" % ch)
    return '"' + "".join(out) + '"'


def resolve(recipe, scales, speeds):
    kind, notes, duration, fx, _why = recipe
    if kind == "scale":
        if fx not in scales:
            raise SystemExit("gen_facts: scale %r not in kScales" % fx)
        index, semis = scales[fx]
        if notes != [60 + s for s in semis[: len(notes)]]:
            raise SystemExit("gen_facts: notes %s are not the first tones of %r from C4" % (notes, fx))
        return kind, notes, duration, index
    if kind == "effect":
        return kind, notes, duration, FX[fx]
    if kind == "dopplerPass":
        if speeds[fx] != 30:
            raise SystemExit("gen_facts: kSpeeds[%d] is %d, expected 30" % (fx, speeds[fx]))
    return kind, notes, duration, fx


def main():
    with open(JSON_PATH, encoding="utf-8") as handle:
        entries = json.load(handle)

    found = lint_curated.problems(entries)
    if found:
        for line in found:
            print("REFUSED", line, file=sys.stderr)
        print("gen_facts: %d problems, nothing written" % len(found), file=sys.stderr)
        return 1

    with open(CPP_PATH, encoding="utf-8") as handle:
        cpp_text = handle.read()
    scales = parse_scales(cpp_text)
    speeds = parse_speeds(cpp_text)

    used = set()
    rows_text, rows_cat, rows_demo = [], [], []
    for index, entry in enumerate(entries):
        ident, kind_json = entry["id"], entry["demo"]
        if kind_json == "none":
            if ident in RECIPES:
                print("gen_facts: %s has a recipe but demo is 'none'" % ident, file=sys.stderr)
                return 1
            demo = ("none", [], 0, 0)
        else:
            if ident not in RECIPES:
                print("gen_facts: %s (demo %r) has no recipe in RECIPES" % (ident, kind_json), file=sys.stderr)
                return 1
            recipe = RECIPES[ident]
            if recipe[0] != kind_json:
                print("gen_facts: %s recipe kind %r != JSON demo %r" % (ident, recipe[0], kind_json), file=sys.stderr)
                return 1
            if not all(21 <= n <= 108 for n in recipe[1]) or len(recipe[1]) > 6:
                print("gen_facts: %s notes out of range" % ident, file=sys.stderr)
                return 1
            kind, notes, duration, fx_value = resolve(recipe, scales, speeds)
            demo = (kind, notes, duration, fx_value)
            used.add(ident)

        kind, notes, duration, fx_value = demo
        padded = (notes + [-1] * 6)[:6]
        rows_text.append("    %s," % c_escape(entry["text"]))
        rows_cat.append("    %s," % c_escape(entry["category"]))
        rows_demo.append("    { Demo::Kind::%s, %d, { %s }, %d, %d },  // %s %s" % (
            kind, len(notes), ", ".join(str(n) for n in padded), duration, fx_value,
            index, ident))

    unused = sorted(set(RECIPES) - used)
    if unused:
        print("gen_facts: recipes for unknown or non-demo ids: %s" % ", ".join(unused), file=sys.stderr)
        return 1

    count = len(entries)
    body = [
        "// GENERATED FILE. Do not edit by hand.",
        "// Source: docs/data/curated_facts.json (%d entries). Generator: tools/gen_facts.py." % count,
        "// Included inside the anonymous namespace of Source/Engine/MusicFacts.cpp.",
        "// Index i is the i-th JSON entry and is served at fact ID 27000 + i.",
        "",
        "constexpr int kEmbedCount = %d;" % count,
        "",
        "struct EmbedDemo",
        "{",
        "    Demo::Kind kind;",
        "    int noteCount;",
        "    int notes[6];",
        "    int durationMs;",
        "    int fxId;",
        "};",
        "",
        "constexpr const char* kEmbedText[kEmbedCount] = {",
    ] + rows_text + [
        "};",
        "",
        "constexpr const char* kEmbedCategory[kEmbedCount] = {",
    ] + rows_cat + [
        "};",
        "",
        "constexpr EmbedDemo kEmbedDemo[kEmbedCount] = {",
    ] + rows_demo + [
        "};",
        "",
    ]
    with open(OUT_PATH, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(body))
    print("gen_facts: wrote %s (%d entries, %d demonstrable)" % (
        os.path.normpath(OUT_PATH), count, len(used)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
