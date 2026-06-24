#!/usr/bin/env python3
"""
Neural Pitch Scorer — Classification MLP
Train a context-window MLP as a classifier over the pitch vocabulary.

Usage:
    python3 scripts/train_pitch_mlp.py [--dataset PATH] [--out PATH] [--epochs N]

Network:
    Input  : CONTEXT_SIZE recent MIDI pitches, normalised to [0,1] by /127
    Hidden : HIDDEN_SIZE neurons, ReLU
    Output : softmax over pitch_vocab (all unique MIDI values in training data)
    Loss   : cross-entropy

Scoring at inference:
    score(candidate | context) = log P(candidate | context)
    = log softmax(logits / T)[vocab_idx(candidate)]

    T = neural_temperature in config
    T=1.0  → raw trained probabilities (default, recommended)
    T<1.0  → sharper distribution (more greedy / folk-like)
    T>1.0  → flatter distribution (more varied)

With temperature=1.0 and no noise, the solver picks the statistically most
likely transition at each position → melodies that naturally resemble the
training data without any random noise.
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
HIDDEN_SIZE  = 32   # Neurons in the single hidden layer (larger for classifier)
LR           = 0.005
EPOCHS       = 600
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

def softmax_fn(logits):
    max_l = max(logits)
    exps  = [math.exp(l - max_l) for l in logits]
    total = sum(exps)
    return [e / total for e in exps]

def forward_clf(W1, b1, W2, b2, x):
    """Forward pass for classifier.
    W1: [hidden x input], W2: [vocab x hidden], b2: [vocab]
    Returns (h_pre, h, logits)."""
    h_pre  = [dot(W1[j], x) + b1[j] for j in range(len(W1))]
    h      = relu(h_pre)
    logits = [dot(W2[k], h) + b2[k] for k in range(len(W2))]
    return h_pre, h, logits

# ---------------------------------------------------------------------------
# Dataset loading
# ---------------------------------------------------------------------------

def build_examples(pitches, context_size, vocab):
    """Build (X, y_idx) classification pairs.
    y_idx is the index into vocab for the target pitch.
    Positions whose target is outside vocab are skipped."""
    pitch_to_idx = {p: i for i, p in enumerate(vocab)}
    X, y_idx = [], []
    for i in range(context_size, len(pitches)):
        target = pitches[i]
        if target not in pitch_to_idx:
            continue
        ctx = [pitches[i - context_size + k] / 127.0 for k in range(context_size)]
        X.append(ctx)
        y_idx.append(pitch_to_idx[target])
    return X, y_idx


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
# Training (SGD, cross-entropy loss)
# ---------------------------------------------------------------------------

def train(X, y_idx, vocab_size, context_size, hidden_size, lr, epochs, batch_size, rng):
    W1 = glorot_uniform(context_size, hidden_size, rng)   # [hidden x input]
    b1 = zeros(hidden_size)
    W2 = glorot_uniform(hidden_size, vocab_size, rng)     # [vocab x hidden]
    b2 = zeros(vocab_size)

    n       = len(X)
    indices = list(range(n))

    print(f"Training classifier on {n} examples  vocab={vocab_size}  ctx={context_size}  hidden={hidden_size}")
    print(f"Epochs={epochs}  LR={lr}  batch={batch_size}")

    for epoch in range(epochs):
        rng.shuffle(indices)
        epoch_loss  = 0.0
        num_batches = 0

        for start in range(0, n, batch_size):
            batch_idx = indices[start:start + batch_size]
            if not batch_idx:
                break

            dW1 = [[0.0] * context_size for _ in range(hidden_size)]
            db1 = [0.0] * hidden_size
            dW2 = [[0.0] * hidden_size for _ in range(vocab_size)]
            db2 = [0.0] * vocab_size

            batch_loss = 0.0
            for i in batch_idx:
                h_pre, h, logits = forward_clf(W1, b1, W2, b2, X[i])
                probs  = softmax_fn(logits)
                target = y_idx[i]
                batch_loss += -math.log(max(probs[target], 1e-15))

                # dL/dlogit[k] = probs[k] - 1(k==target)
                dlogits = list(probs)
                dlogits[target] -= 1.0

                for k in range(vocab_size):
                    for j in range(hidden_size):
                        dW2[k][j] += dlogits[k] * h[j]
                    db2[k] += dlogits[k]

                rg = relu_grad(h_pre)
                for j in range(hidden_size):
                    dh    = sum(dlogits[k] * W2[k][j] for k in range(vocab_size))
                    delta = dh * rg[j]
                    for kk in range(context_size):
                        dW1[j][kk] += delta * X[i][kk]
                    db1[j] += delta

            bs = len(batch_idx)
            epoch_loss += batch_loss / bs

            for j in range(hidden_size):
                for kk in range(context_size):
                    W1[j][kk] -= lr * dW1[j][kk] / bs
                b1[j] -= lr * db1[j] / bs
            for k in range(vocab_size):
                for j in range(hidden_size):
                    W2[k][j] -= lr * dW2[k][j] / bs
                b2[k] -= lr * db2[k] / bs
            num_batches += 1

        if (epoch + 1) % 100 == 0 or epoch == 0:
            avg_loss = epoch_loss / max(num_batches, 1)
            print(f"  epoch {epoch+1:4d}/{epochs}  CE={avg_loss:.4f}")

    return W1, b1, W2, b2

# ---------------------------------------------------------------------------
# Evaluation
# ---------------------------------------------------------------------------

def evaluate(W1, b1, W2, b2, X, y_idx, vocab_size):
    total_loss = 0.0
    correct    = 0
    for x_i, t in zip(X, y_idx):
        _, _, logits = forward_clf(W1, b1, W2, b2, x_i)
        probs = softmax_fn(logits)
        total_loss += -math.log(max(probs[t], 1e-15))
        if probs.index(max(probs)) == t:
            correct += 1
    n = len(X)
    return total_loss / n, correct / n * 100.0

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
    print(f"  range: {min(pitches)}-{max(pitches)}  mean: {sum(pitches)/len(pitches):.1f}")

    # Vocabulary = all unique pitches in training data (sorted)
    vocab      = sorted(set(pitches))
    vocab_size = len(vocab)
    print(f"  vocab: {vocab_size} unique pitches {vocab[:5]}...{vocab[-5:]}")

    if len(pitches) < args.context + 2:
        print("ERROR: dataset too small", file=sys.stderr)
        sys.exit(1)

    # Warm-start samples: used by C++ to fill missing context slots at inference.
    sample_indices = [rng.randint(0, len(pitches) - 1) for _ in range(512)]
    pitch_samples  = [pitches[i] for i in sample_indices]

    X, y_idx = build_examples(pitches, args.context, vocab)
    print(f"  examples: {len(X)} (skipped {len(pitches) - args.context - len(X)} out-of-vocab)")

    split             = int(0.9 * len(X))
    X_train, y_train  = X[:split],     y_idx[:split]
    X_val,   y_val    = X[split:],     y_idx[split:]

    W1, b1, W2, b2 = train(X_train, y_train, vocab_size,
                            args.context, args.hidden,
                            args.lr, args.epochs, BATCH_SIZE, rng)

    train_loss, train_acc = evaluate(W1, b1, W2, b2, X_train, y_train, vocab_size)
    val_loss,   val_acc   = evaluate(W1, b1, W2, b2, X_val,   y_val,   vocab_size)
    print(f"\nFinal — train: CE={train_loss:.4f} acc={train_acc:.1f}%"
          f"  val: CE={val_loss:.4f} acc={val_acc:.1f}%")

    weights = {
        "phase":        "classification",
        "context_size": args.context,
        "hidden_size":  args.hidden,
        "vocab_size":   vocab_size,
        "pitch_vocab":  vocab,
        "description":  "Classification MLP pitch scorer (cross-entropy, folk melody dataset)",
        "train_ce_loss": round(train_loss, 4),
        "val_ce_loss":   round(val_loss,   4),
        "train_acc":     round(train_acc,  2),
        "val_acc":       round(val_acc,    2),
        "dataset":       os.path.basename(dataset_path),
        "num_examples":  len(X),
        "pitch_samples": pitch_samples,
        "W1": W1,
        "b1": b1,
        "W2": W2,   # [vocab_size x hidden_size]
        "b2": b2    # [vocab_size]
    }

    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w") as f:
        json.dump(weights, f, indent=2)
    print(f"Weights saved to {out_path}")
    print(f"  set neural_temperature: 1.0 in config (T=1.0 = trained probabilities as-is)")
    print(f"  T<1.0 = sharper (more folk-like), T>1.0 = more varied")

if __name__ == "__main__":
    main()
