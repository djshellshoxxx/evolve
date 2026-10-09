#!/usr/bin/env python3
"""Validate docs/data/curated_facts.json for the Music Facts embed.

Standalone: needs only Python 3, no build. tools/gen_facts.py runs the same
problems() check before it writes Source/Engine/MusicFactsCurated.inc, so a
JSON file that lints clean always generates.

Rules (each entry):
  - required keys present; ids unique
  - at least two distinct source domains (src1, src2)
  - text 60..190 characters, printable ASCII only, no '?'
  - text unique across the file
  - demo is one of the known kinds; demoSpec is non-empty when demo is not 'none'
  - the file holds at most 2500 entries (IDs 27000..29499; 29500.. are effects)

Usage:
    python3 tools/lint_curated.py [PATH]
Exit status is 0 when the file is clean.
"""
import json
import os
import sys
from urllib.parse import urlparse

DEFAULT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs", "data", "curated_facts.json")

REQUIRED = ("id", "category", "text", "demo", "demoSpec", "src1", "src2")
DEMO_KINDS = {"none", "note", "pianoKey", "interval", "chord", "scale", "octavePair", "keySound",
              "circleOfFifths", "ghostNote", "instrumentColour", "effect", "songMotif",
              "dopplerPass", "beats"}
MIN_LEN, MAX_LEN = 60, 190
MAX_ENTRIES = 2500


def domain_of(url):
    host = urlparse(url).netloc.lower()
    return host[4:] if host.startswith("www.") else host


def problems(entries):
    """Return a list of human-readable problems; empty when the entries are clean."""
    out = []
    if not isinstance(entries, list):
        return ["top level must be a JSON array"]
    if len(entries) > MAX_ENTRIES:
        out.append("%d entries exceed the %d curated slots" % (len(entries), MAX_ENTRIES))
    seen_ids, seen_text = {}, {}
    for index, entry in enumerate(entries):
        where = "entry %d (%s)" % (index, entry.get("id", "?") if isinstance(entry, dict) else "?")
        if not isinstance(entry, dict):
            out.append(where + ": not an object")
            continue
        missing = [key for key in REQUIRED if key not in entry]
        if missing:
            out.append(where + ": missing " + ", ".join(missing))
            continue
        if entry["id"] in seen_ids:
            out.append(where + ": duplicate id, first at entry %d" % seen_ids[entry["id"]])
        seen_ids.setdefault(entry["id"], index)

        text = entry["text"]
        if not isinstance(text, str):
            out.append(where + ": text is not a string")
            continue
        domains = {domain_of(entry["src1"]), domain_of(entry["src2"])}
        domains.discard("")
        if len(domains) < 2:
            out.append(where + ": fewer than two distinct source domains")
        if not (MIN_LEN <= len(text) <= MAX_LEN):
            out.append(where + ": text length %d outside %d..%d" % (len(text), MIN_LEN, MAX_LEN))
        if any(ord(ch) > 126 or (ord(ch) < 32) for ch in text):
            out.append(where + ": text has non-ASCII or control characters")
        if "?" in text:
            out.append(where + ": text contains '?'")
        if text in seen_text:
            out.append(where + ": text duplicates entry %d" % seen_text[text])
        seen_text.setdefault(text, index)

        if entry["demo"] not in DEMO_KINDS:
            out.append(where + ": unknown demo kind %r" % entry["demo"])
        elif entry["demo"] != "none" and not str(entry["demoSpec"]).strip():
            out.append(where + ": demo %r has no demoSpec" % entry["demo"])
    return out


def main(argv):
    path = argv[1] if len(argv) > 1 else DEFAULT_PATH
    with open(path, encoding="utf-8") as handle:
        entries = json.load(handle)
    found = problems(entries)
    for line in found:
        print("PROBLEM", line)
    count = len(entries) if isinstance(entries, list) else 0
    print("%s: %d entries, %d problems" % (path, count, len(found)))
    return 0 if not found else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
