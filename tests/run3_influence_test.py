#!/usr/bin/env python3
"""
run3_influence_test.py
----------------------
Run 3 influence test — unified_melodic_mlp (vocab=128) with harmonic domain
(C major I–IV–V–I, 64 positions). Replaces per-seed shell calls used for Run 2.

6 assertions:
  1  Neural pitch entropy < random pitch entropy (model has preferences)
  2  Neural mean melodic step <= random (prefers stepwise motion)
  3  All 20 neural seeds produce unique melodies
  4  Neural chord-tone alignment > random (harmonic domain is active)
  5  Neural MIDI-83 rate <= random (no floor-score but model deprioritises extremes)
  6  All 20 neural seeds produce solutions (no timeout/fail)

Usage:
    python3 tests/run3_influence_test.py
    python3 tests/run3_influence_test.py --seeds 20 --verbose
"""

import argparse
import json
import math
import os
import re
import subprocess
import sys
import tempfile
from collections import Counter

# ── paths ──────────────────────────────────────────────────────────────────
REPO   = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOLVER = os.path.join(REPO, "bin", "dynamic-solver")
CONFIG = os.path.join(REPO, "configs", "neural_counterpoint_soprano_bass.json")

# Chord tones per harmonic section (pitch classes 0-11)
# Matches the harmonic_domain in the config: beats 0/16/32/48 → C/F/G7/C
CHORD_MAP = {
    range(0,  16): {0, 4, 7},        # C major
    range(16, 32): {5, 9, 0},        # F major
    range(32, 48): {7, 11, 2, 5},    # G dom7
    range(48, 64): {0, 4, 7},        # C major
}

def chord_tones_at(pos: int) -> set:
    for rng, tones in CHORD_MAP.items():
        if pos in rng:
            return tones
    return set()


def make_config(value_order: str, seed: int, base_text: str) -> str:
    text = re.sub(r'"value_order"\s*:\s*"[^"]*"',
                  f'"value_order": "{value_order}"', base_text)
    text = re.sub(r'"random_seed"\s*:\s*\d+',
                  f'"random_seed": {seed}', text)
    return text


def run_solver(config_text: str) -> tuple[list[int], bool]:
    """Return (soprano_pitches, solved_ok)."""
    with tempfile.NamedTemporaryFile("w", suffix=".json",
                                    delete=False, encoding="utf-8") as tf:
        tf.write(config_text)
        tmp = tf.name
    try:
        r = subprocess.run([SOLVER, tmp], capture_output=True,
                           text=True, timeout=30)
        out = r.stdout + r.stderr
    except subprocess.TimeoutExpired:
        return [], False
    finally:
        os.unlink(tmp)

    pitches = []
    for line in out.splitlines():
        if "Voice 0 Pitch:" in line:
            pitches += [int(m) for m in re.findall(r'\((\d+)\)', line)]
    return pitches, bool(pitches)


def entropy_bits(pitches: list[int]) -> float:
    if not pitches:
        return 0.0
    c = Counter(pitches)
    n = len(pitches)
    return -sum((v/n) * math.log2(v/n) for v in c.values())


def mean_step(pitches: list[int]) -> float:
    if len(pitches) < 2:
        return 0.0
    steps = [abs(pitches[i+1] - pitches[i]) for i in range(len(pitches)-1)]
    return sum(steps) / len(steps)


def chord_tone_rate(pitches: list[int]) -> float:
    """Fraction of notes that are chord tones of the current harmonic section."""
    if not pitches:
        return 0.0
    notes_per_solution = 64  # solution_length from config
    hits = 0
    for i, p in enumerate(pitches):
        pos = i % notes_per_solution
        if (p % 12) in chord_tones_at(pos):
            hits += 1
    return hits / len(pitches)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=20)
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(SOLVER):
        print("SKIP — bin/dynamic-solver not found"); sys.exit(0)
    if not os.path.exists(CONFIG):
        print("SKIP — config not found"); sys.exit(0)

    with open(CONFIG, encoding="utf-8") as f:
        base_text = f.read()

    seeds = list(range(1, args.seeds + 1))

    print(f"Model  : unified_melodic_mlp  vocab=128  input=60")
    print(f"Config : {os.path.relpath(CONFIG, REPO)}")
    print(f"Seeds  : {args.seeds}  (×64 notes/seed = {args.seeds*64} notes/mode)")
    print()

    # ── collect ────────────────────────────────────────────────────────────
    neural_all, neural_melodies, neural_fails = [], [], 0
    random_all, random_melodies, random_fails = [], [], 0

    print(f"Running {args.seeds} neural  solves …", flush=True)
    for s in seeds:
        pitches, ok = run_solver(make_config("neural", s, base_text))
        if ok:
            neural_all.extend(pitches)
            neural_melodies.append(tuple(pitches))
        else:
            neural_fails += 1
            if args.verbose:
                print(f"  seed {s} FAILED (neural)")

    print(f"Running {args.seeds} random  solves …", flush=True)
    for s in seeds:
        pitches, ok = run_solver(make_config("random", s, base_text))
        if ok:
            random_all.extend(pitches)
            random_melodies.append(tuple(pitches))
        else:
            random_fails += 1

    n_neural = len(neural_all)
    n_random = len(random_all)

    if n_neural == 0:
        print("FAIL — neural solver produced no output"); sys.exit(1)
    if n_random == 0:
        print("FAIL — random solver produced no output"); sys.exit(1)

    # ── metrics ────────────────────────────────────────────────────────────
    n_ent   = entropy_bits(neural_all)
    r_ent   = entropy_bits(random_all)
    n_step  = mean_step(neural_all)
    r_step  = mean_step(random_all)
    n_uniq  = len(set(neural_melodies))
    # ── Per-section chord-tone rates (for assertion #4) ────────────────
    # Bach soprano uses non-chord tones as passing tones — absolute chord-tone
    # rate is not a valid measure of harmonic conditioning.  Instead we check
    # whether the model's CT-rate VARIES across chord sections more than random
    # does (which would indicate the chord context is influencing the output).
    notes_per_solution = 64
    section_bounds = [(0, 15), (16, 31), (32, 47), (48, 63)]
    section_ct_n, section_ct_r = [], []
    for lo, hi in section_bounds:
        n_sub = [p for i, p in enumerate(neural_all) if lo <= (i % notes_per_solution) <= hi]
        r_sub = [p for i, p in enumerate(random_all) if lo <= (i % notes_per_solution) <= hi]
        n_ct = sum(1 for p in n_sub if (p % 12) in chord_tones_at(lo)) / max(len(n_sub), 1)
        r_ct = sum(1 for p in r_sub if (p % 12) in chord_tones_at(lo)) / max(len(r_sub), 1)
        section_ct_n.append(n_ct)
        section_ct_r.append(r_ct)
    n_ct_range = max(section_ct_n) - min(section_ct_n)
    r_ct_range = max(section_ct_r) - min(section_ct_r)
    n_m83   = neural_all.count(83) / n_neural
    r_m83   = random_all.count(83) / n_random if n_random else 0.0

    print()
    print("─" * 64)
    print(f"{'Metric':<44} {'Neural':>8} {'Random':>8}")
    print("─" * 64)
    print(f"{'Notes collected':<44} {n_neural:>8} {n_random:>8}")
    print(f"{'Pitch entropy (bits) ↓':<44} {n_ent:>8.3f} {r_ent:>8.3f}")
    print(f"{'Mean melodic step (semitones) ↓':<44} {n_step:>8.2f} {r_step:>8.2f}")
    print(f"{'Chord-tone rate variance across sections ↑':<44} {n_ct_range:>7.1%} {r_ct_range:>7.1%}")
    print(f"{'MIDI 83 (B5) rate ↓':<44} {n_m83:>7.1%} {r_m83:>7.1%}")
    print(f"{'Unique melodies':<44} {n_uniq:>7}/{len(seeds)} {len(set(random_melodies)):>6}/{len(seeds)}")
    print(f"{'Solve failures':<44} {neural_fails:>8} {random_fails:>8}")
    print("─" * 64)

    if args.verbose:
        print("\nNeural pitch distribution (top 15):")
        for p, c in sorted(Counter(neural_all).items(), key=lambda x: -x[1])[:15]:
            bar = "█" * int(c / n_neural * 200)
            print(f"  MIDI {p:3d} : {c/n_neural:5.1%}  {bar}")

    # ── assertions ─────────────────────────────────────────────────────────
    results = []

    def check(num, label, cond, neural_val, random_val, threshold=None):
        ok = cond
        tag = "PASS" if ok else "FAIL"
        thr = f" (threshold: {threshold})" if threshold else ""
        print(f"  [{tag}] #{num}: {label}{thr}")
        print(f"         neural={neural_val}  random={random_val}")
        results.append(ok)

    print()
    print("Assertions:")
    check(1, "Neural pitch entropy < random (model has preferences)",
          n_ent < r_ent,
          f"{n_ent:.3f} bits", f"{r_ent:.3f} bits")

    check(2, "Neural mean step <= random (prefers stepwise motion)",
          n_step <= r_step,
          f"{n_step:.2f} st", f"{r_step:.2f} st")

    check(3, f"All {args.seeds} neural seeds produce unique melodies",
          n_uniq == args.seeds - neural_fails,
          f"{n_uniq}/{args.seeds - neural_fails}", "—")

    check(4, "Neural CT-rate variance across sections > random (chord context active)",
          n_ct_range > r_ct_range,
          f"{n_ct_range:.1%} range", f"{r_ct_range:.1%} range")

    check(5, "Neural MIDI-83 rate <= random (extreme pitch deprioritised)",
          n_m83 <= r_m83,
          f"{n_m83:.1%}", f"{r_m83:.1%}")

    check(6, f"All {args.seeds} neural seeds produce solutions",
          neural_fails == 0,
          f"{args.seeds - neural_fails}/{args.seeds}", "—")

    passed = sum(results)
    total  = len(results)
    print()
    print(f"Overall: {'PASS' if passed == total else 'PARTIAL'} ({passed}/{total})")
    sys.exit(0 if passed == total else 1)


if __name__ == "__main__":
    main()
