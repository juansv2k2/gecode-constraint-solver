# Usage Guide — Complete Configuration Reference

Complete reference for authoring configs for the Gecode Musical Constraint Solver.

Canonical key policy: use `target_voices` for rule targeting (always an array, even for one voice).

**Quick Summary:**

- Use voice-first configs (`voices`, optional `meter`)
- Use simple expression strings for dynamic rules
- Expression parser supports `&&`, `||`, `!`, `not`, `in`, and `not_in`
- Use simple built-in shorthand: `"constraint": "all_different"`
- Configure search behavior with `search_options`

The advanced JSON-AST expression form is still supported but not required.

## Table of Contents

1. [Quick Start](#1-quick-start)
2. [Top-Level Structure](#2-top-level-structure)
3. [Voices & Domains](#3-voices--domains)
4. [Meter Configuration](#4-meter-configuration)
5. [Built-in Rules](#5-built-in-rules)
6. [Dynamic Rules & Heuristics](#6-dynamic-rules--heuristics)
7. [Wildcard Constraints](#7-wildcard-constraints)
8. [Search Options (DETAILED)](#8-search-options-detailed)
9. [Output Options](#9-output-options)
10. [Complete Parameter Reference](#10-complete-parameter-reference)
11. [CRITICAL: How Heuristics Interact with Rules](#11-critical-how-heuristics-interact-with-rules)
12. [Common Patterns](#12-common-patterns)

## 1. Quick Start

CLI:

```bash
bin/dynamic-solver configs/metric_domain_example.json
```

Wrapper path (same as used by Max):

```bash
./bin/test-max-wrapper configs/metric_domain_example.json
```

## 2. Top-Level Structure

```json
{
  "name": "My Config",
  "description": "Configuration description",
  "solution_length": 8,
  "num_voices": 2,
  "score_length": "12q",
  "voices": [ ... ],
  "meter": { ... },
  "rules": [ ... ],
  "dynamic_rules": [ ... ],
  "search_options": { ... },
  "output_options": { ... }
}
```

**Required fields:**

- `solution_length`: Number of notes per voice (integer)
- `num_voices`: Number of voices (integer)
- `voices`: Array of voice configurations

**Optional fields:**

- `name`, `description`: Metadata strings
- `score_length`: Total musical score length (e.g., `"8q"` = 8 quarter notes)
- `meter`: Metric signature configuration
- `rules`: Built-in constraints
- `dynamic_rules`: Expression-based constraints and heuristics
- `search_options`: Search behavior configuration
- `output_options`: Export and display options

## 3. Voices & Domains

Voices define the pitch and rhythm domains for each voice.

```json
"voices": [
  {
    "rhythm": {
      "duration_values": ["1/8", "1/4", "1/2"],
      "description": "Eighth notes, quarters, or halves"
    },
    "pitch": {
      "midi_values": [60, 62, 64, 65, 67, 69, 71, 72],
      "description": "C major scale"
    }
  },
  {
    "rhythm": {
      "duration_values": ["1/4", "-1/4"],
      "description": "Quarters or quarter rests"
    },
    "pitch": {
      "midi_values": [48, 50, 52, 53, 55, 57, 59, 60],
      "description": "Low register"
    }
  }
]
```

**Rhythm values:**

- Positive fractions: `"1/16"`, `"1/8"`, `"1/4"`, `"1/2"`, `"1/1"`, `"2/1"`, etc.
- Negative fractions are rests: `"-1/4"` = quarter rest
- Triplet support: `"1/12"` (triplet eighth)

**Pitch values:**

- MIDI note numbers (0–127)
- Each voice can have any subset of the MIDI range
- Used to constrain variable domains during search

## 4. Meter Configuration

```json
"meter": {
  "time_signatures": ["4/4", "3/4", "6/8"],
  "tuplets": [3, 8],
  "beat_divisions": [2, 3, 4]
}
```

**Meter fields:**

- `time_signatures`: Array of allowed time signatures.
- `tuplets`: Subdivision factors that **require tuplet notation** (e.g. `[3, 6, 12]` for triplets and sextuplets in a binary meter).
- `beat_divisions`: All subdivision factors that exist for each beat, including both native and tuplet ones.

**Tuplet vs. non-tuplet subdivisions:**  
A `beat_divisions` entry is treated as a _native_ (non-tuplet) subdivision unless the same value also appears in `tuplets`. This means the classification is meter-aware:

| Meter | `tuplets` | `beat_divisions` | 3 is…  |
| ----- | --------- | ---------------- | ------ |
| 4/4   | `[3]`     | `[2, 3, 4]`      | tuplet |
| 6/8   | `[2]`     | `[3]`            | native |

Metric rules (`r-metric-hierarchy`) use this distinction to filter allowed rhythm values when `"no-tuplets"` is specified.

Metric signature rules are automatically targeted to the metric engine (last engine).

## 5. Built-in Rules

### 5.1 Single-Voice Rules (`r-one-voice`)

`r-one-voice` is the canonical rule type for constraints that target **one component of one or more voices**. Use `target_component` to select `"pitch"` or `"rhythm"`.

**Pitch — all different (twelve-tone row or unique-pitch sequence):**

```json
{
  "rule_type": "r-one-voice",
  "constraint": "all_different",
  "indices": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
  "target_voices": [0],
  "target_component": "pitch",
  "enabled": true
}
```

Apply the same rule to multiple voices at once:

```json
{
  "rule_type": "r-one-voice",
  "constraint": "all_different",
  "target_voices": [0, 1, 2, 3],
  "target_component": "pitch"
}
```

**Rhythm — all different (every note has a unique duration):**

```json
{
  "rule_type": "r-one-voice",
  "constraint": "all_different",
  "target_voices": [0],
  "target_component": "rhythm"
}
```

**Twelve-tone explicit (legacy alias, same behaviour):**

```json
{
  "rule_type": "r-twelve-tone-voice1",
  "constraint": "all_different",
  "indices": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
  "target_voices": [0],
  "target_component": "pitch"
}
```

> **Migration note:** `r-pitches-one-engine` is a legacy alias for `r-one-voice` with `target_component: "pitch"`. It continues to work but new configs should use `r-one-voice`.

### 5.2 Cross-Voice Pitch Rules

**Perfect Fifth Intervals:**

```json
{
  "rule_type": "r-perfect-fifth-intervals",
  "constraint": "consecutive_perfect_fifths",
  "target_voices": [0, 1],
  "target_component": "pitch"
}
```

**Palindrome (voice N is exact retrograde of voice M):**

```json
{
  "rule_type": "r-palindrome-voice2",
  "constraint": "palindrome_of_engine",
  "parameters": [1, 3],
  "indices": [0, 1, 2, 3, 4, 5, 6, 7],
  "target_voices": [0, 1],
  "target_component": "pitch"
}
```

`parameters`: optional rule-specific values only. For voice relationships, use `target_voices` plus `target_component`.

**Retrograde Inversion:**

```json
{
  "rule_type": "r-cross-voice-retrograde-inversion",
  "constraint": "retrograde_inversion_relationship",
  "target_voices": [0, 1],
  "target_component": "pitch"
}
```

**No Unisons:**

```json
{
  "rule_type": "r-cross-voice-no-unisons",
  "constraint": "no_unisons_between_engines",
  "target_voices": [0, 1],
  "indices": [0, 1, 2, 3, 4, 5, 6, 7],
  "target_component": "pitch"
}
```

### 5.3 Rhythm Rules

**Uniform Duration (all notes share the same value):**

```json
{
  "rule_type": "r-uniformity",
  "constraint": "equal_values",
  "parameters": ["1/4"],
  "indices": [0, 1, 2, 3, 4, 5, 6, 7],
  "target_voices": [0],
  "target_component": "rhythm"
}
```

### 5.4 Metric Hierarchy Rules (`r-metric-hierarchy`)

`r-metric-hierarchy` constrains rhythm values relative to the beat grid defined in `meter`. It is **automatically targeted to rhythm engines** — do not add `target_component` or `engine_type`.

**Legacy no-tuplets (only native beat subdivisions; no triplets or other tuplet durations):**

```json
{
  "rule_type": "r-metric-hierarchy",
  "constraint": "equal",
  "parameters": ["durations", "no-tuplets"],
  "target_voices": [0, 1]
}
```

The allowed set is derived from `meter.beat_divisions` entries that are **not** listed in `meter.tuplets`. For example with `beat_divisions: [2, 3, 4]` and `tuplets: [3]`, only divisions by 2 and 4 (eighth and sixteenth) are admitted.

**Strict grid floor (no duration shorter than a given value):**

```json
{
  "rule_type": "r-metric-hierarchy",
  "constraint": "min_grid",
  "parameters": ["min-grid", "1/8"],
  "target_voices": [0, 1]
}
```

**Tuplet alignment (tuplet durations must be onset/end-aligned to the tuplet grid):**

```json
{
  "rule_type": "r-metric-hierarchy",
  "constraint": "tuplet_on_beat_start",
  "target_voices": [0, 1]
}
```

**Voice-onset hierarchy (every onset of the coarse voice also appears in the fine voice):**

```json
{
  "rule_type": "r-metric-hierarchy",
  "constraint": "hierarchical_voices",
  "parameters": ["0<-1"],
  "target_voices": [0, 1]
}
```

The `"0<-1"` parameter means voice 0 is the fine voice and voice 1 is the coarse voice.

`r-uniformity` works on whichever component you target. For pitch, provide MIDI values or integers in the pitch domain; for rhythm, provide duration strings or tick values.

### 5.5 Time Signature Rules

**Fixed time signature:**

```json
{
  "rule_type": "r-time-signature",
  "constraint": "equal_values",
  "parameters": ["4/4"],
  "indices": [0],
  "target_component": "metric"
}
```

**Time signature with timepoints (changes at specific positions):**

```json
{
  "rule_type": "r-time-signature",
  "constraint": "equal_values",
  "parameters": ["4/4", "3/4", "6/8"],
  "timepoints": ["0q", "4q", "7q"]
}
```

Timepoints are positions in quarter-note units where time signatures can change.

### 5.6 Verbose Form (still supported)

The shorthand `"constraint": "..."` expands to:

```json
{
  "rule_type": "r-one-voice",
  "constraint_function": {
    "type": "builtin",
    "function": "all_different",
    "parameters": []
  },
  "indices": [0, 1, 2, 3, 4, 5, 6, 7],
  "target_voices": [0],
  "target_component": "pitch"
}
```

### 5.7 Common Rule Parameters

| Parameter          | Type   | Description                                                                                          |
| ------------------ | ------ | ---------------------------------------------------------------------------------------------------- |
| `rule_type`        | string | `r-one-voice`, `r-uniformity`, `r-metric-hierarchy`, `r-time-signature`, `r-palindrome-voice2`, etc. |
| `constraint`       | string | Built-in function: `all_different`, `equal_values`, `palindrome_of_engine`, `min_grid`, etc.         |
| `parameters`       | array  | Constraint parameters (rhythm values, time signatures, engine indices)                               |
| `target_voices`    | array  | Multiple voices: `[0, 1]`                                                                            |
| `target_component` | string | `"pitch"`, `"rhythm"`, or `"metric"`                                                                 |
| `indices`          | array  | Positions in sequence — omit to apply to all                                                         |
| `timepoints`       | array  | Quarter-note positions for metric rules: `["0q", "4q"]`                                              |
| `enabled`          | bool   | Enable/disable rule (default: `true`)                                                                |
| `priority`         | int    | Rule priority (higher = tried first)                                                                 |
| `description`      | string | Human-readable label                                                                                 |

> **Compatibility:** Legacy aliases `target_voice` and `voice` are auto-normalized. Always use `target_voices` in new configs.

## 6. Dynamic Rules & Heuristics

Dynamic rules use string expressions and come in two types: hard constraints and heuristics.

### 6.1 Hard Constraints (`basic_constraint`)

Hard constraints are Boolean: a candidate either passes or fails.

```json
{
  "id": "step_motion",
  "type": "basic_constraint",
  "mode": "true_false",
  "expression": "abs(voice[0].pitch[i+1] - voice[0].pitch[i]) <= 2",
  "enabled": true,
  "priority": 9,
  "description": "Stepwise melodic motion (max 2 semitones)"
}
```

**Expression features:**

- Math: `+ - * / abs() max() min()`
- Comparison: `== != < > <= >=`
- Logic: `&& || ! not`
- Membership: `in`, `not_in` with integer array literals
- Variables: `voice[N].pitch[i]`, `voice[N].rhythm[i]`, `?current`

Examples:

- `abs(voice[1].pitch[i] - voice[0].pitch[i]) in [0, 5, 7, 12]`
- `abs(voice[2].pitch[i] - voice[1].pitch[i]) not_in [1, 2, 6, 10, 11]`
- `not (voice[0].pitch[i] == voice[1].pitch[i])`

**Modes:**

- `"true_false"`: Boolean constraint (default)
- `"real_heuristic"`: Treat as heuristic energy

### 6.2 Heuristic Energy (`heuristic_energy`)

Heuristics score candidates numerically. **Higher score = tried first.** They do not make any value valid or invalid — they only affect search order.

```json
{
  "id": "prefer_fifths",
  "type": "heuristic_energy",
  "mode": "real_heuristic",
  "expression": "24 - abs((voice[1].pitch[i] - voice[0].pitch[i]) - 7)",
  "weight": 10,
  "priority": 1,
  "direction": "maximize",
  "enabled": true,
  "description": "Prefer voice 1 to be exactly a perfect fifth above voice 0"
}
```

**Heuristic-specific parameters:**

| Parameter         | Default      | Description                                                                 |
| ----------------- | ------------ | --------------------------------------------------------------------------- |
| `weight`          | 1            | Relative importance among heuristics                                        |
| `priority`        | 0            | Scoring bucket (higher priority evaluated first in tie-breaking)            |
| `direction`       | `"maximize"` | `"maximize"` = higher score preferred; `"minimize"` = lower score preferred |
| `candidate_voice` | —            | Restrict heuristic to a specific voice                                      |
| `wildcard_type`   | —            | See Section 7                                                               |

### 6.3 Expression Variables

| Syntax                | Meaning                                            |
| --------------------- | -------------------------------------------------- |
| `voice[0].pitch[i]`   | Pitch of voice 0 at position i                     |
| `voice[1].rhythm[i]`  | Rhythm of voice 1 at position i                    |
| `voice[v].pitch[i+1]` | Wildcard voice v, next position                    |
| `?current`            | The candidate value being scored (heuristics only) |
| `i`                   | Current position index                             |
| `v`                   | Wildcard voice index                               |

### 6.4 `constraint` as alias for `expression`

For dynamic rules, `constraint` normalizes to `expression`:

```json
{
  "id": "no_repeat",
  "type": "basic_constraint",
  "mode": "true_false",
  "constraint": "voice[0].pitch[i] != voice[0].pitch[i+1]"
}
```

## 7. Wildcard Constraints

Wildcards expand a single constraint template across all positions or voices automatically.

### 7.1 `for_all_positions`

Expands constraint at every position `i=0,1,...,length-1`.

```json
{
  "id": "stepwise_all",
  "rule_type": "wildcard_constraint",
  "wildcard_type": "for_all_positions",
  "pattern_offsets": [0, 1],
  "constraint": "abs(voice[0].pitch[i+1] - voice[0].pitch[i]) <= 2",
  "target_voices": [0],
  "target_component": "pitch"
}
```

If you include `indices`, expansion is restricted to those positions only.

```json
{
  "id": "strong_beats_only",
  "rule_type": "wildcard_constraint",
  "wildcard_type": "for_all_positions",
  "constraint": "abs(voice[1].pitch[i] - voice[0].pitch[i]) in [0, 5, 7, 12]",
  "indices": [0, 4, 8, 12, 16, 20],
  "target_voices": [0, 1],
  "target_component": "pitch"
}
```

### 7.2 `for_all_voices`

Expands `voice[v]` to every voice in `target_voices`.

```json
{
  "id": "stepwise_all_voices",
  "rule_type": "wildcard_constraint",
  "wildcard_type": "for_all_voices",
  "pattern_offsets": [0, 1],
  "constraint": "abs(voice[v].pitch[i+1] - voice[v].pitch[i]) <= 2",
  "target_voices": [0, 1],
  "target_component": "pitch"
}
```

### 7.3 `sliding_window`

Applies constraint to every consecutive pair `(i, i+1)`.

```json
{
  "id": "ascending",
  "rule_type": "wildcard_constraint",
  "wildcard_type": "sliding_window",
  "pattern_offsets": [0, 1],
  "constraint": "voice[0].pitch[i+1] == voice[0].pitch[i] + 4",
  "target_voices": [0],
  "target_component": "pitch"
}
```

### 7.4 Wildcard used in `dynamic_rules`

Wildcards work in `dynamic_rules` too, for both constraints and heuristics:

```json
{
  "id": "prefer_fifths_all_positions",
  "type": "heuristic_energy",
  "wildcard_type": "for_all_positions",
  "expression": "24 - abs((voice[1].pitch[i] - voice[0].pitch[i]) - 7)",
  "candidate_voice": 1,
  "weight": 10,
  "direction": "maximize"
}
```

### 7.5 Wildcard Parameters

| Parameter          | Purpose                                                 |
| ------------------ | ------------------------------------------------------- |
| `wildcard_type`    | `for_all_positions`, `for_all_voices`, `sliding_window` |
| `pattern_offsets`  | Offset tuple for the window, e.g. `[0, 1]`              |
| `constraint`       | Expression using `i`, `v`, `v1`, `v2`                   |
| `target_voices`    | Voices to which the wildcard applies                    |
| `target_component` | `"pitch"` or `"rhythm"`                                 |

`indices` behavior is the same across CLI and Max-wrapper paths because both use the same wildcard compiler.

### 7.6 Harmonic Consonance Pattern (4 Voices, 4/4)

See `configs/harmonic_consonance_4voice.json` for a complete working setup.

Pattern summary:

- 4 voices
- Fixed metric domain: `4/4`
- Rhythmic domain: quarter notes only (`"1/4"`)
- Pitch domain: MIDI `48..72` (explicit `midi_values`)
- `solution_length`: `24`
- Adjacent voice-pair pitch rules `(0,1)`, `(1,2)`, `(2,3)`
- Strong beats (1 and 3): perfect consonances `[0, 5, 7, 12]`
- Weak beats (2 and 4): imperfect consonances `[3, 4, 8, 9]`

For a 24-note quarter-note sequence in `4/4`, beat index groups are:

- Beats 1 and 3: `[0, 4, 8, 12, 16, 20, 2, 6, 10, 14, 18, 22]`
- Beats 2 and 4: `[1, 5, 9, 13, 17, 21, 3, 7, 11, 15, 19, 23]`

## 8. Search Options (DETAILED)

Controls how the solver searches for solutions. None of these affect whether constraints are applied — all rules run regardless.

```json
"search_options": {
  "branching": "first_fail",
  "value_order": "heuristic",
  "restart_policy": "none",
  "timeout_ms": 30000,
  "max_solutions": 1,
  "random_seed": 0,
  "heuristic_top_k": 0,
  "heuristic_trace": false
}
```

### 8.1 `branching` — Which variable to branch on

- **`first_fail`** (recommended): Pick the variable with the smallest remaining domain. Prunes the search tree most aggressively.
- **`input_order`**: Pick variables in declaration order. Simpler but less efficient.

### 8.2 `value_order` — Which value to try first

- **`min`** (default): Try the smallest value first. Fully deterministic.
- **`random`**: Try values in random order. Requires `random_seed > 0` for reproducibility.
- **`heuristic`**: Score candidates using `heuristic_energy` rules from `dynamic_rules`, try higher-scoring candidates first. When candidates are tied, selection depends on `random_seed` (see below).

### 8.3 `random_seed` — Randomness control

| Value                | Behavior                                                  |
| -------------------- | --------------------------------------------------------- |
| omitted / `max_uint` | Deterministic, no randomness                              |
| `0`                  | Fresh random seed per solve → different solution each run |
| `N > 0`              | Fixed seed → same solution every run (reproducible)       |

### 8.4 Heuristic + random_seed interaction

This is the most important combination to understand.

When `value_order: "heuristic"` and `random_seed: 0`:

- Candidates are scored by heuristic expressions
- **Tied candidates** (same score) are randomly selected
- Enables diverse solutions while preserving heuristic preference
- All hard constraints (palindrome, 12-tone, etc.) still apply — heuristics never bypass them

When `value_order: "heuristic"` and `random_seed > 0`:

- Tied candidates are resolved by deterministic hash → same solution every run

### 8.5 Other parameters

| Parameter         | Default  | Description                                                       |
| ----------------- | -------- | ----------------------------------------------------------------- |
| `restart_policy`  | `"none"` | `"none"` or `"luby"` — Luby restarts can help escape local minima |
| `timeout_ms`      | 30000    | Time limit in milliseconds                                        |
| `max_solutions`   | 1        | Number of solutions to collect (0 = unlimited)                    |
| `heuristic_top_k` | 0        | Only evaluate top K candidates for heuristic scoring (0 = all)    |
| `heuristic_trace` | false    | Print heuristic scores during search (for debugging)              |

## 9. Output Options

```json
"output_options": {
  "export_path": "/path/to/output",
  "export_filename": "my_solution",
  "export_json": true,
  "export_txt": true,
  "export_xml": true,
  "export_png": false,
  "export_midi": true,
  "show_statistics": true,
  "include_analysis": true,
  "analysis_options": {
    "harmonic_analysis": true,
    "interval_analysis": true,
    "palindrome_verification": true
  }
}
```

| Parameter          | Description                                            |
| ------------------ | ------------------------------------------------------ |
| `export_path`      | Output directory (required if any export flag is true) |
| `export_filename`  | Base filename for all exported files                   |
| `export_json`      | Save raw solution data as JSON                         |
| `export_txt`       | Save human-readable text summary                       |
| `export_xml`       | Save as MusicXML (viewable in notation software)       |
| `export_png`       | Render notation as PNG                                 |
| `export_midi`      | Save as MIDI audio file                                |
| `show_statistics`  | Print solve time, rules checked, etc.                  |
| `include_analysis` | Compute harmonic and melodic analysis                  |
| `analysis_options` | Fine-grained analysis settings                         |

## 10. Complete Parameter Reference

### Top-Level

| Field             | Type   | Required | Description                                 |
| ----------------- | ------ | -------- | ------------------------------------------- |
| `solution_length` | int    | ✅       | Notes per voice                             |
| `num_voices`      | int    | ✅       | Number of voices                            |
| `voices`          | array  | ✅       | Voice domain definitions                    |
| `name`            | string |          | Config label                                |
| `description`     | string |          | Human-readable description                  |
| `score_length`    | string |          | e.g. `"8q"` = 8 quarter notes               |
| `meter`           | object |          | Metric engine configuration                 |
| `rules`           | array  |          | Built-in constraints                        |
| `dynamic_rules`   | array  |          | Expression-based constraints and heuristics |
| `search_options`  | object |          | Search strategy                             |
| `output_options`  | object |          | Export configuration                        |

### Rule Object

| Field              | Type   | Description                                                                                                                                                                                                           |
| ------------------ | ------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `rule_type`        | string | `r-one-voice`, `r-uniformity`, `r-metric-hierarchy`, `r-time-signature`, `r-twelve-tone-voice1`, `r-palindrome-voice2`, `r-cross-voice-no-unisons`, `r-perfect-fifth-intervals`, `r-cross-voice-retrograde-inversion` |
| `constraint`       | string | Shorthand: `all_different`, `equal_values`, `palindrome_of_engine`, `no_unisons_between_engines`, `consecutive_perfect_fifths`, `retrograde_inversion_relationship`                                                   |
| `parameters`       | array  | Constraint-specific params                                                                                                                                                                                            |
| `target_voices`    | array  | Multiple voices `[0, 1]`                                                                                                                                                                                              |
| `target_component` | string | `"pitch"`, `"rhythm"`, `"metric"`                                                                                                                                                                                     |
| `indices`          | array  | Positions — omit to apply to all                                                                                                                                                                                      |
| `timepoints`       | array  | Quarter-note positions for metric changes                                                                                                                                                                             |
| `enabled`          | bool   | On/off (default `true`)                                                                                                                                                                                               |
| `priority`         | int    | Higher = tried first                                                                                                                                                                                                  |
| `id`               | string | Optional label                                                                                                                                                                                                        |
| `description`      | string | Optional documentation                                                                                                                                                                                                |

### Dynamic Rule Object

| Field             | Type   | Description                                                   |
| ----------------- | ------ | ------------------------------------------------------------- |
| `id`              | string | Rule label                                                    |
| `type`            | string | `"basic_constraint"` or `"heuristic_energy"`                  |
| `mode`            | string | `"true_false"` (hard) or `"real_heuristic"` (soft)            |
| `expression`      | string | Constraint or scoring expression                              |
| `constraint`      | string | Alias for `expression`                                        |
| `weight`          | number | Heuristic importance (default 1)                              |
| `priority`        | int    | Heuristic scoring bucket                                      |
| `direction`       | string | `"maximize"` or `"minimize"`                                  |
| `wildcard_type`   | string | `"for_all_positions"`, `"for_all_voices"`, `"sliding_window"` |
| `pattern_offsets` | array  | Window offsets e.g. `[0, 1]`                                  |
| `candidate_voice` | int    | Target voice for heuristic                                    |
| `enabled`         | bool   | On/off (default `true`)                                       |
| `description`     | string | Optional documentation                                        |

## 11. CRITICAL: How Heuristics Interact with Rules

**Heuristics affect search order. They never affect constraint validity.**

The search process:

```
1. Select a variable          → branching (first_fail / input_order)
2. Propose a value            → value_order (min / random / heuristic)
3. Check ALL constraints      → rules + dynamic_rules (always applied)
   ✓ Hard constraints: value PASSES or FAILS
   ✓ Heuristic scores: only determined ORDERING of step 2
4. If pass → go deeper
   If fail → backtrack
```

**What this means in practice:**

- Setting `value_order: "heuristic"` does not weaken or bypass any rule
- Palindrome constraints, 12-tone rows, stepwise limits — all enforced exactly the same
- Heuristics only change _which value is proposed first_ at each variable
- If the best heuristic candidate violates a constraint, it is rejected and the next candidate is tried

**Stochastic tie-breaking explained:**

When `value_order: "heuristic"` and `random_seed: 0`:

- Many candidates may score identically (especially when cross-voice context isn't yet assigned)
- The solver randomly selects from the tied group
- This gives **different valid solutions on each run**, all satisfying all constraints
- Use this to get solution diversity without giving up musical structure

**Example — Heuristic + Palindrome:**

```
Heuristic: prefer perfect fifths between voices
Constraint: voice 1 is palindrome of voice 0

Run 1 → [D E F# G A B C# D] + palindrome + fifths ✓
Run 2 → [A G F# E D C# B A] + palindrome + fifths ✓
Run 3 → [C D E F G A B C]   + palindrome + fifths ✓

All different sequences. All palindromes. All prefer fifths.
```

The heuristic guides _which solutions are found first_, not _which solutions are valid_.

## 12. Common Patterns

### Twelve-Tone Row with Palindrome

```json
{
  "solution_length": 12,
  "num_voices": 2,
  "rules": [
    {
      "rule_type": "r-twelve-tone-voice1",
      "constraint": "all_different",
      "indices": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
      "target_voices": [0],
      "target_component": "pitch"
    },
    {
      "rule_type": "r-palindrome-voice2",
      "constraint": "palindrome_of_engine",
      "parameters": [1, 3],
      "indices": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
      "target_voices": [0, 1],
      "target_component": "pitch"
    }
  ]
}
```

### Stepwise Motion with Solution Diversity

```json
{
  "dynamic_rules": [
    {
      "id": "prefer_stepwise",
      "type": "heuristic_energy",
      "wildcard_type": "for_all_positions",
      "expression": "max(0, 3 - abs(voice[0].pitch[i+1] - voice[0].pitch[i]))",
      "weight": 10,
      "direction": "maximize"
    }
  ],
  "search_options": {
    "value_order": "heuristic",
    "random_seed": 0
  }
}
```

### Hard Constraint + Heuristic Preference

```json
{
  "rules": [
    {
      "rule_type": "r-one-voice",
      "constraint": "all_different",
      "target_voices": [0],
      "target_component": "pitch"
    }
  ],
  "dynamic_rules": [
    {
      "id": "prefer_center",
      "type": "heuristic_energy",
      "wildcard_type": "for_all_positions",
      "expression": "24 - abs(?current - 66)",
      "weight": 5
    }
  ]
}
```

Result: All-different notes, biased toward middle C (MIDI 66).

### Parallel Motion (Strict)

```json
{
  "dynamic_rules": [
    {
      "id": "parallel_fifths",
      "type": "basic_constraint",
      "mode": "true_false",
      "wildcard_type": "for_all_positions",
      "expression": "(voice[1].pitch[i] - voice[0].pitch[i]) == 7",
      "target_voices": [0, 1],
      "target_component": "pitch"
    }
  ]
}
```

### Mixed Time Signature

```json
{
  "rules": [
    {
      "rule_type": "r-time-signature",
      "constraint": "equal_values",
      "parameters": ["4/4", "3/4"],
      "timepoints": ["0q", "4q"]
    }
  ]
}
```

---

## Legacy Compatibility

The wrapper still normalizes legacy patterns:

- `domains.voice_domains`, `note_domain`
- `configuration` blocks
- legacy rule targeting (`voice`, wildcard scopes, inferred voice refs)
- built-in shorthand `constraint` and dynamic rule `constraint` alias

**Deprecated rule types (still accepted, prefer `r-one-voice`):**

| Legacy type               | Replacement                                 |
| ------------------------- | ------------------------------------------- |
| `r-pitches-one-engine`    | `r-one-voice` + `target_component: "pitch"` |
| `r-pitches-all-different` | `r-one-voice` + `target_component: "pitch"` |

**Deprecated fields on `r-metric-hierarchy` rules (now inferred automatically):**

- `target_component: "rhythm"` — metric hierarchy rules always target rhythm engines; omit this field.
- `engine_type: "rhythm"` — same as above; omit.

Use the modern voice-first contract for all new configs.

## Common Errors

- **`missing target_voice/target_voices`**: Add `target_voice` or `target_voices` + `target_component` for non-metric built-ins.
- **`'engine_domains' is deprecated`**: Migrate to top-level `voices`.
- **Metric timepoint errors**: Ensure `timepoints` are strictly increasing and within `score_length`.
- **No solutions found with heuristic**: Heuristics never relax constraints — check that constraints aren't contradictory independent of heuristics.

## References

- [README.md](README.md)
- [configs/cluster_config_schema.json](configs/cluster_config_schema.json)
