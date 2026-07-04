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

import mlx.core as mx
import mlx.nn as nn
import mlx.optimizers as optim

# ---------------------------------------------------------------------------
# Hyper-parameters
# ---------------------------------------------------------------------------
CONTEXT_SIZE = 8    # Number of preceding pitches fed to the network
HIDDEN_SIZE  = 256  # Neurons in the single hidden layer
LR           = 0.003
EPOCHS       = 5000
BATCH_SIZE   = 32
SEED         = 42

# ---------------------------------------------------------------------------
# Model
# ---------------------------------------------------------------------------

class PitchMLP(nn.Module):
    """Single hidden-layer MLP for pitch classification."""
    def __init__(self, input_size, hidden_size, vocab_size):
        super().__init__()
        self.fc1 = nn.Linear(input_size, hidden_size)
        self.fc2 = nn.Linear(hidden_size, vocab_size)

    def __call__(self, x):
        return self.fc2(nn.relu(self.fc1(x)))

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
# Training
# ---------------------------------------------------------------------------

def loss_fn(model, X, y):
    """Cross-entropy loss for classification."""
    return mx.mean(nn.losses.cross_entropy(model(X), y))

def train(X_list, y_list, vocab_size, context_size, hidden_size, lr, epochs, batch_size, rng):
    model     = PitchMLP(context_size, hidden_size, vocab_size)
    optimizer = optim.SGD(learning_rate=lr)

    X_mx = mx.array(X_list, dtype=mx.float32)
    y_mx = mx.array(y_list, dtype=mx.int32)
    n    = len(X_list)

    loss_and_grad = nn.value_and_grad(model, loss_fn)

    print(f"Training classifier on {n} examples  vocab={vocab_size}  ctx={context_size}  hidden={hidden_size}")
    print(f"Epochs={epochs}  LR={lr} (cosine decay)  batch={batch_size}  backend=MLX")

    indices = list(range(n))

    for epoch in range(epochs):
        lr_t = lr * 0.5 * (1.0 + math.cos(math.pi * epoch / epochs))
        optimizer.learning_rate = lr_t
        rng.shuffle(indices)

        epoch_loss  = 0.0
        num_batches = 0

        for start in range(0, n, batch_size):
            batch_idx = indices[start:start + batch_size]
            if not batch_idx:
                break
            X_batch = X_mx[batch_idx]
            y_batch = y_mx[batch_idx]

            loss, grads = loss_and_grad(model, X_batch, y_batch)
            optimizer.update(model, grads)
            mx.eval(model.parameters(), optimizer.state)

            epoch_loss  += loss.item()
            num_batches += 1

        if (epoch + 1) % 100 == 0 or epoch == 0:
            avg_loss = epoch_loss / max(num_batches, 1)
            print(f"  epoch {epoch+1:4d}/{epochs}  CE={avg_loss:.4f}  lr={lr_t:.5f}")

    return model

# ---------------------------------------------------------------------------
# Evaluation
# ---------------------------------------------------------------------------

def evaluate(model, X, y_idx):
    X_mx   = mx.array(X,     dtype=mx.float32)
    y_mx   = mx.array(y_idx, dtype=mx.int32)
    logits = model(X_mx)
    mx.eval(logits)
    loss    = mx.mean(nn.losses.cross_entropy(logits, y_mx)).item()
    preds   = mx.argmax(logits, axis=1)
    correct = mx.sum(preds == y_mx).item()
    return loss, correct / len(y_idx) * 100.0

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description="Train tiny pitch MLP and export weights")
    ap.add_argument("--dataset", default="datasets/pitch_<100.txt")
    ap.add_argument("--out",     default="datasets/weights/pitch_mlp_weights.json")
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
    mx.random.seed(args.seed)

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

    model = train(X_train, y_train, vocab_size,
                  args.context, args.hidden,
                  args.lr, args.epochs, BATCH_SIZE, rng)

    train_loss, train_acc = evaluate(model, X_train, y_train)
    val_loss,   val_acc   = evaluate(model, X_val,   y_val)
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
        "W1": model.fc1.weight.tolist(),   # [hidden_size x context_size]
        "b1": model.fc1.bias.tolist(),     # [hidden_size]
        "W2": model.fc2.weight.tolist(),   # [vocab_size x hidden_size]
        "b2": model.fc2.bias.tolist()      # [vocab_size]
    }

    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w") as f:
        json.dump(weights, f, indent=2)
    print(f"Weights saved to {out_path}")
    print(f"  set neural_temperature: 1.0 in config (T=1.0 = trained probabilities as-is)")
    print(f"  T<1.0 = sharper (more folk-like), T>1.0 = more varied")

if __name__ == "__main__":
    main()
