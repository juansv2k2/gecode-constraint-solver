# Neural Heuristic Integration Plan

## Goal

Integrate a neural model into the current Gecode solver as a soft heuristic scorer for candidate ordering.

## Core Recommendation

- Keep Gecode responsible for correctness and search.
- Keep built-in rules and dynamic constraints as hard legality checks.
- Plug the neural model into the existing heuristic value-ordering path.
- Use the neural score to rank candidates, ideally below explicit symbolic heuristic buckets.

## Best Active Integration Points

- `include/gecode_cluster_integration.hh`
  - `HeuristicValueScorer`
- `src/gecode_cluster_integration.cpp`
  - `configure_pitch_heuristic_value_ordering(...)`
  - `select_value_by_heuristic(...)`
  - `candidate_is_better(...)`
- `include/dynamic_rule_compiler.hh`
  - `CompiledConstraint::score_candidate`
  - heuristic bucket types
- `src/musical_constraint_solver.cpp`
  - current heuristic scorer registration and runtime context setup

## Best First Musical Task

- Single-voice melodic continuation.
- Prefer interval or pitch-class prediction over a joint large-output model.

## Research Progression

Latency should stay under 1ms/candidate and musical benefit is measurable (vs. previous phase.)

1. **Context-Window MLP — pitch scorer** (start here)
   - Predict next pitch-class or interval from last N events.
   - Regression. Establishes integration point and latency baseline.

2. **Interval/Pitch-Class Classifier**
   - Same MLP, reframed as classification over discrete candidates.
   - Outputs probability per candidate → maps directly onto heuristic bucket weight.
   - Better than scalar regression for discrete Gecode ranking; avoids MAE distortion.
   - **This is the first real research result.**

3. **Separate Rhythm Scorer MLP**
   - Parallel MLP for rhythm duration prediction.
   - Keeps pitch and rhythm domains independent (matches solver's engine separation).
   - Validates whether neural guidance helps rhythm quality independently of pitch.

4. **Small GRU / LSTM**
   - Replace fixed context window with recurrent hidden state.
   - Only adopt if phases 1/2 measurably miss phrase-level patterns.
   - Compare directly against phase 2 on jazz dataset.

5. **Joint Pitch+Rhythm Scorer**
   - Single model; context = recent pitches + recent durations.
   - Captures rhythmic-melodic correlations (e.g. long notes on chord tones).
   - Only worth it if phases 1–4 show the domains interact strongly.

6. **Small Transformer Encoder (context-only)**
   - Encoder-only, sequence length 8–16 events. No autoregressive decoding.
   - Context embedding → candidate score.
   - Only justified if GRU (phase 4) still misses clear long-range structure.

7. **Energy / Discriminative Model**
   - Train to score "does this candidate fit this context?" as a discriminator.
   - Closer to how Gecode uses the scorer (ranking, not generating).
   - Likely best long-term formulation for discrete constraint-search integration.

8. **Style-Conditioned Scorer**
   - Add style embedding (from chord/genre metadata) as additional input.
   - Allows different stylistic preferences at runtime via JSON parameter, without retraining.

## Phase Gate Criteria

- Shadow mode first: log scores, don't change search. Confirm no-model runs are identical to current behavior.
- Gate each phase transition on: latency < 1ms/candidate AND measurable musical improvement over previous phase.
- For each phase: record musical quality delta on a fixed held-out jazz test set.

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
