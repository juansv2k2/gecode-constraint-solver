#!/usr/bin/env python3
"""
Build unified training dataset for the unified melodic MLP.

Sources
-------
1. Bach chorales  — music21 built-in corpus (433 pieces, 4-voice SATB)
2. Folk melodies  — datasets/melody.csv (357 k rows, Weimar Jazz Database)

Output: datasets/unified_training.tsv
Columns (tab-separated):
    voice_id        int   0=soprano/folk  1=alto  2=tenor  3=bass
    mel_id          str   unique melody identifier
    position        int   note index within that melody
    pitch           int   MIDI 0-127
    duration_frac   float duration as fraction of one beat
                          (quarterLength for Bach; duration/beatdur for folk)
    chord_root      int   0-11 (C=0) or -1 (unlabeled)
    chord_quality   int   0=major  1=minor  2=dom7  or -1 (unlabeled)

Chord vocabulary: 36 classes = 12 roots × 3 qualities.
Unlabeled rows (chord_root=-1) train the model with an all-zero chord vector,
valid conditioning that teaches the model to operate without harmonic context.

Usage
-----
    python3 scripts/build_chorale_dataset.py [--verify] [--bach-only] [--folk-only]
    python3 scripts/build_chorale_dataset.py --verify   # build + print stats
"""

import csv
import sys
import argparse
import bisect
from pathlib import Path
from collections import Counter

REPO_ROOT   = Path(__file__).parent.parent
DATASETS    = REPO_ROOT / "datasets"
OUT_PATH    = DATASETS / "unified_training.tsv"
FOLK_CSV    = DATASETS / "melody.csv"

# Voice name -> voice_id mapping (0=soprano, 1=alto, 2=tenor, 3=bass)
VOICE_NAME_MAP = {
    "soprano": 0, "s ": 0,
    "alto":    1, "a ": 1,
    "tenor":   2, "t ": 2,
    "bass":    3, "b ": 3,
}

# music21 chord quality string -> quality index
QUALITY_MAP = {
    "major":             0,
    "minor":             1,
    "dominant-seventh":  2,
}

# Chord vocab: list of (root 0-11, quality_name)
CHORD_VOCAB = [(r, q) for r in range(12) for q in ("major", "minor", "dom7")]

FIELDNAMES = ["voice_id", "mel_id", "position", "pitch",
              "duration_frac", "chord_root", "chord_quality"]

# ---------------------------------------------------------------------------
# Bach chorale extraction
# ---------------------------------------------------------------------------

def _part_voice_id(part):
    """Return voice_id for a music21 Part, or None if unrecognised."""
    name = str(part.partName or part.id or "").lower().strip()
    if name in VOICE_NAME_MAP:
        return VOICE_NAME_MAP[name]
    for key, vid in VOICE_NAME_MAP.items():
        if name.startswith(key.strip()):
            return vid
    return None


def _build_chord_index(score):
    """Return sorted list of (offset_float, root_pc 0-11, quality_idx 0/1/2/-1).

    NOTE: music21's Chord.quality returns the *triad* quality string and reports
    "major" even for complete dominant-seventh chords (G-B-D-F → "major").  The
    QUALITY_MAP lookup therefore never produced quality=2 (dom7).  We now use the
    typed predicate methods instead of the quality string.
    """
    entries = []
    try:
        chordified = score.chordify()
        for c in chordified.flatten().getElementsByClass("Chord"):
            root = c.root()
            if root is None:
                continue
            if c.isDominantSeventh():
                qual_idx = 2
            elif c.isMajorTriad():
                qual_idx = 0
            elif c.isMinorTriad():
                qual_idx = 1
            else:
                qual_idx = -1
            entries.append((float(c.offset), root.pitchClass, qual_idx))
        entries.sort(key=lambda x: x[0])
    except Exception:
        pass
    return entries


def _chord_at(chord_index, offset):
    """Binary-search chord_index for the chord active at offset."""
    if not chord_index:
        return -1, -1
    offsets = [e[0] for e in chord_index]
    idx = bisect.bisect_right(offsets, float(offset)) - 1
    idx = max(idx, 0)
    return chord_index[idx][1], chord_index[idx][2]  # root_pc, qual_idx (int)


def process_bach_chorales(verbose=True):
    from music21 import corpus

    rows = []
    paths = corpus.getComposer("bach")
    n_paths = len(paths)

    for piece_idx, path in enumerate(paths):
        try:
            score = corpus.parse(path)
        except Exception:
            continue

        # Map parts -> voice IDs (keep first occurrence per voice_id)
        voice_parts = {}
        for part in score.parts:
            vid = _part_voice_id(part)
            if vid is not None and vid not in voice_parts:
                voice_parts[vid] = part

        if not voice_parts:
            continue

        chord_index = _build_chord_index(score)
        mel_id = f"bach_{piece_idx}"

        for voice_id, part in voice_parts.items():
            position = 0
            for note in part.flatten().notes:
                # Chord object: take highest pitch
                if hasattr(note, "pitches"):
                    pitch = max(p.midi for p in note.pitches)
                else:
                    pitch = note.pitch.midi

                duration_frac = round(float(note.quarterLength), 4)
                chord_root, chord_quality = _chord_at(chord_index, note.offset)

                rows.append({
                    "voice_id":      voice_id,
                    "mel_id":        mel_id,
                    "position":      position,
                    "pitch":         pitch,
                    "duration_frac": duration_frac,
                    "chord_root":    chord_root,
                    "chord_quality": chord_quality,
                })
                position += 1

        if verbose and (piece_idx + 1) % 50 == 0:
            print(f"  Bach: {piece_idx + 1}/{n_paths} pieces processed…")

    return rows


# ---------------------------------------------------------------------------
# Folk melody extraction
# ---------------------------------------------------------------------------

def process_folk_melodies(verbose=True):
    rows = []
    current_melid = None
    position     = 0

    with open(FOLK_CSV, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            melid = row["melid"]
            if melid != current_melid:
                current_melid = melid
                position = 0

            pitch = int(float(row["pitch"]))

            duration = float(row["duration"])
            try:
                beatdur = float(row["beatdur"])
                if beatdur <= 0:
                    beatdur = 0.5
            except (ValueError, TypeError):
                beatdur = 0.5   # fallback: 120 BPM quarter note = 0.5 s

            duration_frac = round(duration / beatdur, 4)

            rows.append({
                "voice_id":      0,
                "mel_id":        f"folk_{melid}",
                "position":      position,
                "pitch":         pitch,
                "duration_frac": duration_frac,
                "chord_root":    -1,
                "chord_quality": -1,
            })
            position += 1

    if verbose:
        print(f"  Folk: {len(rows):,} notes from {FOLK_CSV.name}")

    return rows


# ---------------------------------------------------------------------------
# TSV writer
# ---------------------------------------------------------------------------

def write_tsv(rows, out_path=OUT_PATH):
    out_path = Path(out_path)
    with open(out_path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=FIELDNAMES, delimiter="\t")
        w.writeheader()
        w.writerows(rows)
    print(f"\nWrote {len(rows):,} rows -> {out_path}")


# ---------------------------------------------------------------------------
# Verification
# ---------------------------------------------------------------------------

def verify(rows):
    SEP = "=" * 60
    print(f"\n{SEP}")
    print("Dataset verification")
    print(SEP)
    print(f"Total notes : {len(rows):,}")

    voice_names = {0: "soprano/folk", 1: "alto", 2: "tenor", 3: "bass"}
    by_voice = {}
    for r in rows:
        by_voice.setdefault(r["voice_id"], []).append(r)

    print(f"\n{'Voice':<18} {'Notes':>8}  {'MIDI range':>11}  {'Chord labeled':>14}")
    print("-" * 56)
    for vid in sorted(by_voice.keys()):
        vrows   = by_voice[vid]
        pitches = [r["pitch"] for r in vrows]
        labeled = sum(1 for r in vrows if r["chord_root"] != -1)
        pct     = 100 * labeled / len(vrows)
        print(f"{voice_names.get(vid, str(vid)):<18} {len(vrows):>8}  "
              f"{min(pitches):>4}–{max(pitches):<5}  "
              f"{labeled:>7} ({pct:.0f}%)")

    all_labeled = sum(1 for r in rows if r["chord_root"] != -1)
    print(f"\nTotal chord labels : {all_labeled:,} / {len(rows):,} "
          f"({100*all_labeled/len(rows):.1f}%)")

    # Duration sanity
    durations = [r["duration_frac"] for r in rows]
    print(f"Duration frac range: {min(durations):.3f} – {max(durations):.3f}")

    # Melody boundary integrity: mel_id groups must be contiguous
    mel_ids_seen = set()
    violations   = 0
    prev_mel     = None
    for r in rows:
        m = r["mel_id"]
        if m != prev_mel:
            if m in mel_ids_seen:
                violations += 1
            mel_ids_seen.add(m)
            prev_mel = m
    status = "OK — no mel_id splits" if violations == 0 else f"WARNING: {violations} split(s)"
    print(f"Melody boundaries  : {status}")
    print(f"Unique melodies    : {len(mel_ids_seen):,}")
    print(SEP + "\n")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Build unified_training.tsv from Bach chorales + folk melodies"
    )
    parser.add_argument("--verify",    action="store_true", help="Print stats after building")
    parser.add_argument("--bach-only", action="store_true", help="Only process Bach chorales")
    parser.add_argument("--folk-only", action="store_true", help="Only process folk melodies")
    parser.add_argument("--out",       default=str(OUT_PATH), help="Output TSV path")
    args = parser.parse_args()

    rows = []

    if not args.folk_only:
        print("Processing Bach chorales (this may take ~30 s)…")
        bach_rows = process_bach_chorales()
        print(f"  -> {len(bach_rows):,} notes from Bach corpus")
        rows.extend(bach_rows)

    if not args.bach_only:
        print("Processing folk melodies…")
        folk_rows = process_folk_melodies()
        rows.extend(folk_rows)

    write_tsv(rows, args.out)

    if args.verify:
        verify(rows)


if __name__ == "__main__":
    main()
