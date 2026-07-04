# How Gecode Works in This System

A plain-language guide for musicians and composers using this solver.

---

## The Basic Idea

Gecode is a constraint programming (CP) library. Instead of composing music note by note with a generative algorithm, Gecode:

1. **Declares all notes at once** as unknown variables with a range of allowed values.
2. **Posts rules** that eliminate impossible combinations.
3. **Searches** systematically for a complete assignment that satisfies every rule simultaneously.

The result is always guaranteed to satisfy every hard rule you specified. No post-hoc filtering, no rejection of bad outputs after the fact.

---

## Step 1 — Variables and Domains

Every note slot in a solution becomes a **Gecode integer variable** (`IntVar`). Each variable has a **domain**: the set of values it is currently allowed to take.

In this system three kinds of variables are created at startup:

| Variable array          | What it holds                       | Example domain                                     |
| ----------------------- | ----------------------------------- | -------------------------------------------------- |
| `absolute_vars` (pitch) | MIDI pitch values                   | `{60, 62, 64, 65, 67, 69, 71, 72}` = C major scale |
| `rhythm_vars`           | Duration in ticks (negative = rest) | `{6, 12, -6, -12}` = eighth/quarter note or rest   |
| `metric_vars`           | Time-signature numerator per slot   | `{3, 4}` = 3/4 or 4/4                              |

For a 2-voice, 4-note piece, Gecode allocates `2 × 4 = 8` pitch variables and `8` rhythm variables — 16 variables total. Each starts with its full domain intact.

**Musical example:**

```
Config: 2 voices, 4 positions, C-major scale, quarter notes only

Pitch variables at start:
  voice[0][0] = {60,62,64,65,67,69,71,72}   ← all 8 notes allowed
  voice[0][1] = {60,62,64,65,67,69,71,72}
  voice[0][2] = {60,62,64,65,67,69,71,72}
  voice[0][3] = {60,62,64,65,67,69,71,72}
  voice[1][0] = {60,62,64,65,67,69,71,72}
  voice[1][1] = {60,62,64,65,67,69,71,72}
  voice[1][2] = {60,62,64,65,67,69,71,72}
  voice[1][3] = {60,62,64,65,67,69,71,72}

Rhythm variables at start:
  all 8 positions = {24}   ← quarter note only (24 ticks in this example)
```

---

## Step 2 — Posting Constraints

Before any search begins, all rules are translated into **Gecode constraint objects** and "posted" into the space. Posting means: _install a propagator that will continuously reduce domains as the search proceeds_.

**Example: `all_different` on voice 0 (twelve-tone style)**

```cpp
// What the solver does internally when you write:
// { "rule_type": "r-one-voice", "constraint": "all_different", "target_voices": [0] }

Gecode::IntVarArgs vars;
for (int i = 0; i < 4; ++i)
    vars << pitch_vars[0 * 4 + i];  // voice 0, positions 0–3

Gecode::distinct(*gecode_space, vars);  // ← posted once, runs forever
```

After posting `distinct`, Gecode immediately knows: no two positions in voice 0 can share a MIDI value. If the solver ever tries to assign the same pitch twice, the constraint propagator removes it from the remaining domains automatically.

**Example: `voice_above` (soprano above bass)**

```cpp
// { "rule_type": "r-pitch-pitch", "constraint": "voice_above", "target_voices": [0, 1] }

for (int i = 0; i < 4; ++i) {
    Gecode::rel(*gecode_space,
        pitch_vars[0 * 4 + i],   // voice 0
        Gecode::IRT_GR,           // strictly greater than
        pitch_vars[1 * 4 + i]);   // voice 1
}
```

At every position, voice 0 must be strictly above voice 1. This immediately prunes any pitch from voice 0's domain that isn't above _some_ value in voice 1's domain, and vice versa.

**Example: time signature pinned to 4/4**

```cpp
// { "rule_type": "r-time-signature", "parameters": ["4/4"] }

Gecode::IntSet single(4, 4);
for (int i = 0; i < 4; ++i)
    Gecode::dom(*gecode_space, metric_vars[i], single);
```

All 4 metric variables are immediately fixed to 4 (the numerator of 4/4).

---

## Step 3 — Propagation

When a constraint is posted — and every time any domain changes — Gecode runs **propagation**: it inspects every variable's domain and removes values that cannot possibly be part of any solution given the current constraints.

Propagation does not require any search decisions. It runs automatically.

**Musical example — propagation chain:**

```
Setup: voice 0 and voice 1 must form consonant intervals (interval class ∈ {0,3,4,7,8,9}).
Voice 0 domain: {60, 61, 62}   (C4, C#4, D4)
Voice 1 domain: {60, 61, 62}

After posting interval_class constraint and propagating:
  If voice[0]=61 (C#4), what consonant values exist in {60,61,62} for voice[1]?
    60: interval = 1 semitone → not consonant
    61: interval = 0 → consonant (unison) ✓ ... but only if voice[0]=61 is ever used
    62: interval = 1 → not consonant
  So voice[0]=61 could only pair with voice[1]=61 — but if voice[1]=61 is also
  unresolvable, propagation eliminates 61 from voice[0].

After propagation:
  voice[0] = {60, 62}   ← 61 removed
  voice[1] = {60, 62}   ← 61 removed
```

Propagation has shrunk both domains from 3 to 2 values — without any explicit search decision.

---

## Step 4 — Search (Branching + Backtracking)

After propagation, some variables may still have multiple values in their domain. The solver must **choose** a value. This is the search step.

### 4.1 Variable selection (branching)

`branching: "first_fail"` — pick the variable with the **smallest remaining domain**. A variable with only 1 value left costs nothing to assign. A variable with 10 options is more likely to cause future failures, so tackle it first.

`branching: "input_order"` — pick variables in declaration order (voice 0 pos 0, then pos 1, etc.).

### 4.2 Value selection (what to try first)

This is the hook where heuristics and neural scoring plug in. Given the chosen variable, which value do we try first?

| `value_order` | What happens                                                                    |
| ------------- | ------------------------------------------------------------------------------- |
| `"min"`       | Lowest MIDI pitch / shortest duration always tried first                        |
| `"heuristic"` | All domain values scored by `heuristic_energy` rules; highest score tried first |
| `"neural"`    | All domain values scored by the MLP; Gumbel-max ranking tried first             |

**Important:** this only affects which value is tried _first_. The constraint check happens after — hard rules are enforced regardless.

### 4.3 The branch

Gecode creates a binary choice node:

```
Branch on voice[0][0]:
  ├─ Left branch:  assign voice[0][0] = 64  (try this first)
  └─ Right branch: exclude 64, pick next    (used if left fails)
```

After assigning `voice[0][0] = 64`, propagation runs again and may prune other variables.

### 4.4 Backtracking

If at any point a domain becomes **empty** (no valid value remains), the search backtracks to the most recent choice node and activates the right branch (exclude the tried value, pick next-best from the domain).

```
voice[0][0] = 64  ← try E4
  voice[0][1] = 65  ← F4, propagation OK
    voice[0][2] = 67  ← G4, propagation OK
      voice[1][0] = ??  ← domain empty! no consonant interval exists with 64 in {60}
    ← backtrack to voice[0][2]
  voice[0][2] = 69  ← A4, try next
    voice[1][0] = 60  ← C4 is a major sixth below A4: consonant ✓
      ... continue ...
```

This is **not** sample rejection. No complete sequence is ever generated and then tested. Each backtrack step touches only the variables affected by the failure.

---

## Step 5 — Solution Extraction

When every variable has exactly one value in its domain (all domains reduced to singletons), the search has found a complete solution. The solver reads out the assigned integers:

```cpp
for (int v = 0; v < num_voices; ++v)
    for (int i = 0; i < seq_len; ++i)
        pitches[v][i] = pitch_vars[v * seq_len + i].val();   // exact integer
```

This is then packaged into a `MusicalSolution` object and exported to MusicXML / JSON / text.

---

## The Full Picture

```
JSON config
    │
    ▼
Parse domains
    │
    ▼
Create IntegratedMusicalSpace
    ├── pitch variables   absolute_vars[voice * len + pos]
    ├── rhythm variables  rhythm_vars[voice * len + pos]
    └── metric variables  metric_vars[pos]
    │
    ▼
Post all constraints
    ├── Gecode::distinct   (all_different / twelve-tone)
    ├── Gecode::rel        (voice_above, exact_interval, time_sig equality)
    ├── Gecode::dom        (domain restriction, metric pinning)
    ├── Gecode::linear     (onset sum / bar fill for metric hierarchy)
    └── custom propagators (palindrome, retrograde inversion, no-syncopation,
                            wildcard dynamic expressions)
    │
    ▼
Initial propagation → prune impossible values
    │
    ▼
┌──────────────────────────────────────────────────────────────┐
│  SEARCH LOOP                                                 │
│                                                              │
│  1. Select variable     → branching (first_fail)             │
│  2. Rank candidates     → value_order (min/heuristic/neural) │
│  3. Assign best value   → propagate immediately              │
│  4. Contradiction?      → backtrack to last choice node      │
│  5. All assigned?       → solution found ✓                   │
└──────────────────────────────────────────────────────────────┘
    │
    ▼
Extract pitch + rhythm + metric values
    │
    ▼
Export MusicXML / JSON / text
```

---

## Worked Example: 2-Voice Counterpoint

**Config:** 2 voices, 4 positions. Soprano (voice 0): C4–G5 diatonic. Bass (voice 1): A2–C4 diatonic. Rules: soprano above bass, no parallel fifths, soft consonance preference, `value_order: "neural"`.

**At startup:**

```
voice[0][0..3] = {60,62,64,65,67,69,71,72,74,76,79}  ← soprano domain
voice[1][0..3] = {45,47,48,50,52,53,55,57,59,60}     ← bass domain
```

**After posting `voice_above` and propagating:**

```
voice[1][0..3]: 60 removed (C4 = C4 is not strictly below any soprano note ≥ 60)
→ voice[1] = {45,47,48,50,52,53,55,57,59}
```

**Search begins (`first_fail` picks voice[1][0], smallest domain):**

```
Neural scorer ranks bass candidates for position 0:
  C3(48)=0.31  G3(55)=0.41  A3(57)=0.38  ...  ← G3 wins
→ Assign voice[1][0] = 55 (G3)

Propagation: voice[0][0] must be > 55 and form a consonant interval.
  Consonant above G3: C4(60, P4), D4(62, min6), E4(64, maj6), G4(67, P8) ...
→ voice[0][0] pruned to {60,62,64,67,69,...}

Neural scorer ranks soprano candidates for position 0:
  C4(60)=0.45  D4(62)=0.33  E4(64)=0.28  ...  ← C4 wins
→ Assign voice[0][0] = 60 (C4, perfect fourth above G3) ✓

No parallel fifths: constraint not yet checkable (only 1 position assigned)

→ Move to position 1, repeat ...
```

Search continues position by position. If at any position a contradiction arises (e.g. parallel fifths makes all soprano candidates invalid), Gecode backtracks to the last choice node, excludes the tried value, and the neural scorer picks the next-best candidate from the remaining domain.

---

## What Gecode Does NOT Do

| Common assumption                                               | What actually happens                                                                      |
| --------------------------------------------------------------- | ------------------------------------------------------------------------------------------ |
| Generates a random complete sequence then checks it             | No — domains are pruned incrementally during search                                        |
| The neural scorer can override a hard rule                      | No — neural scoring only reorders candidates; hard constraints always checked after        |
| Backtracking means starting over                                | No — backtracks to the exact choice node that caused the failure                           |
| `heuristic_top_k: N` makes the solver faster safely             | Only if N ≥ domain size; smaller values make the scorer blind to high-MIDI candidates      |
| Without heuristics the solver finds no solutions                | No — `value_order: "min"` is fully deterministic and always finds a solution if one exists |
| Heuristics guarantee the musically best solution is found first | No — heuristics guide toward likely-good solutions but cannot guarantee musical optimality |

---

## Key Concepts Glossary

| Term                                 | Meaning                                                                                            |
| ------------------------------------ | -------------------------------------------------------------------------------------------------- |
| **Variable**                         | An unknown note value (pitch, duration, or time sig)                                               |
| **Domain**                           | Current set of still-possible values for a variable                                                |
| **Constraint / propagator**          | A rule installed in the space that removes impossible values automatically                         |
| **Propagation**                      | Automatic domain shrinking triggered by any domain change                                          |
| **Branching**                        | Choosing which variable to assign next                                                             |
| **Value selector**                   | Function that picks which domain value to try first (the heuristic/neural hook)                    |
| **Backtracking**                     | Undoing an assignment and trying the next alternative when a contradiction is found                |
| **Space** (`IntegratedMusicalSpace`) | The Gecode object holding all variables and constraints for one solve attempt                      |
| **`first_fail`**                     | Branching strategy: always pick the variable with the fewest remaining options                     |
| **`heuristic_top_k`**                | Limit how many candidates the value selector scores; `0` = score all (safe default)                |
| **Gumbel-max sampling**              | Stochastic ranking used by the neural scorer: `logit/T + noise` → different valid outputs per seed |
