# Neural Constraint Solving — Research Survey

_Based on literature search, March 2026._

## Research Gap

Musical constraint solving + neural networks is underexplored compared to physics/engineering domains. Most music generation uses soft constraints or heuristics rather than formal constraint satisfaction. Using neural networks as heuristic rules **within** a formal CP solver for real-time music generation appears to be a novel combination not well-covered in current literature.

## Key Research Areas

### 1. Physics-Informed Neural Networks (PINNs)

- 3,000+ citations; embed physical laws as soft or hard constraints inside neural training.
- Most relevant architectural precedent, but domain is continuous ODEs/PDEs — not discrete combinatorial search.

### 2. Lagrangian Neural Networks

- 100+ papers on using Lagrangian formulations for constrained optimization.
- Relevant for understanding constraint-as-loss formulations; less directly applicable to CP search.

### 3. Music Generation + Constraints

- ~27 papers. Mostly RNNs with positional or melodic constraints.
- Primary approach: soft constraints via penalized loss, not formal satisfaction.

### 4. Neural ODEs + Constraints

- Emerging area for real-time optimization with stability guarantees.
- Relevant if solver is eventually used for continuous control, not current discrete scope.

## Most Relevant Papers

| Paper                                                                                          | Relevance                                          |
| ---------------------------------------------------------------------------------------------- | -------------------------------------------------- |
| "DeepSaDe: Learning Neural Networks That Guarantee Domain Constraint Satisfaction" (AAAI 2024) | Hard constraint satisfaction in learned models     |
| "Anticipation-RNN: Enforcing unary constraints in sequence generation"                         | Music, unary constraints, RNN                      |
| "Interactive Music Generation with Positional Constraints using Anticipation-RNNs" (2017)      | Music + positional constraints                     |
| "Lagrange Oscillatory Neural Networks for Constraint Satisfaction and Optimization" (2025)     | Constraint satisfaction via Lagrangian formulation |

## Jazz Dataset (Weimar Jazz Database)

**Location**: `../NeuralConstraints/` (sibling repo)

### Quantization Work (March 2026)

- **Problem**: Original quantization had 0.0881 beats average error.
- **Solution**: Data-driven threshold approach with 0.0625 beats minimum (64th note resolution).
- **Key insight**: Swing is a performance aspect, not notation — remove from quantization.
- **Result**: 67% error reduction, clean musical representations.

### Final Dataset (April 2026)

- **File**: `complete_jazz_dataset.json`
- **Scale**: 456 solos, ~15MB
- **Features per event**: raw pitches, melodic intervals, mod-octave, rhythm fractions, chord progressions
- **Compatibility**: direct integration with pitch-engine, rhythm-engine, interval rules, mod-octave rules of the Gecode solver
- **Preprocessing scripts**: `comprehensive_jazz_preprocessor.py`, `threshold_jazz_quantizer.py`
- **Source DB**: `Datasets/Weimar/wjazzd.db` (SQLite)
- **Status**: ready for training

## Novelty Assessment

The specific combination of:

1. Formal constraint propagation (Gecode CP solver)
2. Neural soft heuristic inside the value-ordering step
3. Real-time musical generation

…is not covered by any paper found in the March 2026 survey. The closest precedent is Anticipation-RNN, which enforces constraints by conditioning generation — a fundamentally different architecture that does not use a CP solver.

## See Also

- `docs/NEURAL_HEURISTIC_INTEGRATION.md` — implementation plan and research progression
