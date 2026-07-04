# Neural Heuristic Integration

## Status (July 2026)

Harmonic conditioning is **fully operational**. The unified melodic MLP with
chord one-hot input is integrated into the Gecode value-ordering path and
produces **100% chord-tone output** on isolated tests.

### What is implemented

- **`include/neural_pitch_scorer.hh`** — `NeuralPitch::make_scorer()` factory.
  Loads `unified_melodic_mlp` weights, builds a scorer lambda capturing
  `harm_state` (per-position chord class vector) and `rhythm_context` by value.
  Uses Gumbel-max sampling (`logit_i / T + gumbel(seed, voice, pos, cand)`).
- **`src/gecode_cluster_integration.cpp`** — `configure_pitch_heuristic_value_ordering()`,
  `get_pitch_heuristic_value_ordering()` (new — exposes the current scorer so it
  can be chained).
- **`src/musical_constraint_solver.cpp`** — When heuristic soft-rules are active,
  the solver now **saves the prior (neural) scorer** before registering its own
  rule scorer, then wraps both: neural score goes into the highest-priority bucket,
  soft rules into lower-priority tiebreaker buckets.
- **`src/dynamic_constraint_solver_main.cpp`** — Parses `harmonic_domain` from
  config JSON, builds a 16/52/N-element `harmonic_state` vector (forward-fill
  from beat entries), and passes it to `make_scorer()`.
- **`scripts/train_unified_melodic.py`** — Trains harmonic model on 67k labeled
  Bach rows using `isDominantSeventh()` / `isMajorTriad()` / `isMinorTriad()`
  for correct dom7 chord labeling (music21 `Chord.quality` bug workaround).
  Output: `datasets/weights/harmonic_weights.json`.

### Key bugs fixed (July 2026)

| Bug | Symptom | Fix |
|---|---|---|
| `forward_clf` loop bound was `context_size` (8) instead of `input_size` (60) | Rhythm, voice, and chord input dims silently ignored | Changed inner loop to `in_dim = w.input_size > 0 ? w.input_size : x.size()` |
| `musical_constraint_solver.cpp` overwrote neural scorer on every `solve()` | Neural MLP never actually ran — soft heuristic rules always won | Save prior scorer with `get_pitch_heuristic_value_ordering()`, chain neural as primary bucket |
| `heuristic_top_k: 5` in test config only evaluated lowest-MIDI candidates | G4(67), A4(69) etc. never scored — E4 was best of first 5 | Removed top_k limit from test config; document that top_k must be 0 or ≥ domain size |

### Isolation test result

Config: `configs/chromatic_chord_test.json`
- 1 voice, 16 notes, chromatic domain C4–B5 (24 pitches)
- Chord progression: C major (beats 0–3), F major (4–7), G dom7 (8–11), C major (12–15)
- Constraints: no adjacent repeat + no pitch[i] == pitch[i+2]
- `neural_temperature: 0.3`

```
Output: G E C G | A C F A | G F D G | E C G C   →  16/16 chord tones (100%)
```

---

## Goal

Integrate a neural model into the current Gecode solver as a soft heuristic scorer for candidate ordering.

## Core Recommendation

- Keep Gecode responsible for correctness and search.
- Keep built-in rules and dynamic constraints as hard legality checks.
- Plug the neural model into the existing heuristic value-ordering path.
- Neural score is the **primary** (highest-priority) bucket; symbolic heuristic soft-rules are lower-priority tiebreakers.

## Integration Points

- `include/gecode_cluster_integration.hh`
  - `HeuristicValueScorer`
  - `get_pitch_heuristic_value_ordering()` — exposes current scorer for chaining
- `src/gecode_cluster_integration.cpp`
  - `configure_pitch_heuristic_value_ordering(...)`
  - `select_value_by_heuristic(...)`
  - `candidate_is_better(...)`
- `include/neural_pitch_scorer.hh`
  - `NeuralPitch::make_scorer(...)` — unified MLP factory with harm_state capture
  - `forward_clf(w, x)` — full 60-dim forward pass (W1 is 256×60, not 256×8)
- `src/musical_constraint_solver.cpp`
  - heuristic scorer registration — chains neural + soft rules
- `src/dynamic_constraint_solver_main.cpp`
  - `harmonic_domain` parsing → `harmonic_state` vector → `make_scorer()`

## Research Progression

Latency should stay under 1ms/candidate and musical benefit is measurable (vs. previous phase.)

1. **Context-Window MLP — pitch scorer** ✅ done
2. **Interval/Pitch-Class Classifier** ✅ done (128-class softmax over MIDI)
3. **Chord-conditioned unified MLP** ✅ done (`harmonic_weights.json`, 50% val-acc)
4. **Separate Rhythm Scorer MLP** — next candidate
5. **Small GRU / LSTM** — only if fixed context window misses phrase-level patterns
6. **Joint Pitch+Rhythm Scorer** — if domains interact strongly
7. **Small Transformer Encoder** — if GRU still misses long-range structure
8. **Style-Conditioned Scorer** — style embedding as additional runtime JSON parameter

## Phase Gate Criteria

- Shadow mode first: log scores, don't change search.
- Gate each phase on: latency < 1ms/candidate AND measurable musical improvement.
- Record musical quality delta on a fixed held-out test set.

## Heuristic Bucket Priority Order

- **Highest**: explicit symbolic style rules and must-have preferences
- **Middle**: neural likelihood or transformed negative error
- **Lowest**: deterministic seeded order or optional randomization

## Explicit Non-Goals For First Version

- No neural hard constraints.
- No neural domain pruning.
- No learned backtracking.
- No large generative models in the live search loop.

## Efficiency Rules

- Cache context features per `(voice, position)`.
- Cache candidate scores when possible.
- Use `heuristic_top_k` to cap expensive reranking.
- Keep models in memory.
- Avoid runtime parsing, model rebuilding, or large per-candidate feature extraction.

## Verification Plan

1. Add a neural scorer in shadow mode that logs scores without changing search.
2. Compare its rankings against current heuristic rankings.
3. Enable it as a low-priority bucket.
4. Measure solve time and candidate-scoring latency.
5. Compare MLP classifier (phase 2) vs GRU (phase 4) before considering a Transformer.
6. Confirm no-model runs remain identical to current behavior.
7. For each phase: record musical quality delta on a fixed held-out jazz test set.

## Dataset

- **Location**: `../NeuralConstraints/` (sibling repo)
  - `complete_jazz_dataset.json` — 456 solos, 15MB, cluster-engine-compatible features
  - `Datasets/Weimar/wjazzd.db` — source SQLite database (Weimar Jazz Database)
  - `comprehensive_jazz_preprocessor.py` — preprocessing pipeline
  - `threshold_jazz_quantizer.py` — quantization (67% error reduction vs naive approach)
- **Features**: raw pitches, melodic intervals, mod-octave, rhythm fractions, chord progressions
- **Integration**: direct compatibility with pitch-engine, rhythm-engine, interval rules, mod-octave rules
- **Status**: ready for training (April 2026)

## Relevant References

### In this repo

- `include/gecode_cluster_integration.hh`
- `src/gecode_cluster_integration.cpp`
- `include/dynamic_rule_compiler.hh`
- `src/dynamic_rule_compiler.cpp`
- `src/musical_constraint_solver.cpp`

### In NeuralConstraints repo (`../NeuralConstraints/`)

- `Sources/Cluster-NN-rule.lisp`
- `Sources/core_functions_to_Cluster.lisp`
- `Sources/nn-MAE-mod-oct_rule.lisp`
- `Sources/nn-MAE-rhythm_rule.lisp`

### See also

- `docs/NEURAL_RESEARCH_SURVEY.md` — literature survey and novelty assessment
