# Heuristic Example - Determinism Explanation

## What You're Observing

With `random_seed: 123` (fixed), the solver produces **identical solutions every run**:

```
Run 1: [65, 64, 65, 64, 65, 67]
Run 2: [65, 64, 65, 64, 65, 67]
Run 3: [65, 64, 65, 64, 65, 67]
```

**This is correct behavior.** Here's why:

## Three Components Causing Determinism

### 1. Fixed Random Seed = Reproducible Randomization

- `random_seed: 123` → Deterministic branching choices
- Each run explores search tree identically
- Ideal for testing/debugging (reproducible behavior)

### 2. Strong Heuristic Convergence

The heuristics create tight guidance:

- **Priority 5 (stepwise motion)**: Strongly prefer ±2 semitones
- **Priority 3 (consonance)**: Prefer major triad tones (C, E, G)
- **Priority 1 (register)**: Weak preference for middle range

With these priorities + hard constraints (no adjacent repeats), there's **only ONE solution that maximizes all priority levels**.

### 3. First Solution Finding

Gecode's search finds the first/best solution satisfying all constraints and heuristics, then returns it.

## How to Get Variety

### Option A: Use Random Seed (Recommended for Demonstrations)

Change config to:

```json
"random_seed": 99999999
```

This tells the solver: "Generate a random seed each time" → **Different solutions per run**

### Option B: Modify the Config

Reduce heuristic strength or constraints:

- Lower `weight` values
- Reduce `priority` levels
- Add fewer hard constraints

### Option C: Longer Sequences

Use `solution_length: 12` instead of 6 → More constraint interactions → More solution diversity

## Understanding Random Seeds

| Seed Value              | Behavior                                    |
| ----------------------- | ------------------------------------------- |
| `0`                     | Auto-generates random seed (varies per run) |
| `1`-`uint_max-1`        | Fixed seed (reproducible)                   |
| `uint_max` (4294967295) | Maps to seed 0 (random)                     |

## Recommendation

For the example config:

- Keep `random_seed: 123` for **reproducibility/teaching**
- Add note: "Change to larger number for variety"
- Or create two versions (reproducible + random)

The determinism is a **feature**, not a bug—it's essential for constraint solver testing and scientific reproducibility!

## Current Status

✅ `heuristic_example_musical_melody_8x1.json` - Fixed seed (reproducible)
✅ `heuristic_example_reproducible_seed.json` - Same with explicit documentation
