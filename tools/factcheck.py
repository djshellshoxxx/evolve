#!/usr/bin/env python3
"""Independent check of the Music Facts text dump.

Every expected value here is derived from music theory definitions written in this
file (interval numbers and semitones, scale degree strings, chord tone degrees,
the circle of fifths, equal temperament, the wave equation). Nothing is imported
from or copied from MusicFacts.cpp. The C++ side only supplies the text dump
produced by `MutagenMusicFactsTest --dump` (id<TAB>text).

Usage:
    python3 tools/factcheck.py                      # runs the binary in /tmp/mbuild
    python3 tools/factcheck.py --binary PATH
    python3 tools/factcheck.py --dump FILE          # check a saved dump

Two families are handled by ID rather than by text pattern:
  curated  IDs 27000..27088 (the embedded curated facts). They are skipped by the
           arithmetic checks and counted; each text must equal the JSON entry.
  effects  IDs 29500..29999 (sound-design effects). Each text must match an effect
           template, its numbers are recomputed here, and its effect must be the
           one the ID implies: (id - 29500) % 8, in Fx order.

Exit status is 0 when there are zero mismatches and zero unparsed texts.
"""
import argparse
import glob
import json
import math
import os
import re
import subprocess
import sys
from collections import Counter, defaultdict
from fractions import Fraction

# ----------------------------------------------------------------- theory

EPS = 1e-9   # printed decimals can sit exactly on a rounding tie; allow for float noise
LETTERS = "CDEFGAB"
NATURAL = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
ACCIDENTAL = {"": 0, "#": 1, "##": 2, "b": -1, "bb": -2}
SHARP_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
DEGREE_BASE = {1: 0, 2: 2, 3: 4, 4: 5, 5: 7, 6: 9, 7: 11}

# name -> (generic number, semitones above the root)
INTERVALS = {
    "unison": (1, 0), "minor second": (2, 1), "major second": (2, 2),
    "minor third": (3, 3), "major third": (3, 4), "perfect fourth": (4, 5),
    "augmented fourth": (4, 6), "perfect fifth": (5, 7), "minor sixth": (6, 8),
    "major sixth": (6, 9), "minor seventh": (7, 10), "major seventh": (7, 11),
    "octave": (8, 12), "minor ninth": (9, 13), "major ninth": (9, 14),
    "minor tenth": (10, 15), "major tenth": (10, 16), "perfect eleventh": (11, 17),
    "augmented eleventh": (11, 18), "perfect twelfth": (12, 19),
    "minor thirteenth": (13, 20), "major thirteenth": (13, 21),
    "minor fourteenth": (14, 22), "major fourteenth": (14, 23), "double octave": (15, 24),
}

# quality -> chord tones as (generic number, semitones above root)
CHORDS = {
    "major": [(1, 0), (3, 4), (5, 7)],
    "minor": [(1, 0), (3, 3), (5, 7)],
    "diminished": [(1, 0), (3, 3), (5, 6)],
    "augmented": [(1, 0), (3, 4), (5, 8)],
    "suspended two": [(1, 0), (2, 2), (5, 7)],
    "suspended four": [(1, 0), (4, 5), (5, 7)],
    "major sixth": [(1, 0), (3, 4), (5, 7), (6, 9)],
    "minor sixth": [(1, 0), (3, 3), (5, 7), (6, 9)],
    "dominant seventh": [(1, 0), (3, 4), (5, 7), (7, 10)],
    "major seventh": [(1, 0), (3, 4), (5, 7), (7, 11)],
    "minor seventh": [(1, 0), (3, 3), (5, 7), (7, 10)],
    "half-diminished seventh": [(1, 0), (3, 3), (5, 6), (7, 10)],
    "diminished seventh": [(1, 0), (3, 3), (5, 6), (7, 9)],
    "dominant ninth": [(1, 0), (3, 4), (5, 7), (7, 10), (9, 14)],
    "major ninth": [(1, 0), (3, 4), (5, 7), (7, 11), (9, 14)],
    "minor ninth": [(1, 0), (3, 3), (5, 7), (7, 10), (9, 14)],
    "dominant eleventh": [(1, 0), (3, 4), (5, 7), (7, 10), (11, 17)],
    "dominant thirteenth": [(1, 0), (3, 4), (5, 7), (7, 10), (13, 21)],
    "add nine": [(1, 0), (3, 4), (5, 7), (9, 14)],
    "seven suspended four": [(1, 0), (4, 5), (5, 7), (7, 10)],
    "dominant seven sharp nine": [(1, 0), (3, 4), (5, 7), (7, 10), (9, 15)],
    "dominant seven flat nine": [(1, 0), (3, 4), (5, 7), (7, 10), (9, 13)],
    "augmented seventh": [(1, 0), (3, 4), (5, 8), (7, 10)],
    "minor major seventh": [(1, 0), (3, 3), (5, 7), (7, 11)],
}

# scale name -> scale degrees as written in theory ("b3" = minor third, "#4" = sharp fourth)
SCALES = {
    "major": "1 2 3 4 5 6 7", "dorian": "1 2 b3 4 5 6 b7", "phrygian": "1 b2 b3 4 5 b6 b7",
    "lydian": "1 2 3 #4 5 6 7", "mixolydian": "1 2 3 4 5 6 b7", "aeolian": "1 2 b3 4 5 b6 b7",
    "locrian": "1 b2 b3 4 b5 b6 b7", "harmonic minor": "1 2 b3 4 5 b6 7",
    "melodic minor": "1 2 b3 4 5 6 7", "harmonic major": "1 2 3 4 5 b6 7",
    "major pentatonic": "1 2 3 5 6", "minor pentatonic": "1 b3 4 5 b7",
    "blues": "1 b3 4 b5 5 b7", "whole tone": "1 2 3 #4 #5 #6",
    "dorian flat two": "1 b2 b3 4 5 6 b7", "lydian dominant": "1 2 3 #4 5 6 b7",
    "mixolydian flat six": "1 2 3 4 5 b6 b7", "locrian sharp two": "1 2 b3 4 b5 b6 b7",
    "altered": "1 b2 b3 b4 b5 b6 b7", "phrygian dominant": "1 b2 3 4 5 b6 b7",
    "double harmonic": "1 b2 3 4 5 b6 7", "hungarian minor": "1 2 b3 #4 5 b6 7",
    "neapolitan minor": "1 b2 b3 4 5 b6 7", "neapolitan major": "1 b2 b3 4 5 6 7",
    "enigmatic": "1 b2 3 #4 #5 #6 7", "hirajoshi": "1 2 b3 5 b6", "in sen": "1 b2 4 5 b7",
    "iwato": "1 b2 4 b5 b7", "yo": "1 2 4 5 6", "egyptian": "1 2 4 5 b7",
    "bebop dominant": "1 2 3 4 5 6 b7 7", "bebop major": "1 2 3 4 5 #5 6 7",
    "major blues": "1 2 b3 3 5 6", "persian": "1 b2 3 4 b5 b6 7",
    "prometheus": "1 2 3 #4 6 b7", "tritone": "1 b2 3 b5 5 b7",
}

# instrument -> key the instrument is pitched in (its written C sounds as this key)
INSTRUMENT_KEY = {
    "B-flat clarinet": "Bb", "A clarinet": "A", "E-flat clarinet": "Eb",
    "B-flat bass clarinet": "Bb", "B-flat trumpet": "Bb", "A trumpet": "A", "D trumpet": "D",
    "E-flat trumpet": "Eb", "F trumpet": "F", "B-flat cornet": "Bb", "B-flat flugelhorn": "Bb",
    "E-flat alto saxophone": "Eb", "B-flat tenor saxophone": "Bb", "E-flat baritone saxophone": "Eb",
    "B-flat soprano saxophone": "Bb", "E-flat sopranino saxophone": "Eb",
    "B-flat bass saxophone": "Bb", "F horn": "F", "E-flat horn": "Eb", "B-flat tenor horn": "Bb",
    "English horn": "F", "oboe d'amore": "A", "G clarinet": "G", "F clarinet": "F",
    "alto flute in G": "G", "E-flat alto clarinet": "Eb", "B-flat contrabass clarinet": "Bb",
    "E-flat soprano clarinet": "Eb", "A piccolo trumpet": "A", "B-flat bass trumpet": "Bb",
}

# the circle of fifths from C: pitch classes in order of sharps added (0 .. 6 sharps), then flats
CIRCLE = [(7 * i) % 12 for i in range(12)]

# ------------------------------------------------------------- helpers

NOTE = r"[A-G](?:#{1,2}|b{1,2})?"
NOTE_OCT = NOTE + r"-?\d+"
ROOT = r"[A-G](?:#|b)?"
NOTES = NOTE + r"(?: " + NOTE + r")*"
QUALITY = "(?:" + "|".join(sorted(map(re.escape, CHORDS), key=len, reverse=True)) + ")"
SCALE_NAME = "(?:" + "|".join(sorted(map(re.escape, SCALES), key=len, reverse=True)) + ")"
SIG = r"(?:no sharps or flats|\d+ (?:sharps?|flats?))"


def parse_note(text):
    """(letter index, pitch class, octave or None) of a spelled note."""
    m = re.fullmatch(r"([A-G])(#{1,2}|b{1,2})?(-?\d+)?", text)
    if not m:
        raise ValueError("bad note " + text)
    letter = m.group(1)
    pc = (NATURAL[letter] + ACCIDENTAL[m.group(2) or ""]) % 12
    octave = int(m.group(3)) if m.group(3) is not None else None
    return LETTERS.index(letter), pc, octave


def midi_of(text):
    """MIDI number of a spelled note with octave. Octave follows the letter (Cb4 = B3 = 59)."""
    m = re.fullmatch(r"([A-G])(#{1,2}|b{1,2})?(-?\d+)", text)
    letter, acc, octave = m.group(1), ACCIDENTAL[m.group(2) or ""], int(m.group(3))
    return 12 * (octave + 1) + NATURAL[letter] + acc


def absolute_name(midi):
    return SHARP_NAMES[midi % 12] + str(midi // 12 - 1)


def freq(midi, a4):
    return a4 * 2 ** ((midi - 69) / 12)


def round_half_away(x):
    """Round to nearest, halves away from zero (matches C++ lround)."""
    return int(math.copysign(math.floor(abs(x) + 0.5), x))


def expected_tones(root, tones):
    """(letter index, pitch class) of each tone in a chord or scale built on root."""
    r_letter, r_pc, _ = parse_note(root)
    return [((r_letter + number - 1) % 7, (r_pc + semis) % 12) for number, semis in tones]


def scale_tones(name, root):
    """(letter index, pitch class) of each degree of a named scale, from its degree string."""
    r_letter, r_pc, _ = parse_note(root)
    out = []
    for token in SCALES[name].split():
        m = re.fullmatch(r"([b#]?)(\d)", token)
        acc = {"": 0, "b": -1, "#": 1}[m.group(1)]
        degree = int(m.group(2))
        out.append(((r_letter + degree - 1) % 7, (r_pc + DEGREE_BASE[degree] + acc) % 12))
    return out


def tones_of(notes_text):
    return [parse_note(n)[:2] for n in re.findall(NOTE, notes_text)]


def key_sharps(pc):
    """Sharps (positive) or flats (negative) of the major key with tonic pc."""
    index = CIRCLE.index(pc)
    return index if index <= 6 else index - 12


def parse_sig(text):
    if text == "no sharps or flats":
        return 0
    m = re.fullmatch(r"(\d+) (sharps?|flats?)", text)
    n = int(m.group(1))
    return n if m.group(2).startswith("sharp") else -n


def interval_gap_check(first, second, direction, name):
    """Check that second lies `name` above (or below) first, spelled correctly."""
    if name not in INTERVALS:
        return ["unknown interval name " + name]
    number, semis = INTERVALS[name]
    f_letter, f_pc, f_oct = parse_note(first)
    s_letter, s_pc, s_oct = parse_note(second)
    if direction == "above":
        gap, letters = (s_pc - f_pc) % 12, (s_letter - f_letter) % 7
    else:
        gap, letters = (f_pc - s_pc) % 12, (f_letter - s_letter) % 7
    problems = []
    if f_oct is not None and s_oct is not None:
        f_midi = midi_of(first)
        s_midi = midi_of(second)
        span = s_midi - f_midi if direction == "above" else f_midi - s_midi
        if span != semis:
            problems.append("%s %s %s is %s: octave span %d != %d" % (name, direction, first, second, span, semis))
    if gap != semis % 12:
        problems.append("%s %s %s is %s: pitch gap %d != %d" % (name, direction, first, second, gap, semis % 12))
    if letters != (number - 1) % 7:
        problems.append("%s %s %s is %s: letter steps %d != %d" % (name, direction, first, second, letters, (number - 1) % 7))
    return problems


# ------------------------------------------------------------- family checks
# Each check returns None when the text is not from its family, else a list of problems.

def check_tuning(text):
    patterns = [
        (r"MIDI (\d+) \(([^)]+)\) is ([\d.]+) Hz when A=(\d+)\. Its wavelength in air at 343 m/s is ([\d.]+) m\.", (0, 1, 2, 3, 4)),
        (r"At A=(\d+), ([^ ]+) \(MIDI (\d+)\) vibrates at ([\d.]+) Hz, a wavelength of ([\d.]+) m in air\.", (2, 1, 3, 0, 4)),
        (r"Tuned to A=(\d+), note ([^ ]+) \(MIDI (\d+)\) runs at ([\d.]+) Hz; in air at 343 m/s that is ([\d.]+) m long\.", (2, 1, 3, 0, 4)),
    ]
    for pattern, order in patterns:
        m = re.fullmatch(pattern, text)
        if not m:
            continue
        g = m.groups()
        midi, name, f, ref, wl = int(g[order[0]]), g[order[1]], float(g[order[2]]), int(g[order[3]]), float(g[order[4]])
        problems = []
        if name != absolute_name(midi):
            problems.append("name %s != %s" % (name, absolute_name(midi)))
        exact = freq(midi, ref)
        if abs(f - exact) > 0.005 + EPS:
            problems.append("freq %.2f != %.4f" % (f, exact))
        if abs(wl - 343 / exact) > 0.005 + EPS:
            problems.append("wavelength %.2f != %.4f" % (wl, 343 / exact))
        return problems
    return None


def check_cents(text):
    m = re.fullmatch(r"Moving from A=440 down to A=(\d+) lowers ([^ ]+) \(MIDI (\d+)\) by ([\d.]+) Hz\.", text)
    if m:
        ref, name, midi, d = int(m.group(1)), m.group(2), int(m.group(3)), float(m.group(4))
    else:
        m = re.fullmatch(r"([^ ]+) \(MIDI (\d+)\) sits ([\d.]+) Hz below its A=440 pitch when A is set to (\d+)\.", text)
        if not m:
            return None
        name, midi, d, ref = m.group(1), int(m.group(2)), float(m.group(3)), int(m.group(4))
    problems = []
    if name != absolute_name(midi):
        problems.append("name")
    expected = freq(midi, 440) - freq(midi, ref)
    if abs(d - expected) > 0.005 + EPS:
        problems.append("delta %.2f != %.4f" % (d, expected))
    return problems


def check_piano(text):
    m = re.fullmatch(r"Piano key (\d+) of 88 is ([^ ,]+), sounding at ([\d.]+) Hz when A=440\.", text)
    if not m:
        m = re.fullmatch(r"Key (\d+) on a standard piano is ([^ ,]+), which vibrates at ([\d.]+) Hz\.", text)
    if not m:
        return None
    midi = 20 + int(m.group(1))
    problems = []
    if m.group(2) != absolute_name(midi):
        problems.append("name %s != %s" % (m.group(2), absolute_name(midi)))
    if abs(float(m.group(3)) - freq(midi, 440)) > 0.005 + EPS:
        problems.append("freq")
    return problems


def check_interval(text):
    # Every form puts the root first. Each entry: (pattern, name group, direction, first group, second group);
    # direction is a group index or a fixed word.
    patterns = [
        (r"([A-Za-z ]+?) (above|below) (%s) is (%s)\." % (NOTE, NOTE), 1, 2, 3, 4),
        (r"Up from (%s) by ([a-z ]+): (%s)\." % (NOTE, NOTE), 2, "above", 1, 3),
        (r"Down from (%s) by ([a-z ]+): (%s)\." % (NOTE, NOTE), 2, "below", 1, 3),
        (r"In octave \d+, (?:a|an) ([a-z ]+?) (above|below) (%s) is (%s)\." % (NOTE, NOTE), 1, 2, 3, 4),
        (r"(?:Listen: |Ear check: )(?:a|an) ([a-z ]+?) (above|below) (%s) is (%s)\." % (NOTE_OCT, NOTE_OCT), 1, 2, 3, 4),
        (r"This is (?:a|an) ([a-z ]+?) (above|below) (%s): (%s)\. Listen\." % (NOTE_OCT, NOTE_OCT), 1, 2, 3, 4),
    ]
    for pattern, name_g, dir_g, first_g, second_g in patterns:
        m = re.fullmatch(pattern, text)
        if not m:
            continue
        direction = dir_g if isinstance(dir_g, str) else m.group(dir_g)
        return interval_gap_check(m.group(first_g), m.group(second_g), direction, m.group(name_g).lower())
    m = re.fullmatch(r"Equal temperament: ([a-z ]+) spans (\d+) cents\.", text)
    if m:
        if m.group(1) not in INTERVALS:
            return ["unknown interval " + m.group(1)]
        return [] if int(m.group(2)) == 100 * INTERVALS[m.group(1)][1] else ["cents " + m.group(2)]
    m = re.fullmatch(r"Equal temperament: ([a-z ]+) is a frequency ratio of ([\d.]+) to 1\.", text)
    if m:
        if m.group(1) not in INTERVALS:
            return ["unknown interval " + m.group(1)]
        semis = INTERVALS[m.group(1)][1]
        return [] if abs(float(m.group(2)) - 2 ** (semis / 12)) <= 0.00005 else ["ratio " + m.group(2)]
    return None


def check_chord(text):
    forms = [
        (r"(%s) (%s) chord: (%s)\." % (ROOT, QUALITY, NOTES), (1, 2, 3), None),
        (r"The (%s) (%s) is built from (%s)\." % (ROOT, QUALITY, NOTES), (1, 2, 3), None),
        (r"(%s) (%s), (first|second) inversion: bass (%s), then (%s)\." % (ROOT, QUALITY, NOTE, NOTES), (1, 2, 3), "inv"),
        (r"In octave \d, the (%s) (%s) chord is (%s)\." % (ROOT, QUALITY, NOTES), (1, 2, 3), None),
        (r"This is (?:a|an) (%s) chord on (%s): (%s)\. Listen\." % (QUALITY, ROOT, NOTES), (2, 1, 3), None),
        (r"Listen for the (%s) chord rooted on (%s): (%s)\." % (QUALITY, ROOT, NOTES), (2, 1, 3), None),
    ]
    for pattern, (r_g, q_g, n_g), kind in forms:
        m = re.fullmatch(pattern, text)
        if not m:
            continue
        root, quality = m.group(r_g), m.group(q_g)
        if kind == "inv":
            inv = 1 if m.group(3) == "first" else 2
            expected = expected_tones(root, CHORDS[quality])
            expected = expected[inv:] + expected[:inv]
            claimed = tones_of(m.group(4) + " " + m.group(5))
            claimed_bass = tones_of(m.group(4))
            problems = []
            if claimed != expected:
                problems.append("inversion tones %s != %s" % (claimed, expected))
            if claimed_bass != expected[:1]:
                problems.append("bass")
            return problems
        expected = expected_tones(root, CHORDS[quality])
        claimed = tones_of(m.group(n_g))
        return [] if claimed == expected else ["tones %s != %s" % (claimed, expected)]
    return None


def check_scale(text):
    forms = [
        (r"(%s) (%s) scale: (%s)\." % (ROOT, SCALE_NAME, NOTES), (1, 2, 3), "all"),
        (r"The (%s) (%s) scale runs (%s)\." % (ROOT, SCALE_NAME, NOTES), (1, 2, 3), "all"),
        (r"In octave \d, (%s) (%s) runs (%s)\." % (ROOT, SCALE_NAME, NOTES), (1, 2, 3), "all"),
        (r"Degree (\d+) of (%s) (%s) is (%s)\. Listen\." % (ROOT, SCALE_NAME, NOTE), (2, 3, 4), "one"),
        (r"Listen: note (\d+) of (%s) (%s) is (%s)\." % (ROOT, SCALE_NAME, NOTE), (2, 3, 4), "one"),
    ]
    for pattern, (r_g, n_g, notes_g), kind in forms:
        m = re.fullmatch(pattern, text)
        if not m:
            continue
        if kind == "all":
            root, name = m.group(r_g), m.group(n_g)
            expected = scale_tones(name, root)
            claimed = tones_of(m.group(notes_g))
            return [] if claimed == expected else ["scale %s != %s" % (claimed, expected)]
        degree = int(m.group(1))
        root, name = m.group(2), m.group(3)
        expected = scale_tones(name, root)
        claimed = tones_of(m.group(4))
        if not 1 <= degree <= len(expected):
            return ["degree out of range"]
        return [] if claimed == [expected[degree - 1]] else ["degree %d %s != %s" % (degree, claimed, expected[degree - 1])]
    return None


def check_keysig(text):
    m = re.fullmatch(r"(%s) major key signature: (%s)\." % (ROOT, SIG), text) or \
        re.fullmatch(r"The key of (%s) major uses (%s)\." % (ROOT, SIG), text)
    if m:
        pc = parse_note(m.group(1))[1]
        claimed = parse_sig(m.group(2))
        return [] if claimed == key_sharps(pc) else ["signature %d != %d" % (claimed, key_sharps(pc))]
    m = re.fullmatch(r"(%s) minor shares the signature of (%s) major: (%s)\." % (ROOT, ROOT, SIG), text)
    if m:
        minor_pc = parse_note(m.group(1))[1]
        relative_pc = parse_note(m.group(2))[1]
        problems = []
        if relative_pc != (minor_pc + 3) % 12:
            problems.append("relative major is not a minor third up")
        if parse_sig(m.group(3)) != key_sharps(relative_pc):
            problems.append("signature")
        return problems
    return None


def check_circle(text):
    m = re.fullmatch(r"Circle of fifths, step (\d+) of 12: (%s) up a perfect fifth is (%s)\." % (ROOT, ROOT), text)
    if not m:
        return None
    step = int(m.group(1))
    pc_from = CIRCLE[step - 1]
    pc_to = (pc_from + 7) % 12
    problems = []
    if parse_note(m.group(2))[1] != pc_from:
        problems.append("from pitch class")
    if parse_note(m.group(3))[1] != pc_to:
        problems.append("to pitch class")
    return problems


BEATS = {"half": 2.0, "quarter": 1.0, "eighth": 0.5, "sixteenth": 0.25}
DOTTING = {"": 1.0, "dotted": 1.5, "triplet": 2.0 / 3.0}


def check_bpm(text):
    m = re.fullmatch(r"At (\d+) BPM, a (dotted |triplet )?(half|quarter|eighth|sixteenth) note lasts ([\d.]+) ms\.", text)
    if m:
        bpm, mod, base, ms = int(m.group(1)), (m.group(2) or "").strip(), m.group(3), float(m.group(4))
    else:
        m = re.fullmatch(r"A (dotted |triplet )?(half|quarter|eighth|sixteenth) note at (\d+) BPM is a delay of ([\d.]+) ms\.", text)
        if not m:
            return None
        mod, base, bpm, ms = (m.group(1) or "").strip(), m.group(2), int(m.group(3)), float(m.group(4))
    expected = 60000.0 / bpm * BEATS[base] * DOTTING[mod]
    return [] if abs(ms - expected) <= 0.05 + EPS else ["ms %.1f != %.4f" % (ms, expected)]


def check_harmonic(text):
    m = re.fullmatch(r"Harmonic (\d+) of (%s) \(([\d.]+) Hz\) is ([\d.]+) Hz, nearest (%s) at ([+-]\d+) cents\." % (NOTE_OCT, NOTE_OCT), text)
    if m:
        n, fund_name, fund_f, fn, near_name, cents = int(m.group(1)), m.group(2), float(m.group(3)), float(m.group(4)), m.group(5), int(m.group(6))
    else:
        m = re.fullmatch(r"The harmonic (\d+) of (%s) sounds at ([\d.]+) Hz, closest to (%s) \(([+-]\d+) cents\)\." % (NOTE_OCT, NOTE_OCT), text)
        if not m:
            return None
        n, fund_name, fn, near_name, cents = int(m.group(1)), m.group(2), float(m.group(3)), m.group(4), int(m.group(5))
        fund_f = None
    fund_midi = midi_of(fund_name)
    f0 = freq(fund_midi, 440)
    problems = []
    if fund_f is not None and abs(fund_f - f0) > 0.005 + EPS:
        problems.append("fundamental")
    exact = n * f0
    if abs(fn - exact) > 0.005 + EPS:
        problems.append("partial %.2f != %.4f" % (fn, exact))
    near = round_half_away(69 + 12 * math.log2(exact / 440.0))
    if near_name != absolute_name(near):
        problems.append("nearest %s != %s" % (near_name, absolute_name(near)))
    expected_cents = round_half_away(1200 * math.log2(exact / freq(near, 440)))
    if cents != expected_cents:
        problems.append("cents %d != %d" % (cents, expected_cents))
    return problems


def check_freqwl(text):
    m = re.fullmatch(r"A ([\d.]+) Hz tone has a wavelength of ([\d.]+) m in air and a period of ([\d.]+) ms\.", text) or \
        re.fullmatch(r"Sound at ([\d.]+) Hz: wavelength ([\d.]+) m at 343 m/s, period ([\d.]+) ms\.", text)
    if not m:
        return None
    printed_f = m.group(1)
    grid = [i for i in range(600) if "%.1f" % (20.0 * 1000.0 ** (i / 599.0)) == printed_f]
    if not grid:
        return ["frequency %s not on the 600-point grid" % printed_f]
    f = 20.0 * 1000.0 ** (grid[0] / 599.0)
    problems = []
    if abs(float(m.group(2)) - 343 / f) > 0.0005 + EPS:
        problems.append("wavelength")
    if abs(float(m.group(3)) - 1000 / f) > 0.0005 + EPS:
        problems.append("period")
    return problems


def check_db(text):
    m = re.fullmatch(r"Amplitude times (\d+) changes the level by ([+-][\d.]+) dB\.", text)
    if m:
        expected = 20 * math.log10(int(m.group(1)))
    else:
        m = re.fullmatch(r"Moving (\d+) times farther from a point source changes its level by ([+-][\d.]+) dB \(inverse square\)\.", text)
        if m:
            expected = -20 * math.log10(int(m.group(1)))
        else:
            m = re.fullmatch(r"Power times (\d+) changes the level by ([+-][\d.]+) dB\.", text)
            if not m:
                return None
            expected = 10 * math.log10(int(m.group(1)))
    return [] if abs(float(m.group(2)) - expected) <= 0.005 + EPS else ["dB %s != %.4f" % (m.group(2), expected)]


def check_nyquist(text):
    m = re.fullmatch(r"A (\d+) Hz sample rate has a Nyquist limit of ([\d.]+) Hz; anything above it folds back as aliasing\.", text)
    if m:
        return [] if abs(float(m.group(2)) - int(m.group(1)) / 2) <= 0.05 + EPS else ["nyquist"]
    m = re.fullmatch(r"(\d+)-bit audio: about ([\d.]+) dB signal-to-quantisation-noise for a full-scale sine, 6\.02 dB per bit\.", text)
    if m:
        expected = 6.02 * int(m.group(1)) + 1.76
        return [] if abs(float(m.group(2)) - expected) <= 0.005 + EPS else ["SQNR %s != %.4f" % (m.group(2), expected)]
    m = re.fullmatch(r"A ([\d.]+) Hz tone sampled at 44100 Hz aliases to ([\d.]+) Hz\.", text)
    if m:
        f = float(m.group(1))
        alias = abs(f - 44100 * round_half_away(f / 44100))
        return [] if round_half_away(alias) == int(float(m.group(2))) else ["alias %s != %.0f" % (m.group(2), alias)]
    return None


def check_transpose(text):
    m = re.fullmatch(r"On the (.+), written (%s) sounds as (%s)\." % (NOTE, NOTE), text) or \
        re.fullmatch(r"A (.+) part written (%s) is heard as (%s)\." % (NOTE, NOTE), text)
    if not m:
        return None
    instrument = m.group(1)
    if instrument not in INSTRUMENT_KEY:
        return ["unknown instrument " + instrument]
    key_pc = parse_note(INSTRUMENT_KEY[instrument])[1]
    written_pc = parse_note(m.group(2))[1]
    concert_pc = parse_note(m.group(3))[1]
    return [] if concert_pc == (written_pc + key_pc) % 12 else ["concert %s" % m.group(3)]


def check_doppler(text):
    m = re.fullmatch(r"A 440 Hz source moving toward a listener at ([\d.]+) m/s is heard at ([\d.]+) Hz\.", text)
    if m:
        v, expected = float(m.group(1)), 440 * 343 / (343 - float(m.group(1)))
    else:
        m = re.fullmatch(r"A 440 Hz source moving away at ([\d.]+) m/s is heard at ([\d.]+) Hz\.", text)
        if not m:
            return None
        v, expected = float(m.group(1)), 440 * 343 / (343 + float(m.group(1)))
    return [] if abs(float(m.group(2)) - expected) <= 0.005 + EPS else ["doppler %s != %.4f" % (m.group(2), expected)]


def check_pairs(text):
    m = re.fullmatch(r"From (%s) up to (%s) is (\d+) semitones\." % (NOTE_OCT, NOTE_OCT), text)
    if m:
        expected = midi_of(m.group(2)) - midi_of(m.group(1))
        return [] if expected == int(m.group(3)) else ["semitones %s != %d" % (m.group(3), expected)]
    m = re.fullmatch(r"From (%s) down to (%s) is (\d+) semitones\." % (NOTE_OCT, NOTE_OCT), text)
    if m:
        expected = midi_of(m.group(1)) - midi_of(m.group(2))
        return [] if expected == int(m.group(3)) else ["semitones %s != %d" % (m.group(3), expected)]
    m = re.fullmatch(r"(%s) to (%s), going (up|down), spans (\d+) semitones\." % (NOTE_OCT, NOTE_OCT), text)
    if m:
        if m.group(3) == "up":
            expected = midi_of(m.group(2)) - midi_of(m.group(1))
        else:
            expected = midi_of(m.group(1)) - midi_of(m.group(2))
        return [] if expected == int(m.group(4)) else ["semitones %s != %d" % (m.group(4), expected)]
    return None


# ------------------------------------------------ sound-design effects (IDs 29500..29999)
# Numbers are recomputed from their definitions: a beat is 60 / bpm seconds, so a
# note value of b beats lasts 60000 * b / bpm ms; beats = seconds * bpm / 60;
# octaves = log2(f2 / f1); equal temperament f = 440 * 2 ** ((midi - 69) / 12).
# Whole-number ms use exact rational arithmetic rounded half up; decimals are
# allowed half a printed unit of error.

EFFECT_FIRST = 29500
FX_NAMES = ["gate", "sidechain", "riser", "sweep", "bassDrop", "tapeStop", "echoThrow", "reverseSwell"]
NOTE_VALUES = {   # note value in beats, as a fraction
    "quarter-note": Fraction(1), "dotted quarter-note": Fraction(3, 2), "eighth-note": Fraction(1, 2),
    "half-note": Fraction(2), "triplet eighth-note": Fraction(1, 3),
}
ONE_DP = 0.05 + EPS
TWO_DP = 0.005 + EPS
LABEL = "quarter-note|dotted quarter-note|eighth-note|half-note|triplet eighth-note"


def half_up(q):
    """Nearest integer to a non-negative Fraction, halves up."""
    return math.floor(q + Fraction(1, 2))


def _gate(g):
    bpm, div, ms = int(g["bpm"]), int(g["div"]), int(g["ms"])
    problems = []
    if div not in (4, 8, 16, 32):
        problems.append("division 1/%d" % div)
    expected = half_up(Fraction(60000 * 4, bpm * div))
    if ms != expected:
        problems.append("ms %d != %d" % (ms, expected))
    return problems


def _sidechain(g):
    bpm, ms = int(g["bpm"]), int(g["ms"])
    beat = Fraction(1) if g["unit"] == "quarter" else Fraction(1, 2)
    expected = half_up(Fraction(60000) * beat / bpm)
    return [] if ms == expected else ["ms %d != %d" % (ms, expected)]


def _riser(g):
    bars, bpm, sec = int(g["bars"]), int(g["bpm"]), float(g["sec"])
    problems = []
    if "word" in g and g["word"] != ("bar" if bars == 1 else "bars"):
        problems.append("plural for %d bars" % bars)
    exact = bars * 4 * 60 / bpm
    if abs(sec - exact) > ONE_DP:
        problems.append("seconds %.1f != %.4f" % (sec, exact))
    return problems


def _sweep(g):
    lo, hi, octaves = int(g["from"]), int(g["to"]), float(g["oct"])
    problems = []
    if (lo - 50) % 10 != 0 or not 0 <= (lo - 50) // 10 <= 62:
        problems.append("start %d off the grid" % lo)
    if hi not in (2000, 4000, 8000, 12000, 16000):
        problems.append("end %d off the grid" % hi)
    expected = math.log2(hi / lo)
    if abs(octaves - expected) > ONE_DP:
        problems.append("octaves %.1f != %.4f" % (octaves, expected))
    return problems


def _bass(g):
    m_hi, m_lo = midi_of(g["n1"]), midi_of(g["n2"])
    problems = []
    if absolute_name(m_hi) != g["n1"] or absolute_name(m_lo) != g["n2"]:
        problems.append("note names")
    if abs(float(g["f1"]) - freq(m_hi, 440)) > TWO_DP:
        problems.append("start Hz %s != %.4f" % (g["f1"], freq(m_hi, 440)))
    if abs(float(g["f2"]) - freq(m_lo, 440)) > TWO_DP:
        problems.append("end Hz %s != %.4f" % (g["f2"], freq(m_lo, 440)))
    drop = m_hi - m_lo
    expected = {12: "one octave", 24: "two octaves"}.get(drop, "%d semitones" % drop)
    if g["fall"] != expected:
        problems.append("fall '%s' != '%s'" % (g["fall"], expected))
    return problems


def _tape(g):
    seconds, bpm, beats = float(g["s"]), int(g["bpm"]), float(g["beats"])
    problems = []
    if abs(seconds * 10 - round(seconds * 10)) > EPS or not 1.0 <= seconds <= 3.0:
        problems.append("seconds %s off the grid" % g["s"])
    exact = seconds * bpm / 60
    if abs(beats - exact) > ONE_DP:
        problems.append("beats %.1f != %.4f" % (beats, exact))
    return problems


def _echo(g):
    label, bpm, ms = g["label"], int(g["bpm"]), int(g["ms"])
    problems = []
    if g.get("article") is not None:
        expected_article = "An" if label.startswith("eighth") else "A"
        if g["article"] != expected_article:
            problems.append("article %s before %s" % (g["article"], label))
    expected = half_up(Fraction(60000) * NOTE_VALUES[label] / bpm)
    if ms != expected:
        problems.append("ms %d != %d" % (ms, expected))
    return problems


def _reverse(g):
    tail, bpm, beats = int(g["tail"]), int(g["bpm"]), float(g["beats"])
    exact = tail * bpm / 60
    return [] if abs(beats - exact) <= ONE_DP else ["beats %.1f != %.4f" % (beats, exact)]


# (family name, template, checker). Each template is the whole text.
EFFECT_FORMS = [
    ("gate", r"A gate at 1/(?P<div>\d+) notes at (?P<bpm>\d+) BPM opens and closes every (?P<ms>\d+) ms\.", _gate),
    ("gate", r"Gating 1/(?P<div>\d+) notes at (?P<bpm>\d+) BPM chops the sound every (?P<ms>\d+) ms\.", _gate),
    ("gate", r"At (?P<bpm>\d+) BPM, a gate on 1/(?P<div>\d+) notes switches the signal on and off every (?P<ms>\d+) ms\.", _gate),
    ("sidechain", r"Sidechain compression pumping on every (?P<unit>quarter|eighth) note at (?P<bpm>\d+) BPM ducks the pad every (?P<ms>\d+) ms\.", _sidechain),
    ("sidechain", r"At (?P<bpm>\d+) BPM, a sidechained pad ducks on every (?P<unit>quarter|eighth) note, which is (?P<ms>\d+) ms apart\.", _sidechain),
    ("sidechain", r"Sidechain ducking on each (?P<unit>quarter|eighth) note at (?P<bpm>\d+) BPM repeats every (?P<ms>\d+) ms\.", _sidechain),
    ("riser", r"A riser over (?P<bars>\d+) (?P<word>bars?) at (?P<bpm>\d+) BPM lasts (?P<sec>[\d.]+) s\.", _riser),
    ("riser", r"At (?P<bpm>\d+) BPM, a riser spanning (?P<bars>\d+) (?P<word>bars?) takes (?P<sec>[\d.]+) s to build\.", _riser),
    ("riser", r"Building a (?P<bars>\d+)-bar riser at (?P<bpm>\d+) BPM takes (?P<sec>[\d.]+) seconds\.", _riser),
    ("sweep", r"A filter sweep from (?P<from>\d+) Hz to (?P<to>\d+) Hz covers (?P<oct>[\d.]+) octaves\.", _sweep),
    ("sweep", r"Sweeping a filter from (?P<from>\d+) Hz up to (?P<to>\d+) Hz spans (?P<oct>[\d.]+) octaves\.", _sweep),
    ("sweep", r"A cutoff sweep rising from (?P<from>\d+) Hz to (?P<to>\d+) Hz is (?P<oct>[\d.]+) octaves wide\.", _sweep),
    ("bassDrop", r"A bass drop sliding from (?P<n1>%s) \((?P<f1>[\d.]+) Hz\) down to (?P<n2>%s) \((?P<f2>[\d.]+) Hz\) falls (?P<fall>one octave|two octaves|\d+ semitones)\." % (NOTE_OCT, NOTE_OCT), _bass),
    ("bassDrop", r"In a bass drop, the pitch glides from (?P<n1>%s) \((?P<f1>[\d.]+) Hz\) down to (?P<n2>%s) \((?P<f2>[\d.]+) Hz\), a fall of (?P<fall>one octave|two octaves|\d+ semitones)\." % (NOTE_OCT, NOTE_OCT), _bass),
    ("bassDrop", r"A bass drop glides from (?P<n1>%s) \((?P<f1>[\d.]+) Hz\) to (?P<n2>%s) \((?P<f2>[\d.]+) Hz\): a drop of (?P<fall>one octave|two octaves|\d+ semitones)\." % (NOTE_OCT, NOTE_OCT), _bass),
    ("tapeStop", r"A tape stop slows playback to a halt; if it takes (?P<s>[\d.]+) s from (?P<bpm>\d+) BPM that is (?P<beats>[\d.]+) beats\.", _tape),
    ("tapeStop", r"A tape stop that takes (?P<s>[\d.]+) s from (?P<bpm>\d+) BPM lasts (?P<beats>[\d.]+) beats\.", _tape),
    ("tapeStop", r"Stopping a tape over (?P<s>[\d.]+) s at (?P<bpm>\d+) BPM spans (?P<beats>[\d.]+) beats\.", _tape),
    ("echoThrow", r"(?P<article>A|An) (?P<label>%s) echo at (?P<bpm>\d+) BPM repeats every (?P<ms>\d+) ms\." % LABEL, _echo),
    ("echoThrow", r"Echo throws on (?P<label>%s) timing at (?P<bpm>\d+) BPM repeat every (?P<ms>\d+) ms\." % LABEL, _echo),
    ("reverseSwell", r"A reverse swell of a (?P<tail>\d+) s tail at (?P<bpm>\d+) BPM spans (?P<beats>[\d.]+) beats\.", _reverse),
    ("reverseSwell", r"A (?P<tail>\d+) s reverse swell at (?P<bpm>\d+) BPM builds over (?P<beats>[\d.]+) beats\.", _reverse),
]


def check_effect(text):
    """(family name, problems) for an effect text, or None when no template matches."""
    for name, pattern, checker in EFFECT_FORMS:
        m = re.fullmatch(pattern, text)
        if m:
            return name, checker(m.groupdict())
    return None


def load_curated_texts():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs", "data", "curated_facts.json")
    with open(path, encoding="utf-8") as handle:
        return [entry["text"] for entry in json.load(handle)]


CURATED_FIRST = 27000

FAMILIES = [
    ("tuning", check_tuning), ("cents", check_cents), ("piano", check_piano),
    ("interval", check_interval), ("chord", check_chord), ("scale", check_scale),
    ("keysig", check_keysig), ("circle", check_circle), ("bpm", check_bpm),
    ("harmonic", check_harmonic), ("freq-wavelength", check_freqwl), ("db", check_db),
    ("nyquist", check_nyquist), ("transpose", check_transpose), ("doppler", check_doppler),
    ("pairs", check_pairs),
]


def load_dump(path=None, binary=None):
    if path:
        with open(path, encoding="utf-8") as handle:
            text = handle.read()
    else:
        result = subprocess.run([binary, "--dump"], capture_output=True, text=True, check=True)
        text = result.stdout
    rows = []
    for line in text.splitlines():
        ident, _, body = line.partition("\t")
        rows.append((int(ident), body))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--dump", help="check a saved dump instead of running the binary")
    parser.add_argument("--binary", default=None, help="MutagenMusicFactsTest binary")
    args = parser.parse_args()

    binary = args.binary
    if binary is None and args.dump is None:
        found = sorted(glob.glob("/tmp/mbuild/MutagenMusicFactsTest"))
        if not found:
            print("binary not found; pass --binary or --dump", file=sys.stderr)
            return 2
        binary = found[0]

    rows = load_dump(args.dump, binary)
    checked = Counter()
    mismatched = Counter()
    samples = defaultdict(list)
    unparsed = []
    total = len(rows)
    curated = load_curated_texts()

    for ident, text in rows:
        if "?" in text:
            unparsed.append((ident, text))
            continue
        if CURATED_FIRST <= ident < CURATED_FIRST + len(curated):
            # Curated facts are skipped by the arithmetic checks and counted here.
            checked["curated"] += 1
            expected = curated[ident - CURATED_FIRST]
            if text != expected:
                mismatched["curated"] += 1
                if len(samples["curated"]) < 3:
                    samples["curated"].append("%d: text differs from curated_facts.json" % ident)
            continue
        if EFFECT_FIRST <= ident < EFFECT_FIRST + 500:
            found = check_effect(text)
            if found is None:
                unparsed.append((ident, text))
                continue
            name, problems = found
            wanted = FX_NAMES[(ident - EFFECT_FIRST) % len(FX_NAMES)]
            if name != wanted:
                problems = problems + ["is '%s', the id implies '%s'" % (name, wanted)]
            checked["effects"] += 1
            if problems:
                mismatched["effects"] += 1
                if len(samples["effects"]) < 3:
                    samples["effects"].append("%d: %s -> %s" % (ident, text, "; ".join(problems)))
            continue
        for name, check in FAMILIES:
            problems = check(text)
            if problems is None:
                continue
            checked[name] += 1
            if problems:
                mismatched[name] += 1
                if len(samples[name]) < 3:
                    samples[name].append("%d: %s -> %s" % (ident, text, "; ".join(problems)))
            break
        else:
            unparsed.append((ident, text))

    print("%-16s %8s %10s" % ("family", "checked", "mismatches"))
    for name in ["curated"] + [name for name, _ in FAMILIES] + ["effects"]:
        print("%-16s %8d %10d" % (name, checked[name], mismatched[name]))
    print("%-16s %8d" % ("total ids", total))
    print("%-16s %8d" % ("unparsed", len(unparsed)))
    for name in samples:
        for line in samples[name]:
            print("  MISMATCH", line)
    for ident, text in unparsed[:10]:
        print("  UNPARSED", ident, text)

    bad = sum(mismatched.values()) + len(unparsed)
    print("result: %s" % ("OK, zero mismatches" if bad == 0 else "%d problems" % bad))
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
