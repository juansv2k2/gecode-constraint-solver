# Engine Usage Guide

This document describes how to use the musical constraint solver: how to write configuration files, what rules are available, how domains work, and what every configuration option does.

---

## Table of Contents

1. [Quick Start](#1-quick-start)
2. [Config File Structure](#2-config-file-structure)
3. [Top-Level Options](#3-top-level-options)
4. [Engine Architecture](#4-engine-architecture)
5. [Domains — `engine_domains`](#5-domains--engine_domains)
6. [Constraint Rules](#6-constraint-rules)
   - 6.0 [Rules vs Dynamic Rules](#60-rules-vs-dynamic-rules)
   - 6.1 [Legacy Built-in Rules](#61-legacy-built-in-rules)
   - 6.2 [Wildcard Constraint Rules](#62-wildcard-constraint-rules)
   - 6.3 [Index Rules](#63-index-rules)
   - 6.4 [Common Rule Fields](#64-common-rule-fields)
7. [Constraint Expressions](#7-constraint-expressions)
   - 7.1 [Variable Syntax](#71-variable-syntax)
   - 7.2 [Operators](#72-operators)
   - 7.3 [Functions](#73-functions)
   - 7.4 [Expression Examples](#74-expression-examples)
8. [Output Options](#9-output-options)
9. [Heuristic Rules (Score-Based Ordering)](#10-heuristic-rules-score-based-ordering)
   - 10.1 [Heuristic Types: heuristic_preference vs heuristic_energy](#101-heuristic-types-heuristic_preference-vs-heuristic_energy)
   - 10.2 [Heuristic Rule Fields](#102-heuristic-rule-fields)
   - 10.3 [Scoring Semantics](#103-scoring-semantics)
   - 10.4 [Priority Buckets](#104-priority-buckets)
   - 10.5 [Candidate Context Variables](#105-candidate-context-variables)
   - 10.6 [Basic Heuristic Example](#106-basic-heuristic-example)
   - 10.7 [Wildcard Heuristic Rules](#107-wildcard-heuristic-rules)

- 10.7.3 [for_all_positions Example](#1073-for_all_positions-example)
- 10.7.4 [for_all_voices Example](#1074-for_all_voices-example)
  - 10.8 [Wildcard Types & Iteration Semantics](#108-wildcard-types--iteration-semantics)
  - 10.9 [Wildcard Placeholder Reference](#109-wildcard-placeholder-reference)
  - 10.10 [Operational Notes](#1010-operational-notes)
  - 10.11 [Reference Configs](#1011-reference-configs)

8. [Search Options & Solver Tuning](#8-search-options--solver-tuning)
9. [Full Config Example](#11-full-config-example)
10. [Output Files](#12-output-files)
11. [Common Errors](#13-common-errors)

---

## 1. Quick Start

```bash
bin/dynamic-solver configs/my_config.json
```

The solver reads a JSON config, posts constraints, runs Gecode search, and writes result files to the path set in `export_path`.

Front-end note:

- Use `bin/dynamic-solver` for CLI workflows.
- Use `bin/test-max-wrapper` to reproduce/debug the async wrapper behavior outside Max.
- Use the Max external for patcher workflows; see [usage-in-max.md](usage-in-max.md).

---

## 2. Config File Structure

Every config file is a JSON object with these top-level sections:

```json
{
  "name": "...",
  "description": "...",

  "solution_length": 8,
  "num_voices": 2,
  "backtrack_method": "intelligent",
  "export_path": "tests/output",

  "engine_architecture": { ... },   // optional, explicit engine layout

  "engine_domains": { ... },        // required — pitch & rhythm domains per voice

  "rules": [ ... ],                 // legacy built-in + wildcard_constraint rules
  "dynamic_rules": [ ... ],         // modern expression-first hard + heuristic rules

  "search_options": { ... },        // optional — control solver behavior + heuristics
  "output_options": { ... }         // optional — control output format
}
```

---

## 3. Top-Level Options

| Field              | Type   | Default          | Description                                   |
| ------------------ | ------ | ---------------- | --------------------------------------------- |
| `name`             | string | `""`             | Human-readable name for the problem           |
| `description`      | string | `""`             | Free-text description                         |
| `solution_length`  | int    | `12`             | Number of notes per voice                     |
| `num_voices`       | int    | `2`              | Number of simultaneous voices                 |
| `backtrack_method` | string | `"intelligent"`  | Search strategy (see below)                   |
| `export_path`      | string | `"tests/output"` | Directory for output files                    |
| `num_engines`      | int    | auto             | Override total engine count (usually omitted) |

**`backtrack_method` values:**

| Value           | Behaviour                               |
| --------------- | --------------------------------------- |
| `"intelligent"` | Context-aware backjumping (recommended) |
| `"simple"`      | Basic chronological backtracking        |
| `"none"`        | No backtracking (first-assignment only) |

---

## 4. Engine Architecture

The solver uses a multi-engine model inspired by Cluster-Engine. Each voice gets two engines: one for **rhythm** and one for **pitch**. A shared **metric engine** handles time signatures.

**Default layout for N voices:**

| Engine index | Role                     |
| ------------ | ------------------------ |
| 0            | Voice 0 — rhythm         |
| 1            | Voice 0 — pitch          |
| 2            | Voice 1 — rhythm         |
| 3            | Voice 1 — pitch          |
| …            | …                        |
| 2N           | Metric (time signatures) |

You can document the layout explicitly in the config (optional, informational only):

```json
"num_engines": 5,
"engine_architecture": {
  "voice_0": { "rhythm_engine": 0, "pitch_engine": 1 },
  "voice_1": { "rhythm_engine": 2, "pitch_engine": 3 },
  "metric_engine": 4
}
```

Rules reference engines by their index number (see `target_engine` below).

---

## 5. Domains — `engine_domains`

Both pitch and rhythm engines **must** declare explicit domains. There are no defaults — if any voice is missing a pitch or rhythm domain the solver stops immediately with a descriptive error.

```json
"engine_domains": {
  "engine_0": {
    "type": "rhythm",
    "voice": 0,
    "duration_values": ["1/4"],
    "description": "Voice 0: quarter notes only"
  },
  "engine_1": {
    "type": "pitch",
    "voice": 0,
    "midi_values": [60, 62, 64, 65, 67, 69, 71, 72],
    "description": "Voice 0: C major scale (one octave)"
  },
  "engine_2": {
    "type": "rhythm",
    "voice": 1,
    "duration_values": ["1/4"],
    "description": "Voice 1: quarter notes only"
  },
  "engine_3": {
    "type": "pitch",
    "voice": 1,
    "midi_values": [60, 62, 64, 65, 67, 69, 71, 72],
    "description": "Voice 1: C major scale (one octave)"
  }
}
```

**Pitch domain fields:**

| Field         | Required | Description                                        |
| ------------- | -------- | -------------------------------------------------- |
| `type`        | yes      | `"pitch"`                                          |
| `voice`       | yes      | Voice index this engine belongs to (0-based)       |
| `midi_values` | yes      | Explicit list of allowed MIDI note numbers (0–127) |
| `description` | no       | Free-text label                                    |

**Rhythm domain fields:**

| Field             | Required | Description                                                                                                                                                      |
| ----------------- | -------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `type`            | yes      | `"rhythm"`                                                                                                                                                       |
| `voice`           | yes      | Voice index this engine belongs to (0-based)                                                                                                                     |
| `duration_values` | yes      | Note-value fractions: `"1/1"` (whole), `"1/2"` (half), `"1/4"` (quarter), `"1/8"` (eighth), `"1/16"` (sixteenth). Dotted values like `"3/8"` are also supported. |
| `description`     | no       | Free-text label                                                                                                                                                  |

> **Note:** Multi-value rhythm domains (e.g. `["1/4", "1/8", "1/2"]`) allow the solver to freely choose a duration for each note position within those options.

#### Rests

Prefix a duration fraction with `-` to make it a rest. A rest occupies time but has no pitch.

```json
"duration_values": ["1/4", "-1/4"]
```

This gives the solver the choice between a quarter note and a quarter rest at every position.

| Duration string | Meaning      |
| --------------- | ------------ |
| `"1/4"`         | Quarter note |
| `"-1/4"`        | Quarter rest |
| `"-1/8"`        | Eighth rest  |
| `"-1/2"`        | Half rest    |

**Rest semantics:**

- A rest position has no pitch. Its pitch output is `R` (console) or `−1` (JSON sentinel).
- Wildcard constraint rules **automatically skip rest positions** — constraints like `abs(voice[0].pitch[i+1] - voice[0].pitch[i]) == 3` are only posted between two consecutive _notes_, never across a rest.
- Interval constraints (semitone distance) are always reified: they only fire when both positions in the window are notes.

**MIDI note reference (selected values):**

| MIDI | Note          |
| ---- | ------------- |
| 60   | C4 (middle C) |
| 62   | D4            |
| 64   | E4            |
| 65   | F4            |
| 67   | G4            |
| 69   | A4            |
| 71   | B4            |
| 72   | C5            |
| 84   | C6            |
| 96   | C7            |

**Important rules for domains:**

- The `engine_domains` section is **required**. The solver throws if it is missing.
- Every voice needs both a pitch engine (`engine_(2N+1)`) and a rhythm engine (`engine_(2N)`).
- Pitch engines must use `midi_values`. Rhythm engines must use `duration_values`. There is no automatic fallback for either.
- Key names must match the engine index: `engine_0`, `engine_1`, `engine_2`, `engine_3`, etc.
- Voice 0: rhythm=`engine_0`, pitch=`engine_1`. Voice 1: rhythm=`engine_2`, pitch=`engine_3`. Voice N: rhythm=`engine_(2N)`, pitch=`engine_(2N+1)`.

---

## 6. Constraint Rules

The solver supports two rule containers with different purposes and authoring styles.

---

### 6.0 Rules vs Dynamic Rules

**`rules` array (legacy):**

- Intended for engine-targeted built-in rules (e.g. `all_different`, `equal_values`)
- Supports classic `wildcard_constraint` rules using expression strings
- Older configs and compatibility layer
- **Best for:** hard constraints, metric setup, cross-engine built-ins

**`dynamic_rules` array (modern):**

- Expression-first rule compilation with dynamic AST evaluation
- Supports three rule type categories:
  - `basic_constraint`: hard constraints with `mode: "true_false"`
  - `heuristic_preference`: boolean preference scoring with `mode: "heur_switch"`
  - `heuristic_energy`: numeric preference scoring with `mode: "real_heuristic"`
- Integrated branch-time heuristic value selector
- **Best for:** new work, especially mixed hard+heuristic objectives

**How and why both exist:**

- Historical configs use `rules` with built-in function names
- Modern workflow uses `dynamic_rules` for cleaner expression-based authoring
- Both are compiled to the same internal Gecode model
- In practice:
  - Use `rules` for legacy built-ins and engine-specific orchestration
  - Use `dynamic_rules` for new work, especially heuristic-guided search

**Quick decision tree:**

- Need `all_different` on a pitch engine? → use `rules` with built-in function
- Need to say "consecutive notes must differ by exactly 4 semitones"? → use `dynamic_rules` with expression
- Need to guide search with preference scores? → use `dynamic_rules` with heuristic modes

---

Rules are listed in either the `"rules"` or `"dynamic_rules"` array. There are three categories: **legacy built-in rules**, **wildcard constraint rules**, and **index rules**.

---

### 6.1 Legacy Built-in Rules

These rules use named constraint functions and target a specific engine by index.

**Structure:**

```json
{
  "rule_type": "<rule-type-id>",
  "constraint_function": {
    "type": "builtin",
    "function": "<function-name>",
    "parameters": [...]
  },
  "indices": [0, 1, 2, 3, 4, 5, 6, 7],
  "target_engine": 1,
  "engine_type": "pitch",
  "enabled": true,
  "priority": 10,
  "description": "..."
}
```

**Available `rule_type` identifiers:**

| `rule_type`                   | Description                                                      |
| ----------------------------- | ---------------------------------------------------------------- |
| `r-pitches-one-engine`        | Applies a pitch constraint to one engine                         |
| `r-rhythmic-uniformity`       | Forces all rhythm values in an engine to one fixed value         |
| `r-metric-signature`          | Sets time signature on the metric engine                         |
| `r-cross-voice-no-unisons`    | Forbids identical pitches at the same position across two voices |
| `r-palindrome-voice`          | Forces one engine to be the exact palindrome of another          |
| `r-twelve-tone-row-generator` | Generates a 12-tone row (all 12 pitch classes, each once)        |

**Available `function` names:**

| Function                     | Parameters     | Effect                                                  |
| ---------------------------- | -------------- | ------------------------------------------------------- |
| `all_different`              | `[]`           | All values in `indices` must be distinct                |
| `equal_values`               | `[N]`          | All values in `indices` must equal N                    |
| `no_unisons_between_engines` | `[engA, engB]` | No matching values at same position between two engines |
| `palindrome_of_engine`       | `[engB, engA]` | Engine B must be the reverse of engine A                |

**Cross-voice rules** target two engines and use `target_engines` instead of `target_engine`:

```json
{
  "rule_type": "r-cross-voice-no-unisons",
  "constraint_function": {
    "type": "builtin",
    "function": "no_unisons_between_engines",
    "parameters": [1, 3]
  },
  "indices": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
  "target_engine": -1,
  "target_engines": [1, 3],
  "engine_type": "pitch",
  "enabled": true,
  "priority": 9,
  "description": "No unisons between Voice 0 and Voice 1"
}
```

**Rhythm rule example** (forces all notes to quarter notes, duration value = 4):

```json
{
  "rule_type": "r-rhythmic-uniformity",
  "constraint_function": {
    "type": "builtin",
    "function": "equal_values",
    "parameters": [4]
  },
  "indices": [0, 1, 2, 3, 4, 5, 6, 7],
  "target_engine": 0,
  "engine_type": "rhythm",
  "enabled": true,
  "priority": 7
}
```

**Metric (time signature) rule example** (sets 4/4):

```json
{
  "rule_type": "r-metric-signature",
  "constraint_function": {
    "type": "builtin",
    "function": "equal_values",
    "parameters": [4]
  },
  "indices": [0],
  "target_engine": 4,
  "engine_type": "metric",
  "enabled": true,
  "priority": 6
}
```

> The `parameters` value for `equal_values` on a metric engine is the numerator of the time signature (e.g. `4` for 4/4, `3` for 3/4).

---

### 6.2 Wildcard Constraint Rules

These rules express constraints as algebraic string expressions that iterate over positions and/or voices automatically. This is the primary way to express custom relationships.

**Structure:**

```json
{
  "id": "my_rule_id",
  "rule_type": "wildcard_constraint",
  "wildcard_type": "sliding_window",
  "pattern_offsets": [0, 1],
  "constraint": "<expression string>",
  "enabled": true,
  "priority": 8,
  "description": "..."
}
```

**`wildcard_type` values:**

| Value                 | Behaviour                                                                                                                 |
| --------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| `"sliding_window"`    | Slides a window of size `max(pattern_offsets)+1` across all positions and all voices, posting the constraint at each step |
| `"for_all_positions"` | Posts the constraint once per position (no sliding)                                                                       |
| `"for_all_voices"`    | Posts the constraint once per voice                                                                                       |

**`pattern_offsets`** defines which relative positions within the window are accessed by `i` in the expression:

- `[0]` — uses only the current position `i`
- `[0, 1]` — uses positions `i` and `i+1` (consecutive pair)
- `[0, 2]` — uses positions `i` and `i+2` (skip one)

The window slides by 1 step at a time, from position 0 to `solution_length - max(pattern_offsets) - 1`.

**Example — consecutive notes must differ by exactly 4 semitones:**

```json
{
  "id": "step_by_4",
  "rule_type": "wildcard_constraint",
  "wildcard_type": "sliding_window",
  "pattern_offsets": [0, 1],
  "constraint": "abs(voice[0].pitch[i+1] - voice[0].pitch[i]) == 4",
  "enabled": true,
  "priority": 9
}
```

**Example — voice 1 always a tritone above voice 0:**

```json
{
  "id": "tritone_above",
  "rule_type": "wildcard_constraint",
  "wildcard_type": "sliding_window",
  "pattern_offsets": [0],
  "constraint": "voice[1].pitch[i] == voice[0].pitch[i] + 6",
  "enabled": true,
  "priority": 8
}
```

---

### 6.3 Index Rules

Index rules pin specific positions of a voice to exact pitch and rhythm values. They are the most direct way to fix a few events while leaving the rest of the sequence free for the solver.

**Structure:**

```json
{
  "id": "fix_voice1",
  "type": "index",
  "voice": 1,
  "events": [
    [0, "1/4", 60],
    [1, "-1/4", null],
    [3, "1/4", 67]
  ],
  "description": "Pin voice 1: pos0=C4 quarter, pos1=quarter rest, pos3=G4 quarter"
}
```

Each entry in `"events"` is a 3-element array: `[position, rhythm, pitch_or_null]`.

| Field in event  | Type              | Description                                                                              |
| --------------- | ----------------- | ---------------------------------------------------------------------------------------- |
| `position`      | int               | 0-based index into the voice sequence                                                    |
| `rhythm`        | string (fraction) | Duration of this event. Use a negative fraction (e.g. `"-1/4"`) for a rest               |
| `pitch_or_null` | int or `null`     | MIDI pitch for a note, or `null` for a rest. Must match the sign of `rhythm` (see below) |

**Validation rules (enforced at parse time):**

| `rhythm` | `pitch` | Result                               |
| -------- | ------- | ------------------------------------ |
| positive | integer | ✅ Note                              |
| negative | `null`  | ✅ Rest                              |
| negative | integer | ❌ Error (rest must have null pitch) |
| positive | `null`  | ❌ Error (note must have a pitch)    |

**Positions not listed in `"events"` are left unconstrained** — the solver may assign any value from the voice's domain to them.

**Multiple voices** can each have their own index rule:

```json
"rules": [
  {
    "id": "fix_v0",
    "type": "index",
    "voice": 0,
    "events": [[0, "1/4", 60]]
  },
  {
    "id": "fix_v1",
    "type": "index",
    "voice": 1,
    "events": [[0, "1/2", 67], [1, "-1/4", null]]
  }
]
```

---

### 6.4 Common Rule Fields

These fields apply to both rule categories:

| Field         | Type   | Required        | Default | Description                                                                    |
| ------------- | ------ | --------------- | ------- | ------------------------------------------------------------------------------ |
| `id`          | string | no              | auto    | Unique identifier for this rule (used in logs)                                 |
| `type`        | string | index only      | —       | Set to `"index"` to use the index rule category                                |
| `rule_type`   | string | legacy/wildcard | —       | Identifies the rule category for legacy and wildcard rules (see above)         |
| `enabled`     | bool   | no              | `true`  | Set to `false` to skip this rule without removing it                           |
| `priority`    | int    | no              | `5`     | Higher priority rules are posted first (cosmetic, does not affect correctness) |
| `description` | string | no              | `""`    | Free-text label shown in output                                                |

---

## 7. Constraint Expressions

Wildcard rules use a string expression parsed into an AST and compiled to Gecode propagators.

---

### 7.1 Variable Syntax

| Syntax                 | Meaning                                                       |
| ---------------------- | ------------------------------------------------------------- |
| `voice[V].pitch[i]`    | Pitch (MIDI number) of voice V at the current window position |
| `voice[V].pitch[i+N]`  | Pitch of voice V at window position offset by N               |
| `voice[V].pitch[i-N]`  | Pitch of voice V at window position shifted back by N         |
| `voice[V].rhythm[i]`   | Duration value of voice V at the current window position      |
| `voice[V].rhythm[i+N]` | Duration of voice V at window position offset by N            |
| `voice[V].rhythm[i-N]` | Duration of voice V at window position shifted back by N      |

- `V` is a literal voice index: `voice[0]`, `voice[1]`, etc.
- `i` is the loop variable (automatically substituted during iteration).
- Offsets must match entries declared in `pattern_offsets`.
- **Duration values:** `voice[V].rhythm[i]` holds the note-value fraction converted to an internal integer. When comparing or constraining rhythm variables, use the same fraction notation as in `duration_values` — the solver maps them to the same internal scale automatically.
- Pitch and rhythm variables can be mixed in a single expression (e.g. cross-domain rules).
- **Rest positions are automatically skipped.** If either position in a sliding window is a rest, the constraint for that window step is not posted. This means pitch-interval rules only apply between consecutive _notes_.

> **No separate rule type for rhythm.** Rhythm constraints use the same `"rule_type": "wildcard_constraint"` as pitch constraints. Switch from `.pitch` to `.rhythm` in the variable name — nothing else changes.

---

### 7.2 Operators

| Operator | Meaning               |
| -------- | --------------------- |
| `==`     | Equal                 |
| `!=`     | Not equal             |
| `<`      | Less than             |
| `<=`     | Less than or equal    |
| `>`      | Greater than          |
| `>=`     | Greater than or equal |
| `+`      | Addition              |
| `-`      | Subtraction           |
| `*`      | Multiplication        |
| `/`      | Integer division      |

> **Important:** Always use `==` for equality, never single `=`.

---

### 7.3 Functions

| Function | Usage                                          | Description         |
| -------- | ---------------------------------------------- | ------------------- |
| `abs(x)` | `abs(voice[0].pitch[i+1] - voice[0].pitch[i])` | Absolute value of x |

Additional functions may be added in future iterations.

---

### 7.4 Expression Examples

**Pitch rules:**

```
// Consecutive notes exactly 3 semitones apart
abs(voice[0].pitch[i+1] - voice[0].pitch[i]) == 3

// Voice 1 always a perfect fifth (7 semitones) above voice 0
voice[1].pitch[i] == voice[0].pitch[i] + 7

// Voice 0 ascending only
voice[0].pitch[i+1] > voice[0].pitch[i]

// Interval between consecutive notes at most 2 semitones
abs(voice[0].pitch[i+1] - voice[0].pitch[i]) <= 2

// Voice 1 mirrors voice 0 at the octave
voice[1].pitch[i] == voice[0].pitch[i] + 12
```

**Rhythm rules** (same rule type, `.rhythm` instead of `.pitch`):

```
// Consecutive durations must be different (no two equal values in a row)
voice[0].rhythm[i+1] != voice[0].rhythm[i]

// Voice 0 rhythm alternates between two values (e.g. eighth then quarter)
// Combine with a domain of [2, 4] to force the two specific values:
voice[0].rhythm[i+1] != voice[0].rhythm[i]

// Voice 1 always has the same duration as voice 0 at every position
voice[1].rhythm[i] == voice[0].rhythm[i]

// Rhythmic augmentation: voice 1 always twice as long as voice 0
voice[1].rhythm[i] == voice[0].rhythm[i] * 2
```

For the rhythm rules above the corresponding rule JSON looks exactly like a pitch rule:

```json
{
  "id": "alternating_rhythm",
  "rule_type": "wildcard_constraint",
  "wildcard_type": "sliding_window",
  "pattern_offsets": [0, 1],
  "constraint": "voice[0].rhythm[i+1] != voice[0].rhythm[i]",
  "enabled": true,
  "priority": 9,
  "description": "Voice 0: no two consecutive durations the same"
}
```

---

## 9. Output Options

```json
"output_options": {
  "export_xml": true,
  "export_png": false,
  "export_midi": false,
  "show_statistics": true,
  "include_analysis": true
}
```

| Field              | Type | Default | Description                                           |
| ------------------ | ---- | ------- | ----------------------------------------------------- |
| `export_xml`       | bool | `false` | Write MusicXML file alongside JSON/text results       |
| `export_png`       | bool | `false` | Write PNG score image (requires external tooling)     |
| `export_midi`      | bool | `false` | Write MIDI file                                       |
| `show_statistics`  | bool | `true`  | Print solve time, rules checked, backjumps in console |
| `include_analysis` | bool | `true`  | Include interval analysis in text output              |

---

## 10. Heuristic Rules (Score-Based Ordering)

Heuristic rules guide the solver towards good solutions by scoring candidate values. The solver selects candidates with the highest combined heuristic score, enabling sophisticated musical preferences beyond hard constraints.

### 10.1 Heuristic Types: heuristic_preference vs heuristic_energy

The solver supports two heuristic rule type categories. Choose based on your expression style and intent.

**`heuristic_preference`** — for boolean (yes/no) objectives:

- Expression evaluates to boolean (true/false)
- Use with `mode: "heur_switch"`
- Typical weight: `2–5`
- Contribution: if true, add `weight` to score; if false, add `0`
- **Best for:** conditional objectives like "prefer stepwise motion," "avoid repeats," "prefer consonant intervals"
- **Example:** `"abs(voice[0].pitch[i+1] - voice[0].pitch[i]) <= 2"` (do consecutive notes differ by ≤2 semitones?)

**`heuristic_energy`** — for numeric scoring:

- Expression evaluates to a numeric score
- Use with `mode: "real_heuristic"`
- Typical weight: `1–3`
- Contribution: `expression * weight` (or just `expression` if weight omitted)
- **Best for:** continuous objectives like "minimize distance to center," "maximize voice spacing," "penalize large jumps"
- **Example:** `24 - abs(?current - 66)` (distance from MIDI 66; closer = higher score)

**Rule of thumb:**

1. If your objective is "should we do X?", use `heuristic_preference` + `heur_switch`
2. If your objective is "how good is candidate value Y?", use `heuristic_energy` + `real_heuristic`

**Why two types?**

- `heuristic_preference` is simpler: just ask yes/no and apply a weight
- `heuristic_energy` gives fine-grained control: score each candidate numerically
- They can coexist: hard constraints stay in `true_false` mode, preferences layer above

### 10.2 Heuristic Rule Fields

Use heuristic rules in `dynamic_rules` with these fields:

| Field         | Required | Description                                                                      |
| ------------- | -------- | -------------------------------------------------------------------------------- |
| `id`          | yes      | Rule identifier (used in logs)                                                   |
| `type`        | yes      | Rule category: `heuristic_preference` or `heuristic_energy`                      |
| `expression`  | yes      | Boolean (for heuristic_preference) or numeric (for heuristic_energy) expression  |
| `mode`        | yes      | `heur_switch` (for preference) or `real_heuristic` (for energy)                  |
| `weight`      | no       | Score multiplier (default `0`; treated as `1.0` for `real_heuristic` if omitted) |
| `priority`    | no       | Bucket priority (default `0`; higher priority dominates in tie-break)            |
| `direction`   | no       | `maximize` (default) or `minimize` (flips score sign internally)                 |
| `description` | no       | Free-text label shown in logs                                                    |

### 10.3 Scoring Semantics

- **`heur_switch`:**
  - Expression evaluated as boolean
  - If true: contributes `weight` to score bucket
  - If false: contributes `0`
- **`real_heuristic`:**
  - Expression evaluated as numeric
  - Contributes: `expression * weight` (or just `expression` if weight omitted)
- **`direction`:**
  - `maximize`: keep score sign as-is (larger is better)
  - `minimize`: invert sign internally (smaller is better)

### 10.4 Priority Buckets

Heuristics are grouped by integer `priority` and compared lexicographically. Higher priority buckets always dominate lower priority buckets.

**Comparison order:**

1. Highest priority bucket: if scores differ, pick best from that bucket
2. Next priority bucket: only compared if highest bucket scores are equal
3. Continue recursively to lowest priority buckets
4. Final tie-break: deterministic hash based on random seed

**Example with three heuristics:**

```json
{
  "priority": 10, // hard preference (e.g. stepwise motion)
  "priority": 5, // medium preference (e.g. consonance)
  "priority": 0 // soft preference (e.g. center register)
}
```

A candidate with priority-10 score=1 will beat priority-10 score=0, regardless of its priority-5 or priority-0 scores.

This allows "harder" musical constraints to dominate softer stylistic preferences.

### 10.5 Candidate Context Variables

Heuristic expressions can reference the candidate value being evaluated and surrounding context.

**Candidate variable:**

- `?current`: the candidate value (MIDI pitch or rhythm tick) currently being scored

**Context variables (read-only, from already-assigned neighbors):**

- `voice[V].pitch[i]`: pitch of voice V at position i
- `voice[V].rhythm[i]`: rhythm value of voice V at position i
- These refer to positions already assigned in the search tree

**Common patterns:**

```
// Score candidates near a target pitch
abs(?current - 66)

// Prefer candidates close to the first note
abs(?current - voice[0].pitch[0]) <= 5

// Encourage stepwise motion to previous note
abs(?current - voice[0].pitch[i-1]) <= 2

// Balance two voices in range
abs(?current - voice[1].pitch[i])
```

### 10.6 Basic Heuristic Example

Here is a complete single heuristic rule in `dynamic_rules`:

```json
{
  "id": "prefer_stepwise_motion",
  "type": "heuristic_preference",
  "expression": "abs(voice[0].pitch[i+1] - voice[0].pitch[i]) <= 2",
  "mode": "heur_switch",
  "weight": 4,
  "priority": 5,
  "direction": "maximize",
  "description": "Prefer stepwise motion (interval ≤ 2 semitones)"
}
```

This rule:

1. Checks if consecutive notes differ by ≤2 semitones
2. Awards `weight=4` points if true, `0` if false
3. Places the score in priority bucket 5
4. Maximizes (higher score is better)

Combined with other heuristics at priority 5, they share the same bucket. Lower priority heuristics only apply if all priority-5 heuristics are equal.

### 10.7 Wildcard Heuristic Rules

Wildcard heuristics let you author a single template rule and automatically expand it across positions (and optionally voices or voice pairs), avoiding manual duplication.

**Supported wildcard heuristic fields:**

| Field             | Required    | Description                                                              |
| ----------------- | ----------- | ------------------------------------------------------------------------ |
| `wildcard_type`   | recommended | `sliding_window`, `for_all_positions`, `for_all_voices`, `for_all_pairs` |
| `pattern_offsets` | no          | Relative offsets used in template, e.g. `[0]` or `[0,1]`                 |
| `step_size`       | no          | Expansion step size (default `1`)                                        |
| `cross_voices`    | no          | Hint for pairwise expansion                                              |

During load, wildcard rules are expanded to concrete per-position (and per-voice or per-pair) rules, then compiled through the normal heuristic pipeline.

### 10.8 Wildcard Types & Iteration Semantics

Understanding `wildcard_type` is key to getting expected behavior:

**`sliding_window`** — most common pattern:

- Iterates every voice and every valid starting position `i`
- `pattern_offsets` defines relative positions accessed in the window
- Window slides: `i = 0, 1, 2, ...` up to `solution_length - max_offset`
- **Best for:** consecutive interval rules, stepwise motion, any window-based pattern
- **Example:** `pattern_offsets: [0, 1]` means pairs (0,1), (1,2), (2,3), ...
- Expands to: N_voices × (solution_length - 1) concrete rules

**`for_all_positions`** — single-position iteration:

- Iterates all positions `i = 0 to solution_length - 1`
- Does NOT automatically create cross-voice combinations
- Template must already specify fixed voice(s) (e.g., `voice[0]`)
- **Best for:** per-position scoring when voice is fixed in the expression
- **Example:** score each note of voice 0 by distance to MIDI 65
- Expands to: solution_length concrete rules (one per position)

**`for_all_voices`** — voice iteration only:

- Iterates all voices at each position already fixed in template
- Requires placeholder `voice[v]` in expression for voice substitution
- Does NOT automatically iterate positions unless template uses `[i]` sliding
- **Best for:** voice-symmetric rules (same rule applies to all voices)
- **Example:** each voice should stay in its own register
- Expands to: N_voices concrete rules

**`for_all_pairs`** — pairwise cross-voice:

- Iterates all ordered or unique voice pairs: (0,1), (0,2), (1,2), ...
- Requires placeholders `voice[v1]` and `voice[v2]` (or aliases `voice[a]`, `voice[b]`)
- Position iteration depends on `pattern_offsets`
- **Best for:** voice-to-voice spacing, contrary motion, parallel interval rules
- **Example:** reward large pitch separation between every pair of voices
- Expands to: (N_voices choose 2) × (solution_length - max_offset) concrete rules

**Important clarifications:**

- `for_all_positions` alone does NOT create cross-voice rules
- `for_all_voices` alone does NOT create cross-voice rules
- Only `for_all_pairs` automatically expands across voice pairs
- If you need both voice iteration AND position iteration, use `sliding_window` with `voice[v]` placeholder

#### 10.7.1 Single-Voice Wildcard Example

**Config pattern:** Prefer stepwise motion everywhere (all voices, all consecutive pairs).

```json
{
  "id": "prefer_stepwise_everywhere",
  "type": "heuristic_preference",
  "wildcard_type": "sliding_window",
  "pattern_offsets": [0, 1],
  "step_size": 1,
  "expression": "abs(voice[v].pitch[i+1] - voice[v].pitch[i]) <= 2",
  "mode": "heur_switch",
  "weight": 4,
  "priority": 5,
  "direction": "maximize",
  "description": "Wildcard: prefer stepwise motion in all voices"
}
```

**Expansion:**

- For a 2-voice, 8-note config:
  - Voice 0: positions (0,1), (1,2), (2,3), (3,4), (4,5), (5,6), (6,7) → 7 rules
  - Voice 1: positions (0,1), (1,2), (2,3), (3,4), (4,5), (5,6), (6,7) → 7 rules
  - Total: 14 concrete heuristic rules from one template

#### 10.7.2 Pairwise Cross-Voice Wildcard Example

**Config pattern:** Reward larger pitch separation between every voice pair at each position (for open harmony).

```json
{
  "id": "prefer_voice_separation",
  "type": "heuristic_energy",
  "wildcard_type": "for_all_pairs",
  "pattern_offsets": [0],
  "step_size": 1,
  "cross_voices": true,
  "expression": {
    "operator": "-",
    "left": 24,
    "right": {
      "function": "abs",
      "args": [
        {
          "operator": "-",
          "left": "voice[v1].pitch[i]",
          "right": "voice[v2].pitch[i]"
        }
      ]
    }
  },
  "mode": "real_heuristic",
  "weight": 3,
  "priority": 8,
  "direction": "maximize",
  "description": "Wildcard pairwise: reward large pitch separation between voices"
}
```

**Expansion:**

- For a 3-voice, 8-note config:
  - Voice pairs: (0,1), (0,2), (1,2) → 3 pairs
  - Positions per pair: 8 positions
  - Total: 3 × 8 = 24 concrete heuristic rules from one template

#### 10.7.3 for_all_positions Example

**Config pattern:** Score every position of a fixed voice against a melodic center.

```json
{
  "id": "center_register_voice0",
  "type": "heuristic_energy",
  "wildcard_type": "for_all_positions",
  "pattern_offsets": [0],
  "expression": "24 - abs(voice[0].pitch[i] - 65)",
  "mode": "real_heuristic",
  "weight": 1,
  "priority": 2,
  "direction": "maximize",
  "description": "Prefer voice 0 notes near F4 (MIDI 65)"
}
```

**Expansion:**

- For an 8-note config:
  - Position rules: i = 0..7 → 8 rules
  - Voice is fixed to voice 0 in the expression
  - Total: 8 concrete heuristic rules from one template

#### 10.7.4 for_all_voices Example

**Config pattern:** Apply the same register preference to every voice.

```json
{
  "id": "mid_register_all_voices",
  "type": "heuristic_preference",
  "wildcard_type": "for_all_voices",
  "pattern_offsets": [0],
  "expression": "abs(voice[v].pitch[i] - 67) <= 7",
  "mode": "heur_switch",
  "weight": 3,
  "priority": 3,
  "direction": "maximize",
  "description": "Prefer all voices to stay near G4"
}
```

**Expansion:**

- For a 3-voice config:
  - Voice substitutions: v = 0, 1, 2 → 3 rules
  - Position behavior follows the template index form
  - Total: 3 concrete heuristic rules from one template

### 10.9 Wildcard Placeholder Reference

**Placeholders in wildcard expressions:**

| Placeholder              | Wildcard Type(s)                   | Meaning                                             |
| ------------------------ | ---------------------------------- | --------------------------------------------------- |
| `voice[v]`               | `sliding_window`, `for_all_voices` | voice iteration; substituted v=0, 1, 2, ...         |
| `voice[v1]`, `voice[v2]` | `for_all_pairs`                    | voice pair iteration; substituted (0,1), (0,2), ... |
| `voice[a]`, `voice[b]`   | `for_all_pairs`                    | Alias for pairwise; same as v1, v2 above            |
| `[i]`, `[i+N]`, `[i-N]`  | sliding patterns                   | position iteration; substituted i=0, 1, 2, ...      |
| `?current`               | all heuristics                     | candidate value being scored (not expanded)         |

### 10.10 Operational Notes

- Wildcard expansion is **load-time only**: one template → many concrete rules stored in memory
- Wildcard expansion applies **only** to heuristic modes (`heur_switch`, `real_heuristic`)
- Hard constraints should remain in `mode: true_false` (not wildcards)
- Expansion is **deterministic** for fixed config: same config = same rule count + order
- Approximate mode (`heuristic_top_k > 0`) can change selected solutions by design (feature, not bug)
- Rest positions are **automatically skipped** in sliding windows (no constraint posted if either window position is a rest)

### 10.11 Reference Configs

**Working examples in this repository:**

| Config                                                 | Features                              | Notes                                                    |
| ------------------------------------------------------ | ------------------------------------- | -------------------------------------------------------- |
| `configs/heuristic_smoke_8x1.json`                     | Basic tri-mode rules                  | Minimal example: hard + heur_switch + real_heuristic     |
| `configs/heuristic_priority_direction_8x1.json`        | Priority buckets, direction semantics | Shows how priority and maximize/minimize interact        |
| `configs/heuristic_priority_direction_topk_8x1.json`   | Approximate top-k selector            | Demonstrates `heuristic_top_k: 3` vs exact (0)           |
| `configs/heuristic_priority_direction_trace_8x1.json`  | Selector diagnostics                  | Enables `heuristic_trace: true` to see per-decision logs |
| `configs/heuristic_wildcard_stepwise_8x1.json`         | Single-voice wildcard                 | Stepwise preference expanded across all positions        |
| `configs/heuristic_wildcard_crossvoice_pairs_8x2.json` | Pairwise cross-voice wildcard         | Spacing preference between voice pairs                   |
| `configs/heuristic_example_musical_melody_8x1.json`    | Production example                    | Mixed hard + heuristics with pedagogical comments        |
| `configs/template_*.json`                              | Rule templates (in max-package)       | Starter templates for each rule type                     |

All examples pass validation; inspect to see syntax and real-world patterns.

---

## 8. Search Options & Solver Tuning

Search options control solve limits, branching strategy, randomization, and heuristic selector behavior.

```json
"search_options": {
  "timeout_ms": 30000,
  "max_solutions": 1,
  "branching": "first_fail",
  "random_seed": 42,
  "heuristic_top_k": 0,
  "heuristic_trace": false
}
```

### 8.1 Core Search Fields

| Field           | Type   | Default        | Description                                                                            |
| --------------- | ------ | -------------- | -------------------------------------------------------------------------------------- |
| `timeout_ms`    | int    | `30000`        | Maximum solve time in milliseconds. Search stops after this duration even if unsolved. |
| `max_solutions` | int    | `1`            | Number of solutions to return. `-1` means find all solutions.                          |
| `branching`     | string | `"first_fail"` | Variable selection strategy (see section 8.2).                                         |
| `random_seed`   | int    | deterministic  | Seed policy for randomized tie-breaking (see section 8.3).                             |

### 8.2 Variable Selection (`branching`)

| Value          | Strategy                                                                                   | When to use                                             |
| -------------- | ------------------------------------------------------------------------------------------ | ------------------------------------------------------- |
| `"first_fail"` | Pick variable with smallest remaining domain, break ties arbitrarily (recommended default) | Most constraint problems; fast pruning.                 |
| `"min_value"`  | Assign minimum domain value first at each branch point                                     | When you want ascending bias (e.g. ascending melody).   |
| `"max_value"`  | Assign maximum domain value first at each branch point                                     | When you want descending bias (e.g. descending melody). |

### 8.3 Randomization (`random_seed`)

| Value                             | Behavior                                                                                             | Use case                         |
| --------------------------------- | ---------------------------------------------------------------------------------------------------- | -------------------------------- |
| Omitted or `4294967295`           | Deterministic seed. Same run gives the same search path.                                             | Reproducible testing, debugging. |
| `0`                               | Fresh random seed from system entropy per solve. Different runs explore different paths.             | Exploring solution variety.      |
| Positive integer (for example 42) | Reproducible randomized search path. Same seed plus same config reproduces the same path and result. | Controlled randomization.        |

Practical examples:

- Reproducible debugging: `"random_seed": 42, "heuristic_trace": true`
- Explore variety: `"random_seed": 0, "max_solutions": 10`
- Performance testing: `"random_seed": 123, "timeout_ms": 5000`

### 8.4 Heuristic Selector Options

These fields apply when `dynamic_rules` include heuristic modes (`heur_switch` or `real_heuristic`).

| Field             | Type | Default | Description                                                                                                                                   |
| ----------------- | ---- | ------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| `heuristic_top_k` | int  | `0`     | `0` evaluates all candidate values (exact ranking). `>0` evaluates only first K candidates (faster, approximate, may change selected values). |
| `heuristic_trace` | bool | `false` | `true` prints per-decision selector diagnostics. `false` keeps solver output minimal.                                                         |

`heuristic_top_k` semantics:

- `0`: exact evaluation over full domain.
- `3`: approximate evaluation of first 3 candidates in domain order.
- Larger K improves accuracy at runtime cost.

Trace output example:

```
selector voice=0 pos=3 evaluated=5 chose=67 buckets={p5:35,p3:0,p1:4}
```

### 8.5 Preset Configurations

For reproducible debugging:

```json
"search_options": {
  "timeout_ms": 30000,
  "random_seed": 42,
  "heuristic_trace": true,
  "heuristic_top_k": 0
}
```

For fast exploration:

```json
"search_options": {
  "timeout_ms": 5000,
  "max_solutions": 10,
  "random_seed": 0,
  "heuristic_trace": false,
  "heuristic_top_k": 3
}
```

For performance analysis:

```json
"search_options": {
  "timeout_ms": 1000,
  "random_seed": 123,
  "heuristic_top_k": 0,
  "heuristic_trace": false
}
```

---

## 11. Full Config Example

This example creates a 2-voice, 8-note sequence where:

- Voice 0 uses only C major notes, all different, each consecutive pair 2 semitones apart.
- Voice 1 is always a perfect fifth (7 semitones) above voice 0.
- Both voices use quarter notes in 4/4 time.

```json
{
  "name": "C Major Stepwise with Parallel Fifths",
  "description": "Voice 0: C major, stepwise, all different. Voice 1: a fifth above.",
  "solution_length": 8,
  "num_voices": 2,
  "backtrack_method": "intelligent",
  "export_path": "tests/output",

  "engine_domains": {
    "engine_0": {
      "type": "rhythm",
      "voice": 0,
      "duration_values": ["1/4"],
      "description": "Voice 0: quarter notes only"
    },
    "engine_1": {
      "type": "pitch",
      "voice": 0,
      "midi_values": [
        60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 77, 79, 81, 83, 84
      ],
      "description": "Voice 0: C major, two octaves"
    },
    "engine_2": {
      "type": "rhythm",
      "voice": 1,
      "duration_values": ["1/4"],
      "description": "Voice 1: quarter notes only"
    },
    "engine_3": {
      "type": "pitch",
      "voice": 1,
      "midi_values": [
        67, 69, 71, 72, 74, 76, 77, 79, 81, 83, 84, 86, 88, 90, 91
      ],
      "description": "Voice 1: C major starting from G4, two octaves"
    }
  },

  "rules": [
    {
      "rule_type": "r-pitches-one-engine",
      "constraint_function": {
        "type": "builtin",
        "function": "all_different",
        "parameters": []
      },
      "indices": [0, 1, 2, 3, 4, 5, 6, 7],
      "target_engine": 1,
      "engine_type": "pitch",
      "enabled": true,
      "priority": 10,
      "description": "Voice 0: all notes different"
    },
    {
      "id": "stepwise_motion",
      "rule_type": "wildcard_constraint",
      "wildcard_type": "sliding_window",
      "pattern_offsets": [0, 1],
      "constraint": "abs(voice[0].pitch[i+1] - voice[0].pitch[i]) == 2",
      "enabled": true,
      "priority": 9,
      "description": "Voice 0: consecutive notes are a whole step apart"
    },
    {
      "id": "parallel_fifths",
      "rule_type": "wildcard_constraint",
      "wildcard_type": "sliding_window",
      "pattern_offsets": [0],
      "constraint": "voice[1].pitch[i] == voice[0].pitch[i] + 7",
      "enabled": true,
      "priority": 8,
      "description": "Voice 1: always a perfect fifth above voice 0"
    },
    {
      "rule_type": "r-rhythmic-uniformity",
      "constraint_function": {
        "type": "builtin",
        "function": "equal_values",
        "parameters": [4]
      },
      "indices": [0, 1, 2, 3, 4, 5, 6, 7],
      "target_engine": 0,
      "engine_type": "rhythm",
      "enabled": true,
      "priority": 7,
      "description": "Voice 0: all quarter notes"
    },
    {
      "rule_type": "r-rhythmic-uniformity",
      "constraint_function": {
        "type": "builtin",
        "function": "equal_values",
        "parameters": [4]
      },
      "indices": [0, 1, 2, 3, 4, 5, 6, 7],
      "target_engine": 2,
      "engine_type": "rhythm",
      "enabled": true,
      "priority": 7,
      "description": "Voice 1: all quarter notes"
    },
    {
      "rule_type": "r-metric-signature",
      "constraint_function": {
        "type": "builtin",
        "function": "equal_values",
        "parameters": [4]
      },
      "indices": [0],
      "target_engine": 4,
      "engine_type": "metric",
      "enabled": true,
      "priority": 6,
      "description": "4/4 time"
    }
  ],

  "search_options": {
    "timeout_ms": 30000,
    "max_solutions": 1,
    "branching": "first_fail"
  },

  "output_options": {
    "export_xml": true,
    "export_png": false,
    "export_midi": false,
    "show_statistics": true,
    "include_analysis": true
  }
}
```

---

## 12. Output Files

After a successful solve, the solver writes files to `export_path/`:

| File                        | Description                                                                  |
| --------------------------- | ---------------------------------------------------------------------------- |
| `<config_name>_result.json` | Machine-readable result: MIDI values, note names, rhythm, metric, statistics |
| `<config_name>_result.txt`  | Human-readable result: per-voice note lists with MIDI numbers and names      |
| `<config_name>_result.xml`  | MusicXML (if `export_xml: true`)                                             |

**JSON result structure:**

```json
{
  "config_file": "my_config.json",
  "problem_name": "...",
  "voices": [
    {
      "voice": 0,
      "pitch_solution": [60, 62, 64, ...],
      "pitch_names": ["C4", "D4", "E4", ...],
      "rhythm_solution": [4, 4, 4, ...],
      "rhythm_names": ["1/4", "1/4", "1/4", ...]
    },
    ...
  ],
  "metric_signature": [4],
  "solve_time_ms": 2,
  "rules_applied": 6,
  "rules_checked": 48
}
```

**Console output** shows each voice as:

```
Voice 0 Pitch: C4(60) → D4(62) → E4(64) → ...
Voice 0 Rhythm: 1/4 + 1/4 + 1/4 + ...
```

**Rest positions** appear as `R` in the pitch column and `R:1/4` (or the actual rest duration) in the rhythm column:

```
Voice 1 Pitch:  C4(60) → R → G4(67) → R
Voice 1 Rhythm: 1/4 + R:1/4 + 1/4 + R:1/4
```

In the JSON result file, rests are encoded as pitch value `−1` (sentinel) and a negative rhythm tick.

---

## 13. Common Errors

| Error message                                                                      | Cause                                                          | Fix                                                                                                                                               |
| ---------------------------------------------------------------------------------- | -------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| `Config is missing required 'engine_domains' section`                              | No `engine_domains` in the JSON                                | Add the `engine_domains` object                                                                                                                   |
| `engine_domains['engine_N']: pitch engine must have 'midi_values' array`           | A pitch engine entry is missing `midi_values`                  | Add the `midi_values` array to that entry                                                                                                         |
| `Voice N has no midi_values in engine_domains`                                     | No pitch engine found for voice N                              | Add an entry for `engine_(2N+1)` in `engine_domains`                                                                                              |
| `Voice N has no rhythm domain. Add an entry to 'engine_domains' with: ...`         | No rhythm engine found for voice N                             | Add `engine_(2N)` with `"type": "rhythm"` and `"duration_values": ["1/4"]` to `engine_domains` (the error message shows the exact snippet to add) |
| `engine_domains['engine_N']: rhythm engine must have 'duration_values' array`      | A rhythm engine entry is missing `duration_values`             | Add `"duration_values": ["1/4"]` (or any note-value fraction array) to that entry                                                                 |
| `engine_domains['engine_N']: numerator and denominator must be positive in '-1/4'` | Old build that did not support rest fractions                  | Rebuild from the latest source (`make bin/dynamic-solver`)                                                                                        |
| `index rule 'X': rest rhythm (negative) requires null pitch`                       | An index rule event has a negative rhythm and a non-null pitch | Set the pitch to `null` for rest events: `[pos, "-1/4", null]`                                                                                    |
| `index rule 'X': note rhythm (positive) requires a non-null pitch`                 | An index rule event has a positive rhythm and `null` pitch     | Provide a MIDI pitch value: `[pos, "1/4", 60]`                                                                                                    |
| `index rule 'X': voice N out of range`                                             | The `"voice"` field exceeds `num_voices - 1`                   | Check `"voice"` is 0-based and less than `"num_voices"`                                                                                           |
| `index rule 'X': position N out of range`                                          | An event position exceeds `solution_length - 1`                | Check positions are 0-based and less than `"solution_length"`                                                                                     |
| `Invalid voice access expression: voice[0].pitch[i])`                              | Tokenizer bug with nested parentheses (now fixed)              | Make sure you are running the latest build                                                                                                        |
| `No solution found`                                                                | The combined constraints are infeasible                        | Check that the domains are wide enough to satisfy all rules simultaneously; try relaxing or removing one rule to isolate the conflict             |
| `Failed to compile rule`                                                           | Syntax error in a constraint expression                        | Check operator syntax (use `==` not `=`), and that voice/index references are correct                                                             |

---
