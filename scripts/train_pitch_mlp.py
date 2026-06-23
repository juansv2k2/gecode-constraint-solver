#!/usr/bin/env python3
"""
Phase 1 — Neural Heuristic Integration
Train a tiny context-window MLP pitch scorer on datasets/pitch_<100.txt
and export weights to datasets/pitch_mlp_weights.json.

Usage:
    python3 scripts/train_pitch_mlp.py [--dataset PATH] [--out PATH] [--epochs N]

Network:
    Input  : CONTEXT_SIZE recent MIDI pitches, normalised to [0,1] by /127
    Hidden : HIDDEN_SIZE neurons, ReLU
    Output : 1 scalar (normalised predicted next pitch)
    Loss   : MSE between prediction and true next pitch
    Scoring: At inference time, candidate score = -|prediction - candidate/127|
             Higher score → better candidate.

This is regression only (Phase 1 = plumbing).  Phase 2 will reframe as
a classifier over discrete candidate values.
"""

import argparse
import json
import math
import os
import random
import sys

# ---------------------------------------------------------------------------
# Hyper-parameters (kept deliberately tiny for fast inference in Gecode)
# ---------------------------------------------------------------------------
CONTEXT_SIZE = 4    # Number of preceding pitches fed to the network
HIDDEN_SIZE  = 16   # Neurons in the single hidden layer
LR           = 0.01
EPOCHS       = 300
BATCH_SIZE   = 32
SEED         = 42

# ---------------------------------------------------------------------------
# Tiny numpy-free matrix helpers (pure Python floats)
# ---------------------------------------------------------------------------

def dot(v, w):
    """Dot product of two flat lists."""
    return sum(a * b for a, b in zip(v, w))

def mat_vec(M, v):
    """Matrix-vector product.  M is list-of-rows, each row a list of floats."""
    return [dot(row, v) for row in M]

def relu(v):
    return [max(0.0, x) for x in v]

def relu_grad(v):
    """Derivative of ReLU: 1 if v > 0, else 0."""
    return [1.0 if x > 0 else 0.0 for x in v]

def forward(W1, b1, W2, b2, x):
    """Forward pass. Returns (h_pre, h, y_pred)."""
    h_pre = [dot(W1[j], x) + b1[j] for j in range(len(W1))]
    h = relu(h_pre)
    y = dot(W2, h) + b2
    return h_pre, h, y

# ---------------------------------------------------------------------------
# Dataset loading
# ---------------------------------------------------------------------------

def load_dataset(path):
    """Parse 'MIDI_PITCH RHYTHM_FRACTION' text file. Returns list of int pitches."""
    pitches = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split()
            try:
                p = int(parts[0])
                if 0 <= p <= 127:
                    pitches.append(p)
            except (ValueError, IndexError):
                pass
    return pitches

def build_examples(pitches, context_size):
    """Build (X, y) pairs using only positions where full context is available.
    Training stays pure — no artificial padding."""
    X, y = [], []
    for i in range(context_size, len(pitches)):
        ctx = [pitches[i - context_size + k] / 127.0 for k in range(context_size)]
        target = pitches[i] / 127.0
        X.append(ctx)
        y.append(target)
    return X, y

# ---------------------------------------------------------------------------
# Initialisation
# ---------------------------------------------------------------------------

def glorot_uniform(fan_in, fan_out, rng):
    limit = math.sqrt(6.0 / (fan_in + fan_out))
    return [[rng.uniform(-limit, limit) for _ in range(fan_in)]
            for _ in range(fan_out)]

def zeros(n):
    return [0.0] * n

# ---------------------------------------------------------------------------
# Training (SGD with mini-batches)
# ---------------------------------------------------------------------------

def train(X, y, context_size, hidden_size, lr, epochs, batch_size, rng):
    W1 = glorot_uniform(context_size, hidden_size, rng)   # [hidden][input]
    b1 = zeros(hidden_size)
    W2 = [rng.uniform(-0.1, 0.1) for _ in range(hidden_size)]  # [hidden]
    b2 = 0.0

    n = len(X)
    indices = list(range(n))

    print(f"Training on {n} examples, context={context_size}, hidden={hidden_size}")
    print(f"Epochs={epochs}, LR={lr}, batch={batch_size}")

    for epoch in range(epochs):
        rng.shuffle(indices)
        epoch_loss = 0.0
        num_batches = 0

        for start in range(0, n, batch_size):
            batch_idx = indices[start:start + batch_size]
            if not batch_idx:
                break

            # Gradient accumulators
            dW1 = [[0.0] * context_size for _ in range(hidden_size)]
            db1 = [0.0] * hidden_size
            dW2 = [0.0] * hidden_size
            db2 = 0.0

            batch_loss = 0.0
            for i in batch_idx:
                x_i = X[i]
                y_i = y[i]
                h_pre, h, y_pred = forward(W1, b1, W2, b2, x_i)

                # MSE loss = (y_pred - y_i)^2 / 2  (factor of 2 cancels in grad)
                err = y_pred - y_i
                batch_loss += err * err

                # Backprop output layer
                # dL/dy_pred = err
                for j in range(hidden_size):
                    dW2[j] += err * h[j]
                db2 += err

                # Backprop hidden layer
                # dL/dh_pre[j] = err * W2[j] * relu_grad(h_pre[j])
                rg = relu_grad(h_pre)
                for j in range(hidden_size):
                    delta = err * W2[j] * rg[j]
                    for k in range(context_size):
                        dW1[j][k] += delta * x_i[k]
                    db1[j] += delta

            bs = len(batch_idx)
            epoch_loss += batch_loss / bs

            # SGD update
            for j in range(hidden_size):
                for k in range(context_size):
                    W1[j][k] -= lr * dW1[j][k] / bs
                b1[j] -= lr * db1[j] / bs
                W2[j] -= lr * dW2[j] / bs
            b2 -= lr * db2 / bs
            num_batches += 1

        if (epoch + 1) % 50 == 0 or epoch == 0:
            avg_loss = epoch_loss / max(num_batches, 1)
            rmse = math.sqrt(avg_loss) * 127  # back to MIDI semitones
            print(f"  epoch {epoch+1:4d}/{epochs}  MSE={avg_loss:.6f}  RMSE≈{rmse:.2f} semitones")

    return W1, b1, W2, b2

# ---------------------------------------------------------------------------
# Evaluation
# ---------------------------------------------------------------------------

def evaluate(W1, b1, W2, b2, X, y):
    total = 0.0
    for x_i, y_i in zip(X, y):
        _, _, pred = forward(W1, b1, W2, b2, x_i)
        err = (pred - y_i) * 127  # in semitones
        total += err * err
    rmse = math.sqrt(total / len(X))
    return rmse

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description="Train tiny pitch MLP and export weights")
    ap.add_argument("--dataset", default="datasets/pitch_<100.txt")
    ap.add_argument("--out",     default="datasets/pitch_mlp_weights.json")
    ap.add_argument("--epochs",  type=int, default=EPOCHS)
    ap.add_argument("--context", type=int, default=CONTEXT_SIZE)
    ap.add_argument("--hidden",  type=int, default=HIDDEN_SIZE)
    ap.add_argument("--lr",      type=float, default=LR)
    ap.add_argument("--seed",    type=int, default=SEED)
    args = ap.parse_args()

    # Resolve paths relative to repo root
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    dataset_path = args.dataset if os.path.isabs(args.dataset) else os.path.join(repo_root, args.dataset)
    out_path     = args.out     if os.path.isabs(args.out)     else os.path.join(repo_root, args.out)

    if not os.path.exists(dataset_path):
        print(f"ERROR: dataset not found at {dataset_path}", file=sys.stderr)
        sys.exit(1)

    rng = random.Random(args.seed)

    pitches = load_dataset(dataset_path)
    print(f"Loaded {len(pitches)} pitches from {dataset_path}")
    print(f"  range: {min(pitches)}\u2013{max(pitches)}  mean: {sum(pitches)/len(pitches):.1f}")

    if len(pitches) < args.context + 2:
        print("ERROR: dataset too small for the requested context size", file=sys.stderr)
        sys.exit(1)

    # Sample 512 pitches from dataset for solver-level warm-start (inference only).
    # Stored in weights JSON so the C++ scorer can draw from the real distribution
    # without any artificial padding during training.
    sample_indices = [rng.randint(0, len(pitches) - 1) for _ in range(512)]
    pitch_samples = [pitches[i] for i in sample_indices]

    X, y = build_examples(pitches, args.context)

    # Train/validation split (90/10)
    split = int(0.9 * len(X))
    X_train, y_train = X[:split], y[:split]
    X_val,   y_val   = X[split:], y[split:]

    W1, b1, W2, b2 = train(X_train, y_train,
                            args.context, args.hidden,
                            args.lr, args.epochs,
                            BATCH_SIZE, rng)

    train_rmse = evaluate(W1, b1, W2, b2, X_train, y_train)
    val_rmse   = evaluate(W1, b1, W2, b2, X_val,   y_val)
    print(f"\nFinal RMSE — train: {train_rmse:.2f} semitones  val: {val_rmse:.2f} semitones")

    # Export
    weights = {
        "context_size": args.context,
        "hidden_size":  args.hidden,
        "phase":        1,
        "description":  "Regression MLP pitch scorer trained on folk melody dataset (Phase 1 plumbing)",
        "train_rmse_semitones": round(train_rmse, 3),
        "val_rmse_semitones":   round(val_rmse,   3),
        "dataset":      os.path.basename(dataset_path),
        "num_examples": len(X),
        "pitch_samples": pitch_samples,
        "W1": W1,
        "b1": b1,
        "W2": W2,
        "b2": b2
    }

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w") as f:
        json.dump(weights, f, indent=2)
    print(f"Weights saved to {out_path}")
    print(f"\nNext step: set search_options.value_order = \"neural\" in your config")
    print(f"           and search_options.neural_weights_file = \"{os.path.relpath(out_path, repo_root)}\"")

if __name__ == "__main__":
    main()
