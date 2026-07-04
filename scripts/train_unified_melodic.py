#!/usr/bin/env python3
"""
Train the unified melodic MLP.

Input vector (60-dim, all float32):
    [0:8]   pitch context      8 prior pitches / 127.0
    [8:16]  rhythm context     8 prior durations / 4.0  (4.0 = whole note)
    [16:24] voice one-hot      8 slots  (max_voices=8)
    [24:60] chord one-hot      36 slots (12 roots x 3 qualities)
                               all-zero when chord is unlabeled

Output: softmax over 128 MIDI classes (full chromatic)

Architecture: 1 hidden layer, ReLU, cross-entropy loss, cosine LR decay (MLX)

Output: datasets/unified_weights.json
  Compatible with neural_pitch_scorer.hh when model_type == "unified_melodic_mlp"
  Existing pitch_mlp_weights.json (no model_type field) is unaffected.

Usage
-----
    python3 scripts/train_unified_melodic.py [options]
    python3 scripts/train_unified_melodic.py --epochs 3000 --hidden 256
    python3 scripts/train_unified_melodic.py --dataset datasets/unified_training.tsv
"""

import argparse
import csv
import json
import math
import random
import sys
from collections import defaultdict
from pathlib import Path

# ---------------------------------------------------------------------------
# Config defaults
# ---------------------------------------------------------------------------
CONTEXT_SIZE  = 8
HIDDEN_SIZE   = 256
MAX_VOICES    = 8
VOCAB_SIZE    = 128        # full MIDI range
CHORD_ROOTS   = 12
CHORD_QUALS   = 3          # 0=major  1=minor  2=dom7
CHORD_VOCAB   = CHORD_ROOTS * CHORD_QUALS   # 36
INPUT_SIZE    = CONTEXT_SIZE * 2 + MAX_VOICES + CHORD_VOCAB  # 60

RHYTHM_NORM   = 4.0        # whole note = 1.0 after normalisation
PITCH_NORM    = 127.0

LR            = 0.003
EPOCHS        = 5000
BATCH_SIZE    = 64
VAL_SPLIT     = 0.1

REPO_ROOT     = Path(__file__).parent.parent
DEFAULT_DATA  = REPO_ROOT / "datasets" / "unified_training.tsv"
DEFAULT_OUT   = REPO_ROOT / "datasets" / "weights" / "unified_weights.json"

# Per-mode defaults
OUT_BY_MODE = {
    "all":      REPO_ROOT / "datasets" / "weights" / "unified_weights.json",
    "folk":     REPO_ROOT / "datasets" / "weights" / "folk_melodic_weights.json",
    "harmonic": REPO_ROOT / "datasets" / "weights" / "harmonic_weights.json",
}
MODEL_TYPE_BY_MODE = {
    "all":      "unified_melodic_mlp",
    "folk":     "folk_melodic_mlp",
    "harmonic": "unified_melodic_mlp",   # same architecture, different training data
}
USES_HARMONIC_BY_MODE = {
    "all":      False,   # chord conditioning trained mostly dead (94% folk)
    "folk":     False,
    "harmonic": True,
}

# Warm-start pitch per voice (used when fewer than context_size prior notes exist)
VOICE_WARM_PITCH = {0: 68, 1: 62, 2: 57, 3: 50}   # S, A, T, B defaults
DEFAULT_WARM_PITCH = 64
WARM_RHYTHM = 1.0   # quarter note as default rhythm context

# ---------------------------------------------------------------------------
# Build training examples
# ---------------------------------------------------------------------------

def load_dataset(tsv_path):
    """
    Returns list of dicts from the TSV, grouped and sorted by (mel_id, voice_id, position).
    Skips grace notes (duration_frac == 0).
    """
    rows = []
    with open(tsv_path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f, delimiter="\t")
        for row in reader:
            dur = float(row["duration_frac"])
            if dur <= 0.0:
                continue   # skip grace notes
            rows.append({
                "voice_id":      int(row["voice_id"]),
                "mel_id":        row["mel_id"],
                "position":      int(row["position"]),
                "pitch":         int(row["pitch"]),
                "duration_frac": dur,
                "chord_root":    int(row["chord_root"]),
                "chord_quality": int(row["chord_quality"]),
            })
    return rows


def build_examples(rows, context_size=CONTEXT_SIZE):
    """
    Returns (X_list, y_list) where each X is a float list of length INPUT_SIZE
    and each y is the target MIDI pitch class (0-127).
    """
    # Group by (mel_id, voice_id)
    groups = defaultdict(list)
    for r in rows:
        groups[(r["mel_id"], r["voice_id"])].append(r)
    for key in groups:
        groups[key].sort(key=lambda r: r["position"])

    X_list, y_list = [], []

    for (mel_id, voice_id), seq in groups.items():
        warm_pitch  = VOICE_WARM_PITCH.get(voice_id, DEFAULT_WARM_PITCH)
        pitches_so_far   = []
        durations_so_far = []

        for note in seq:
            # --- pitch context ---
            if len(pitches_so_far) >= context_size:
                p_ctx = pitches_so_far[-context_size:]
            else:
                pad  = [warm_pitch] * (context_size - len(pitches_so_far))
                p_ctx = pad + pitches_so_far

            # --- rhythm context ---
            if len(durations_so_far) >= context_size:
                r_ctx = durations_so_far[-context_size:]
            else:
                pad  = [WARM_RHYTHM] * (context_size - len(durations_so_far))
                r_ctx = pad + durations_so_far

            # --- voice one-hot ---
            v_hot = [0.0] * MAX_VOICES
            if 0 <= voice_id < MAX_VOICES:
                v_hot[voice_id] = 1.0

            # --- chord one-hot (36 dims; all-zero if unlabeled) ---
            c_hot = [0.0] * CHORD_VOCAB
            cr, cq = note["chord_root"], note["chord_quality"]
            if 0 <= cr < CHORD_ROOTS and 0 <= cq < CHORD_QUALS:
                c_hot[cr * CHORD_QUALS + cq] = 1.0

            # --- assemble input ---
            x = (
                [p / PITCH_NORM  for p in p_ctx] +
                [d / RHYTHM_NORM for d in r_ctx] +
                v_hot +
                c_hot
            )
            assert len(x) == INPUT_SIZE

            target = note["pitch"]
            if 0 <= target < VOCAB_SIZE:
                X_list.append(x)
                y_list.append(target)

            pitches_so_far.append(note["pitch"])
            durations_so_far.append(note["duration_frac"])

    return X_list, y_list


# ---------------------------------------------------------------------------
# MLX model and training
# ---------------------------------------------------------------------------

def train(X_list, y_list, hidden_size=HIDDEN_SIZE, lr=LR,
          epochs=EPOCHS, batch_size=BATCH_SIZE, val_split=VAL_SPLIT,
          train_label="unified melodic MLP"):
    import mlx.core as mx
    import mlx.nn   as nn
    import mlx.optimizers as optim

    class UnifiedMLP(nn.Module):
        def __init__(self):
            super().__init__()
            self.fc1 = nn.Linear(INPUT_SIZE,   hidden_size)
            self.fc2 = nn.Linear(hidden_size,  VOCAB_SIZE)

        def __call__(self, x):
            return self.fc2(nn.relu(self.fc1(x)))

    def loss_fn(model, X, y):
        return mx.mean(nn.losses.cross_entropy(model(X), y))

    # Split
    n     = len(X_list)
    n_val = max(1, int(n * val_split))
    idx   = list(range(n))
    random.shuffle(idx)
    val_idx  = idx[:n_val]
    tr_idx   = idx[n_val:]

    X_tr = [X_list[i] for i in tr_idx]
    y_tr = [y_list[i] for i in tr_idx]
    X_va = [X_list[i] for i in val_idx]
    y_va = [y_list[i] for i in val_idx]

    X_tr_mx = mx.array(X_tr, dtype=mx.float32)
    y_tr_mx = mx.array(y_tr, dtype=mx.int32)
    X_va_mx = mx.array(X_va, dtype=mx.float32)
    y_va_mx = mx.array(y_va, dtype=mx.int32)

    model    = UnifiedMLP()
    optimizer = optim.SGD(learning_rate=lr)
    loss_and_grad = nn.value_and_grad(model, loss_fn)

    n_batches = max(1, len(X_tr) // batch_size)

    print(f"\nTraining {train_label}")
    print(f"  Input:   {INPUT_SIZE}-dim  (pitch×{CONTEXT_SIZE} + rhythm×{CONTEXT_SIZE} "
          f"+ voice×{MAX_VOICES} + chord×{CHORD_VOCAB})")
    print(f"  Hidden:  {hidden_size}  Output: {VOCAB_SIZE} classes")
    print(f"  Train:   {len(X_tr):,}  Val: {len(X_va):,}  Epochs: {epochs}")

    for epoch in range(epochs):
        lr_t = lr * 0.5 * (1.0 + math.cos(math.pi * epoch / epochs))
        optimizer.learning_rate = lr_t

        # Shuffle training data each epoch
        perm = list(range(len(X_tr)))
        random.shuffle(perm)

        epoch_loss = 0.0
        for b in range(n_batches):
            batch_idx = perm[b * batch_size: (b + 1) * batch_size]
            X_b = X_tr_mx[batch_idx]
            y_b = y_tr_mx[batch_idx]
            loss, grads = loss_and_grad(model, X_b, y_b)
            optimizer.update(model, grads)
            mx.eval(model.parameters(), optimizer.state)
            epoch_loss += loss.item()

        if (epoch + 1) % 500 == 0 or epoch == 0:
            tr_loss, tr_acc = evaluate(model, X_tr_mx, y_tr_mx)
            va_loss, va_acc = evaluate(model, X_va_mx, y_va_mx)
            print(f"  Epoch {epoch+1:>5}/{epochs}  "
                  f"train CE={tr_loss:.4f} acc={tr_acc:.1f}%  "
                  f"val CE={va_loss:.4f} acc={va_acc:.1f}%")

    final_tr_loss, final_tr_acc = evaluate(model, X_tr_mx, y_tr_mx)
    final_va_loss, final_va_acc = evaluate(model, X_va_mx, y_va_mx)
    return model, final_tr_loss, final_tr_acc, final_va_loss, final_va_acc


def evaluate(model, X_mx, y_mx):
    import mlx.core as mx
    import mlx.nn   as nn
    logits = model(X_mx)
    mx.eval(logits)
    loss   = mx.mean(nn.losses.cross_entropy(logits, y_mx)).item()
    preds  = mx.argmax(logits, axis=1)
    acc    = mx.sum(preds == y_mx).item() / len(y_mx) * 100.0
    return loss, acc


# ---------------------------------------------------------------------------
# Weights export
# ---------------------------------------------------------------------------

CHORD_VOCAB_LABELS = [
    (r, q)
    for r in range(CHORD_ROOTS)
    for q in ("major", "minor", "dom7")
]

def export_weights(model, out_path, tsv_path,
                   tr_loss, tr_acc, va_loss, va_acc,
                   model_type="unified_melodic_mlp",
                   uses_harmonic_conditioning=False):
    import mlx.core as mx

    # Materialise weight tensors
    W1 = model.fc1.weight.tolist()   # [hidden x input]
    b1 = model.fc1.bias.tolist()
    W2 = model.fc2.weight.tolist()   # [vocab  x hidden]
    b2 = model.fc2.bias.tolist()

    # Collect pitch_samples (for C++ warm-start) — first 500 non-folk pitches
    # Re-read a small portion of the dataset to get representative pitches
    samples = []
    with open(tsv_path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f, delimiter="\t")
        for row in reader:
            if len(samples) >= 500:
                break
            p = int(float(row["pitch"]))
            if 0 <= p < 128:
                samples.append(p)

    payload = {
        "model_type":               model_type,
        "context_size":             CONTEXT_SIZE,
        "hidden_size":              HIDDEN_SIZE,
        "vocab_size":               VOCAB_SIZE,
        "input_size":               INPUT_SIZE,
        "max_voices":               MAX_VOICES,
        "chord_vocab_size":         CHORD_VOCAB,
        "chord_vocab":              [[r, q] for r, q in CHORD_VOCAB_LABELS],
        "pitch_norm":               PITCH_NORM,
        "rhythm_norm":              RHYTHM_NORM,
        "uses_harmonic_conditioning": uses_harmonic_conditioning,
        "train_ce_loss":            round(tr_loss, 4),
        "val_ce_loss":              round(va_loss, 4),
        "train_acc":                round(tr_acc, 2),
        "val_acc":                  round(va_acc, 2),
        "dataset":                  str(tsv_path),
        "pitch_samples":            samples,
        "W1": W1,
        "b1": b1,
        "W2": W2,
        "b2": b2,
    }

    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w") as f:
        json.dump(payload, f)

    size_mb = out_path.stat().st_size / 1_048_576
    print(f"\nWeights saved -> {out_path}  ({size_mb:.1f} MB)")
    print(f"  train CE={tr_loss:.4f}  acc={tr_acc:.1f}%")
    print(f"  val   CE={va_loss:.4f}  acc={va_acc:.1f}%")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Train unified melodic MLP")
    parser.add_argument("--dataset", default=str(DEFAULT_DATA),
                        help="Path to unified_training.tsv")
    parser.add_argument("--out",     default=str(DEFAULT_OUT),
                        help="Output weights JSON path")
    parser.add_argument("--epochs",  type=int,   default=EPOCHS)
    parser.add_argument("--hidden",  type=int,   default=HIDDEN_SIZE)
    parser.add_argument("--lr",      type=float, default=LR)
    parser.add_argument("--batch",   type=int,   default=BATCH_SIZE)
    parser.add_argument("--max-examples", type=int, default=0,
                        help="Cap training examples (0=no cap; use for quick tests)")
    parser.add_argument("--mode", choices=["all", "folk", "harmonic"], default="all",
                        help="Training data filter: all=full dataset, folk=unlabeled only, "
                             "harmonic=chord-labeled Bach only (default: all)")
    args = parser.parse_args()

    # Resolve output path: explicit --out overrides per-mode default
    if args.out == str(DEFAULT_OUT) and args.mode != "all":
        args.out = str(OUT_BY_MODE[args.mode])
    model_type              = MODEL_TYPE_BY_MODE[args.mode]
    uses_harmonic_cond      = USES_HARMONIC_BY_MODE[args.mode]

    tsv_path = Path(args.dataset)
    if not tsv_path.exists():
        sys.exit(f"Dataset not found: {tsv_path}\n"
                 "Run:  python3 scripts/build_chorale_dataset.py  first.")

    print(f"Mode: {args.mode}  model_type={model_type}  "
          f"uses_harmonic_conditioning={uses_harmonic_cond}")
    print(f"Loading {tsv_path} …")
    rows = load_dataset(tsv_path)
    print(f"  {len(rows):,} notes loaded (grace notes skipped)")

    # Filter by mode
    if args.mode == "folk":
        rows = [r for r in rows if r["chord_root"] < 0]
        print(f"  {len(rows):,} folk (unlabeled chord) rows kept")
    elif args.mode == "harmonic":
        rows = [r for r in rows if r["chord_root"] >= 0]
        print(f"  {len(rows):,} chord-labeled (Bach) rows kept")
    if not rows:
        sys.exit("No rows after filter — check --mode and dataset.")

    print("Building training examples …")
    X_list, y_list = build_examples(rows, CONTEXT_SIZE)
    print(f"  {len(X_list):,} examples  (input dim = {INPUT_SIZE})")

    if args.max_examples and len(X_list) > args.max_examples:
        idx = random.sample(range(len(X_list)), args.max_examples)
        X_list = [X_list[i] for i in idx]
        y_list = [y_list[i] for i in idx]
        print(f"  Capped to {len(X_list):,} examples (--max-examples)")

    train_label = {"all": "unified melodic MLP", "folk": "folk melodic MLP",
                   "harmonic": "harmonic melodic MLP (Bach)"}[args.mode]
    model, tr_loss, tr_acc, va_loss, va_acc = train(
        X_list, y_list,
        hidden_size=args.hidden,
        lr=args.lr,
        epochs=args.epochs,
        batch_size=args.batch,
        train_label=train_label,
    )

    export_weights(model, args.out, tsv_path,
                   tr_loss, tr_acc, va_loss, va_acc,
                   model_type=model_type,
                   uses_harmonic_conditioning=uses_harmonic_cond)


if __name__ == "__main__":
    main()
