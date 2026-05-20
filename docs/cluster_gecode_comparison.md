# Cluster Engine vs. Gecode: A Constraint Paradigm Comparison

Two solvers, two philosophies. This project uses both: **Gecode** as the underlying constraint engine, and the **Cluster Engine** architecture as the higher-level musical search strategy layered on top.

---

## 1. Core Paradigm Difference

|                     | Gecode                                              | Cluster Engine                                                                   |
| ------------------- | --------------------------------------------------- | -------------------------------------------------------------------------------- |
| **Paradigm**        | Constraint Propagation + DFS                        | Sequential greedy search with structured backtracking                            |
| **Variables**       | `IntVar` (domain-shrinking objects)                 | Candidate lists (ordered arrays, popped on failure)                              |
| **Constraints**     | Posted once; propagators prune domains continuously | Rules tested on demand at each search step; failure triggers backtrack           |
| **Search**          | Generic DFS over a cloned `Space` tree              | Per-engine index stepping; forward/backtrack rules control navigation            |
| **Backtracking**    | Implicit (space cloning + path stack)               | Explicit (the solver decides _which_ engine to backtrack, not just the last one) |
| **Domain language** | Integer sets, arithmetic expressions, reification   | Musical lists: durations, MIDI pitches, time signatures                          |
| **Implementation**  | C++ template library                                | Common Lisp                                                                      |

The fundamental difference: Gecode treats the whole problem as a constraint graph and relies on propagation to shrink domains before branching. The Cluster Engine treats the problem as a sequence of independent variable streams (engines), one per voice component, and navigates between them with musically-informed forward and backtrack rules.

---

## 2. Gecode

### Terminology

- **`Space`** — the search node. Contains all variables. Cloned at each branch point.
- **`IntVar`** — an integer variable with a domain (set of candidate values). Propagation shrinks it.
- **Constraint / Propagator** — a relation posted onto the space (e.g. `rel`, `dom`, `distinct`). A propagator runs whenever a variable's domain changes and removes inconsistent values from other variables.
- **`branch()`** — registers a variable/value selection heuristic. Gecode uses it to decide which variable to assign next and which value to try first.
- **`DFS`** — depth-first search engine. Repeatedly clones the space, commits a choice, and backtracks when a domain becomes empty.
- **`SS_BRANCH / SS_SOLVED / SS_FAILED`** — space status after propagation.

### Minimal C++ example

```cpp
#include <gecode/int.hh>
#include <gecode/search.hh>
using namespace Gecode;

// A Space subclass represents one search node.
class NoUnison : public Space {
public:
    IntVarArray pitch;   // two pitch variables

    NoUnison() : pitch(*this, 2, 60, 72) {  // MIDI 60–72 each
        rel(*this, pitch[0], IRT_NQ, pitch[1]);   // p0 ≠ p1 (no unison)
        rel(*this, pitch[0], IRT_GR, pitch[1]);   // p0 > p1 (soprano above alto)
        branch(*this, pitch, INT_VAR_NONE(), INT_VAL_MIN());
    }

    // Copy constructor required by Gecode's clone mechanism.
    NoUnison(NoUnison& s) : Space(s) {
        pitch.update(*this, s.pitch);
    }
    virtual Space* copy() { return new NoUnison(*this); }
};

int main() {
    NoUnison* root = new NoUnison();
    DFS<NoUnison> engine(root);
    delete root;

    while (NoUnison* sol = engine.next()) {
        std::cout << sol->pitch[0] << " " << sol->pitch[1] << "\n";
        delete sol;
    }
}
```

**What happens internally:**

1. `NoUnison()` is constructed — two `IRT_NQ` and `IRT_GR` propagators are installed.
2. `DFS` clones the space at each branch point, commits a value, and propagates.
3. If any domain empties → `SS_FAILED` → backtrack (restore the clone).
4. If all variables assigned and no failure → `SS_SOLVED` → emit solution.

### In this project (C++)

```cpp
// From src/gecode_cluster_integration.cpp — IntegratedMusicalSpace constructor
IntegratedMusicalSpace::IntegratedMusicalSpace(int length, int voices, ...) : Space() {

    // Declare variable arrays
    absolute_vars_(*this, length * voices, IntSet(0, 127)),   // pitch vars
    rhythm_vars_  (*this, length * voices, -100000, 100000),  // rhythm vars (neg = rest)

    // Narrow domains per voice
    dom(*this, absolute_vars_[v * length + i], voice_dom);

    // Post interval constraint with reification (only when both positions are notes)
    BoolVar both_notes = expr(*this, cur_note && prev_note);
    rel(*this, interval_vars_[int_idx], IRT_EQ, diff, Reify(both_notes, RM_IMP));

    // Register branching strategy
    branch(*this, rhythm_vars_, INT_VAR_NONE(), INT_VAL(select_rhythm_abs_min_value));
    branch(*this, absolute_vars_, INT_VAR_NONE(), INT_VAL(select_value_by_heuristic));
}
```

A dynamic rule (e.g. `r-pitch-pitch: no_unison`) is compiled into a lambda and posted later:

```cpp
compiled_pp->post_constraint = [pp_v1, pp_v2](DynamicRules::ConstraintContext& ctx) {
    for (int i = 0; i < ctx.sequence_length; ++i) {
        IntVar p1 = (*ctx.pitch_vars)[pp_v1 * ctx.sequence_length + i];
        IntVar p2 = (*ctx.pitch_vars)[pp_v2 * ctx.sequence_length + i];
        Gecode::rel(*ctx.space, p1, Gecode::IRT_NQ, p2);  // p1 ≠ p2 at every position
    }
};
```

---

## 3. Cluster Engine

### Terminology

- **Engine** — one variable stream. Even index = rhythm voice, odd index = pitch voice, last = metric/time-signature. Two engines per musical voice.
- **`vsolution`** — a 2D array: `vsolution[engine][index]` = candidate list at that position. The head of the list is the current candidate; it is popped on failure.
- **`vindex`** — one pointer per engine: how far that engine has been committed.
- **Forward rule** — a function called after a successful step. It decides which engine to advance next, using musical heuristics (e.g. fill metric first, then the rhythm voice that is furthest behind).
- **Backtrack rule** — a function called after failure. It decides which engine to undo, not necessarily the most recent one (backjumping).
- **Rule** — a Lisp lambda tested against `vlinear-solution`. Returns `T` (pass) or a list of engines to prefer for backtracking.
- **Heuristic rule** — sorts the candidate list before committing, guiding the search without constraining.

### Minimal Lisp example (from `cluster-engine-sources/`)

```lisp
;; Domain: two pitch lists
(poly-engine
  max-index   8              ; 8 positions per voice
  domains     '((60 62 64 65 67 69 71 72)   ; rhythm engine 0 (voice 0)
                (60 62 64 65 67 69 71 72)   ; pitch  engine 1 (voice 0)
                (48 50 52 53 55 57 59 60))  ; pitch  engine 3 (voice 1)
  nr-of-voices 2
  vrules      (vector
                ;; Rule applied to pitch engines 1 and 3
                (rule-2-engines
                  (lambda (vsolution vlinear-solution vindex ...)
                    ;; p0[i] ≠ p1[i] at current index
                    (let ((p0 (car (aref vlinear-solution 1)))
                          (p1 (car (aref vlinear-solution 3))))
                      (not (= p0 p1))))
                  '(1 3)))               ; check in engines 1 and 3
              )
  forwardrule  'fwd-rule5    ; musically-informed engine selection
  backtrackrule 'backtrack-rule3)
```

**What happens internally (from `02.engine.lisp`):**

```
1. vindex all set to -1 (nothing committed yet).
2. Forward rule picks the next engine (e.g. metric first, then shortest rhythm).
3. vindex[engine]++ — the head of vsolution[engine][index] is the current candidate.
4. Each rule in vrules[engine] is tested.
   - Returns T   → rule passes; continue.
   - Returns list → failure; record preferred backtrack engines.
5. If all rules pass: call forward rule again, step to next engine.
6. If any rule fails:
   a. Record backtrack history (engine, index, time-point, note-count).
   b. Pop the failed candidate from the list.
   c. If list now empty: decrease vindex (backtrack this engine).
   d. Call backtrack rule to decide which engine to revisit next.
   e. Optionally backjump (skip engines that cannot fix the failure).
7. Repeat until all vindex == max-index → solution found.
```

### How backjumping differs from Gecode backtracking

Gecode always undoes the _most recent_ branch. The Cluster Engine can skip backward across multiple engines, guided by musical knowledge encoded in the backtrack rule. For example, if a pitch rule between voice 0 and voice 2 fails, the solver can jump directly back to voice 2 instead of laboriously unwinding through intervening rhythm assignments.

```lisp
;; backtrack-rule3 (04.Backtrack-rules.lisp):
;; Uses the backtrack routes suggested by failed rules, weighted by
;; how many times each route was proposed across multiple failures.
(defun backtrack-rule3 (vindex vbacktrack-engine nr-of-engines ...)
  (let ((preferred (backtrackroute-from-failed-rules2 vbacktrack-engine ...)))
    (loop for engine in (append preferred default-engine-order)
          while (< (aref vindex engine) 0)
          finally (return engine))))
```

---

## 4. Side-by-Side: Same Problem in Both Paradigms

**Problem:** Two voices, 4 positions. Voice 0 must always be above voice 1. No unisons.

### Gecode (C++)

```cpp
// In Space constructor:
for (int i = 0; i < 4; ++i) {
    rel(*this, p0[i], IRT_GR, p1[i]);   // p0 > p1
    rel(*this, p0[i], IRT_NQ, p1[i]);   // p0 ≠ p1  (implied by GR, shown for clarity)
}
branch(*this, pitches, INT_VAR_NONE(), INT_VAL_MIN());
```

Constraints are posted once. Propagation enforces them globally throughout the search.

### Cluster Engine (Lisp)

```lisp
;; Rule lambda — tested at every forward step of pitch engine 1 or 3:
(lambda (vsolution vlinear-solution vindex ...)
  (let ((p0 (car (aref vlinear-solution 1)))   ; current candidate in voice 0 pitch engine
        (p1 (car (aref vlinear-solution 3))))  ; current candidate in voice 1 pitch engine
    (> p0 p1)))   ; voice 0 must be above voice 1
```

The rule is a function, not a posted constraint. It is called eagerly when the relevant engines are both active. On failure the solver pops the candidate and tries the next value in the list.

### Config (this project's JSON schema)

```json
{
  "rule_type": "r-pitch-pitch",
  "constraint": "voice_above",
  "target_voices": [0, 1]
}
```

The JSON config layer translates this into a Gecode `IRT_GR` constraint applied across all positions — the Cluster Engine's rule-testing semantics are approximated through Gecode propagation at the C++ level.

---

## 5. Performance Comparison

Performance depends heavily on problem structure. The table below compares characteristic behaviours across problem types. "Nodes explored" refers to the number of assignments attempted before a first solution is found; "Backtrack cost" refers to the overhead of undoing an assignment.

| Problem type                                         | Gecode nodes explored                                                         | Cluster nodes explored                                                                 | Backtrack cost                        | Memory per node        | **Faster overall**                                        |
| ---------------------------------------------------- | ----------------------------------------------------------------------------- | -------------------------------------------------------------------------------------- | ------------------------------------- | ---------------------- | --------------------------------------------------------- |
| Hard all-different (N-queens, scheduling)            | Low — propagation eliminates most branches early                              | High — no propagation; each candidate tested by rule                                   | Low (clone is cheap for small spaces) | O(variables) per clone | **Gecode**                                                |
| Arithmetic constraints (sum, linear eq.)             | Very low — arc-consistency prunes to near-zero                                | High — rules only test; no forward pruning                                             | Low                                   | O(variables)           | **Gecode**                                                |
| Multi-voice pitch generation, loose rules            | Moderate — global propagation over all voices at once                         | Low — engines advanced one at a time; musical forward rule avoids irrelevant branches  | Very low (pop a list item)            | O(1) per step          | **Cluster Engine**                                        |
| Multi-voice pitch generation, many cross-voice rules | Moderate–high (constraint graph grows with rule count)                        | Low–moderate — backjumping skips unrelated engines; failed-rule history prunes retries | Very low                              | O(1) per step          | **Cluster Engine**                                        |
| Real-time / incremental generation                   | Moderate — full `Space` must be cloned and rebuilt for each incremental query | Very low — engines are independent streams; partial solutions extend in place          | Negligible                            | O(1)                   | **Cluster Engine**                                        |
| Finding _all_ solutions exhaustively                 | Optimal — DFS with pruning is the standard algorithm                          | Poor — no global completeness guarantee; list exhaustion does not equal full proof     | Low                                   | O(variables)           | **Gecode**                                                |
| Large domains, few hard constraints                  | Moderate — propagation does little; branching factor dominates                | Low — heuristic rules pre-sort candidates, reducing effective branching factor         | Very low                              | O(1)                   | **Cluster Engine**                                        |
| Small domains, many tight constraints                | Very low — propagation often solves the problem at the root                   | Low — fast rule testing with early termination                                         | Very low                              | O(1)                   | **Tie** (similar nodes; Gecode has lower constant factor) |

### Key cost factors

- **Gecode clone cost** scales with the number of variables and propagators in the space. For 4 voices × 16 positions = 128 variables, a clone takes ~10–50 µs. For 8 voices × 32 positions = 512 variables, it can reach 200+ µs per branch point.
- **Cluster Engine step cost** is nearly constant: pop one item from a list (~10–50 ns), run O(R) rule lambdas where R = rules per engine. No allocation, no clone.
- **Propagation payoff**: Gecode's advantage is front-loaded. One tight global constraint (e.g. `all_different` over 128 variables) can prune 99% of the search tree in milliseconds. Without such constraints, Gecode degrades toward raw backtracking, losing its edge.
- **Backjumping payoff**: Cluster's advantage scales with voice count and rule independence. With 6+ voices, a failure in voice 5's pitch rarely implicates voice 2's rhythm; backjumping eliminates those irrelevant unwindings entirely.

### Overall verdict

**Gecode is faster** when constraints are globally tight and propagation eliminates large subtrees early — combinatorial puzzles, scheduling, and hard harmonic constraints with small domains.

**Cluster Engine is faster** for the primary use case here: open-ended multi-voice musical generation with ordered candidate lists and domain-specific musical heuristics. The combination of constant-cost stepping, musically-guided backjumping, and heuristic candidate sorting means it typically finds a musically valid solution with far fewer backtracks than Gecode would require navigating the same search space.

In this project both are active simultaneously: Gecode enforces hard posted constraints (no unisons, interval bounds, metric hierarchy) while the Cluster Engine's architecture controls _which_ voice/position is assigned next, so each `Space::commit()` is already in a near-valid region of the search space.

---

## 6. How They Cooperate in This Project

The two systems do not run in parallel — they are layered. The Cluster Engine's concepts (engine indexing, musical rules, heuristic scoring) are used to **configure** a Gecode space before the Gecode DFS takes over and runs the search.

### The pipeline

```
JSON config
    │
    ▼
Parse rules → compile each rule into a CompiledConstraint { post_constraint lambda }
    │
    ▼
engine_index_for_voice_component(voice, "pitch"|"rhythm"|"metric", num_voices)
    │   maps voice 0 pitch → engine 1, voice 0 rhythm → engine 0, metric → engine num_voices*2
    ▼
build_configured_space_()
    ├── construct IntegratedMusicalSpace (Gecode Space subclass)
    │       rhythm_vars_[voice * length + i]   — IntVarArray, domain narrowed by dom()
    │       absolute_vars_[voice * length + i] — IntVarArray (pitch), domain narrowed by dom()
    │       metric_vars_[i]                    — IntVarArray (time sig numerators)
    │       interval_vars_                     — derived, with BoolVar reification for rests
    │
    ├── for each CompiledConstraint: call post_constraint(ctx)
    │       ctx carries pointers to pitch_vars, rhythm_vars, metric_vars, space
    │       lambda posts Gecode propagators: rel(), dom(), expr(), BoolVar reification
    │
    ├── if heuristic scorers exist: configure_pitch_heuristic_value_ordering(scorer)
    │       wires CompiledHeuristicRule lambdas into Gecode's INT_VAL() brancher callback
    │
    └── branch() — registers variable and value selection strategies
            rhythm_vars_: INT_VAR_NONE / INT_VAR_SIZE_MIN + custom value selector
            absolute_vars_: INT_VAR_NONE / INT_VAR_SIZE_MIN + heuristic value selector
    │
    ▼
Gecode DFS engine runs:
    clone space → commit value → propagate → SS_SOLVED / SS_BRANCH / SS_FAILED
    at each branch point: heuristic scorer is called to rank candidate values
    │
    ▼
extract_solution_from_space_() → MusicalSolution
```

### Concrete example

Config with one hard constraint and one heuristic:

```json
{
  "num_voices": 2,
  "solution_length": 8,
  "voices": [
    { "pitch": [60, 62, 64, 65, 67, 69, 71, 72] },
    { "pitch": [48, 50, 52, 53, 55, 57, 59, 60] }
  ],
  "rules": [
    {
      "rule_type": "r-pitch-pitch",
      "constraint": "voice_above",
      "target_voices": [0, 1],
      "id": "soprano_above_bass"
    },
    {
      "rule_type": "r-one-voice",
      "constraint": "stepwise_preference",
      "target_voice": 0,
      "heuristic": true,
      "id": "prefer_steps"
    }
  ]
}
```

**What happens step by step:**

1. `r-pitch-pitch / voice_above` is compiled into a `CompiledConstraint`. Its `post_constraint` lambda is:

   ```cpp
   // src/dynamic_constraint_solver_main.cpp ~line 2643
   compiled_pp->post_constraint = [pp_v1, pp_v2, ...](DynamicRules::ConstraintContext& ctx) {
       for (int i = 0; i < ctx.sequence_length; ++i) {
           IntVar p1 = (*ctx.pitch_vars)[pp_v1 * ctx.sequence_length + i]; // voice 0
           IntVar p2 = (*ctx.pitch_vars)[pp_v2 * ctx.sequence_length + i]; // voice 1
           Gecode::rel(*ctx.space, p1, Gecode::IRT_GR, p2); // p0 > p1 at every position
       }
   };
   ```

2. `r-one-voice / stepwise_preference` (heuristic) compiles into a scorer lambda stored separately in `compiled_rules_->heuristic_scorers`.

3. `build_configured_space_()` constructs the Gecode space, then:
   - Calls `post_constraint(ctx)` → 8 `IRT_GR` propagators are posted on the space. Gecode propagation immediately narrows pitch domains: any value in voice 1 ≥ min(voice 0 domain) is eliminated.
   - Calls `configure_pitch_heuristic_value_ordering(scorer)` → the stepwise scorer is wired into Gecode's `INT_VAL()` callback.

4. Gecode DFS starts. When it picks a value for `absolute_vars_[0]` (voice 0, position 0), it calls the heuristic scorer for each candidate in the domain. The scorer returns a higher bucket score for pitches one or two semitones away from the previously assigned pitch. The brancher commits the top-ranked value first.

5. The `IRT_GR` propagator immediately fires: it removes from voice 1's domain at that position any value ≥ the committed voice 0 pitch.

6. If propagation leaves voice 1 with an empty domain → `SS_FAILED` → Gecode backtracks to the last branch point and tries the next heuristic-ranked value.

**Division of responsibility:**

|                                | Responsible for                                                                   |
| ------------------------------ | --------------------------------------------------------------------------------- |
| Cluster Engine indexing        | Mapping `voice=0, component="pitch"` → Gecode variable index `voice * length + i` |
| Cluster Engine compiled rules  | Generating the `post_constraint` lambdas that become Gecode propagators           |
| Cluster Engine heuristic rules | Scoring candidate values inside Gecode's brancher callback                        |
| Gecode propagation             | Enforcing `voice_above` globally, pruning domains before branching                |
| Gecode DFS                     | Backtracking, cloning, driving the search to completion                           |
