#!/usr/bin/env python3
"""
test_neural_influence.py
------------------------
Verifies that the neural pitch scorer is actively influencing note selection
rather than acting as pure random noise.

Two properties are checked:

  1. OUT-OF-VOCAB REJECTION
     The vocab contains 28 pitches from the Weimar folk dataset.  Two pitches
     in the solver domain (55-81) are NOT in the vocab: 56 and 80.  The
     classifier assigns those a hard floor score of -20, which the Gumbel
     noise from any in-vocab pitch easily beats.  Neural runs must never
     select 56 or 80.  A random baseline must select them at a measurable
     rate (~7%, i.e., 2 out of 27 domain pitches).

  2. DISTRIBUTION BIAS TOWARD TRAINING DATA
     The Weimar folk dataset has a mean pitch of 68.5 (around A4).  The
     domain minimum is G3 (55).  Neural output should have a mean pitch
     closer to 68.5 than either the domain minimum or a random baseline.

Usage:
    python3 scripts/test_neural_influence.py
    python3 scripts/test_neural_influence.py --seeds 50 --notes-per-run 16

Requires:
    bin/dynamic-solver must be built.
    datasets/pitch_mlp_weights.json must exist (classification format).
"""

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile

# ── paths ──────────────────────────────────────────────────────────────────
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOLVER = os.path.join(REPO, "bin", "dynamic-solver")
WEIGHTS = os.path.join(REPO, "datasets", "pitch_mlp_weights.json")
BASE_CONFIG = os.path.join(REPO, "configs", "neural_pitch_folk_16x1.json")

DOMAIN_MIN = 55
DOMAIN_MAX = 81

TRAINING_MEAN = 68.5          # documented mean of the Weimar dataset

# ── helpers ─────────────────────────────────────────────────────────────────

def load_vocab() -> set:
    with open(WEIGHTS) as f:
        w = json.load(f)
    assert w.get("phase") == "classification", \
        "Weights file is not a classification model — retrain first."
    return set(w["pitch_vocab"])


def run_solver(config_text: str) -> list[int]:
    """Run the solver with raw JSON text; return list of pitch MIDI values."""
    with tempfile.NamedTemporaryFile("w", suffix=".json",
                                    delete=False, encoding="utf-8") as tf:
        tf.write(config_text)
        tmp_path = tf.name
    try:
        result = subprocess.run(
            [SOLVER, tmp_path],
            capture_output=True, text=True, timeout=30
        )
        output = result.stdout + result.stderr
    finally:
        os.unlink(tmp_path)

    # Extract MIDI values from lines like: Voice 0 Pitch: A4(69) → G4(67) → …
    pitches = []
    for line in output.splitlines():
        if "Voice 0 Pitch:" in line:
            pitches += [int(m) for m in re.findall(r'\((\d+)\)', line)]
    return pitches


def make_config_text(base_text: str, value_order: str, seed: int) -> str:
    """Patch two fields in the raw JSON text without a full round-trip parse."""
    text = re.sub(
        r'"value_order"\s*:\s*"[^"]*"',
        f'"value_order": "{value_order}"',
        base_text,
    )
    text = re.sub(
        r'"random_seed"\s*:\s*\d+',
        f'"random_seed": {seed}',
        text,
    )
    return text


def collect_pitches(value_order: str, seeds: list[int],
                    base_text: str) -> list[int]:
    """Run solver for every seed, collect all pitch values."""
    all_pitches: list[int] = []
    for seed in seeds:
        cfg_text = make_config_text(base_text, value_order, seed)
        pitches = run_solver(cfg_text)
        all_pitches.extend(pitches)
    return all_pitches


def mean(values: list[int]) -> float:
    return sum(values) / len(values) if values else 0.0


def out_of_vocab_rate(pitches: list[int], vocab: set) -> float:
    if not pitches:
        return 0.0
    oov = sum(1 for p in pitches if p not in vocab)
    return oov / len(pitches)


# ── main test ────────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(description="Test neural pitch influence")
    ap.add_argument("--seeds", type=int, default=30,
                    help="Number of random seeds to test (default: 30)")
    args = ap.parse_args()

    # ── pre-flight checks ──────────────────────────────────────────────────
    for path, label in [(SOLVER, "bin/dynamic-solver"),
                        (WEIGHTS, "datasets/pitch_mlp_weights.json"),
                        (BASE_CONFIG, "configs/neural_pitch_folk_16x1.json")]:
        if not os.path.exists(path):
            print(f"SKIP — {label} not found")
            sys.exit(0)

    vocab = load_vocab()
    oov_in_domain = sorted(p for p in range(DOMAIN_MIN, DOMAIN_MAX + 1)
                            if p not in vocab)
    print(f"Vocab size       : {len(vocab)}")
    print(f"Domain           : {DOMAIN_MIN}–{DOMAIN_MAX} "
          f"({DOMAIN_MAX - DOMAIN_MIN + 1} pitches)")
    print(f"Out-of-vocab in  : {oov_in_domain} "
          f"({len(oov_in_domain)} pitches)")
    print(f"Training mean    : {TRAINING_MEAN}")
    print(f"Seeds            : {args.seeds}")
    print()

    seeds = list(range(1, args.seeds + 1))

    with open(BASE_CONFIG, encoding="utf-8") as f:
        base_text = f.read()

    # ── collect ────────────────────────────────────────────────────────────
    print(f"Running {args.seeds} neural  solves …", flush=True)
    neural_pitches = collect_pitches("neural", seeds, base_text)

    print(f"Running {args.seeds} random  solves …", flush=True)
    random_pitches = collect_pitches("random", seeds, base_text)

    if not neural_pitches:
        print("FAIL — neural solver produced no output")
        sys.exit(1)
    if not random_pitches:
        print("FAIL — random solver produced no output")
        sys.exit(1)

    # ── metrics ────────────────────────────────────────────────────────────
    neural_oov_rate  = out_of_vocab_rate(neural_pitches, vocab)
    random_oov_rate  = out_of_vocab_rate(random_pitches, vocab)
    neural_mean_pitch = mean(neural_pitches)
    random_mean_pitch = mean(random_pitches)

    # Count out-of-vocab pitch occurrences
    neural_oov_counts = {p: neural_pitches.count(p) for p in oov_in_domain}
    random_oov_counts = {p: random_pitches.count(p) for p in oov_in_domain}

    print()
    print("─" * 60)
    print(f"{'Metric':<38} {'Neural':>10} {'Random':>10}")
    print("─" * 60)
    print(f"{'Notes collected':<38} {len(neural_pitches):>10} "
          f"{len(random_pitches):>10}")
    print(f"{'Out-of-vocab rate':<38} {neural_oov_rate:>9.1%} "
          f"{random_oov_rate:>9.1%}")
    for p in oov_in_domain:
        print(f"  {'MIDI ' + str(p) + ' occurrences':<36} "
              f"{neural_oov_counts[p]:>10} {random_oov_counts[p]:>10}")
    print(f"{'Mean pitch (training mean=68.5)':<38} "
          f"{neural_mean_pitch:>10.1f} {random_mean_pitch:>10.1f}")
    print(f"{'|mean − training_mean|':<38} "
          f"{abs(neural_mean_pitch - TRAINING_MEAN):>10.2f} "
          f"{abs(random_mean_pitch - TRAINING_MEAN):>10.2f}")
    print("─" * 60)

    # ── assertions ─────────────────────────────────────────────────────────
    failures = []

    # 1. Neural must never choose out-of-vocab pitches
    if neural_oov_rate > 0:
        failures.append(
            f"Neural chose out-of-vocab pitches ({neural_oov_counts}) "
            f"— Gumbel floor score (-20) should prevent this"
        )

    # 2. Random baseline must choose some out-of-vocab pitches
    #    (verifies the domain actually contains them; expected ~7%)
    if random_oov_rate < 0.01:
        failures.append(
            f"Random baseline chose 0 out-of-vocab pitches "
            f"(expected ~{len(oov_in_domain)}/{DOMAIN_MAX-DOMAIN_MIN+1:.0f} ≈ "
            f"{len(oov_in_domain)/(DOMAIN_MAX-DOMAIN_MIN+1):.0%}); "
            f"check domain config"
        )

    # 3. Neural mean pitch should be closer to training data mean than random
    neural_dist = abs(neural_mean_pitch - TRAINING_MEAN)
    random_dist = abs(random_mean_pitch - TRAINING_MEAN)
    if neural_dist >= random_dist:
        failures.append(
            f"Neural mean pitch ({neural_mean_pitch:.1f}) is not closer to "
            f"training mean ({TRAINING_MEAN}) than random ({random_mean_pitch:.1f}); "
            f"neural_dist={neural_dist:.2f} random_dist={random_dist:.2f}"
        )

    # 4. Neural output must be meaningfully varied across seeds
    notes_per_run = 16  # matches solution_length in BASE_CONFIG
    unique_runs = len(set(
        tuple(neural_pitches[i:i + notes_per_run])
        for i in range(0, len(neural_pitches), notes_per_run)
        if len(neural_pitches[i:i + notes_per_run]) == notes_per_run
    ))
    variety_ratio = unique_runs / args.seeds if args.seeds > 0 else 0
    print(f"{'Unique melodies / seeds':<38} {unique_runs:>4}/{args.seeds:<6} "
          f"({variety_ratio:.0%})")
    if variety_ratio < 0.5:
        failures.append(
            f"Only {unique_runs}/{args.seeds} unique melodies "
            f"({variety_ratio:.0%}) — expected >50% variety across seeds"
        )

    # ── result ─────────────────────────────────────────────────────────────
    print()
    if failures:
        print(f"FAIL ({len(failures)} assertion(s)):")
        for f in failures:
            print(f"  ✗ {f}")
        sys.exit(1)
    else:
        print("PASS — neural network is actively influencing generation:")
        print("  ✓ Out-of-vocab pitches never selected (classifier floor score works)")
        print("  ✓ Random baseline selects out-of-vocab pitches (domain verified)")
        print("  ✓ Neural mean pitch closer to training data than random baseline")
        print(f"  ✓ {unique_runs}/{args.seeds} unique melodies ({variety_ratio:.0%} variety)")


if __name__ == "__main__":
    main()
