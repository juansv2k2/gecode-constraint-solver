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
   - [8.6 Neural Pitch Scorer](#86-neural-pitch-scorer)
9. [Export Options](#9-export-options)
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
  "search_options": { ... },
  "export_json": true,
  "export_txt": true,
  "export_xml": true,
  "file_name": "output/my_piece",
  "rules": [ ... ],
  "dynamic_rules": [ ... ]
}
```

**Required fields:**

- `solution_length`: Number of notes per voice (integer)
- `num_voices`: Number of voices (integer)
- `voices`: Array of voice configurations (or `global_domain` as fallback)

**Optional fields:**

- `name`, `description`: Metadata strings
- `score_length`: Total musical score length (e.g., `"8q"` = 8 quarter notes)
- `meter`: Metric signature configuration
- `search_options`: Search behavior configuration
- `export_json`, `export_txt`, `export_xml`: Export flags (default `false` for all)
- `file_name`: Output path and base name — see [§9 Export Options](#9-export-options). Directory component optional (e.g. `"output/my_piece"`).
- `rules`: Built-in constraints
- `dynamic_rules`: Expression-based constraints and heuristics

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
- Tuplet support: `"1/10"` (quintuplet eighth in 4/4), `"1/20"` (quintuplet sixteenth), etc.  
  The `rhythm_base` tick grid is automatically extended to cover all tuplet denominators declared in `meter.tuplets`, so these values work without manual configuration.

**Pitch values:**

- MIDI note numbers (0–127)
- Each voice can have any subset of the MIDI range
- Used to constrain variable domains during search

### 3.1 `global_domain` — shared fallback domain

When multiple voices share the same pitch or rhythm domain, repeating the full definition for each voice is tedious. `global_domain` is an optional top-level key that defines a shared domain used as a fallback for any voice that does not declare its own.

```json
{
  "num_voices": 8,
  "global_domain": {
    "rhythm": { "duration_values": ["1/4", "1/8", "-1/4", "-1/8"] },
    "pitch": { "midi_values": [60, 62, 64, 65, 67, 69, 71, 72] }
  }
}
```

All 8 voices inherit both rhythm and pitch from `global_domain`. The `voices` array can be omitted entirely when no per-voice overrides are needed.

**Per-voice overrides** — the `voices` array can be shorter than `num_voices`. Any voice index not represented falls back to `global_domain`. A voice entry can override just one component; the other still comes from `global_domain`:

```json
"voices": [
  { "pitch": { "midi_values": [48, 50, 52, 53, 55] } }
]
```

Voice 0 uses its own pitch but inherits rhythm. Voices 1–7 use both components from `global_domain`.

**Non-positional targeting with `"voice": N`** — by default, entries are assigned positionally (first entry = voice 0, second = voice 1, …). Use the `"voice"` key to target a specific voice index without padding:

```json
"voices": [
  { "voice": 0, "pitch": { "midi_values": [48, 50, 52, 53, 55] } },
  { "voice": 7, "pitch": { "midi_values": [84, 86, 88, 89, 91] } }
]
```

Voices 1–6 are not listed and fall back to `global_domain`. The entries can appear in any order in the array.

**Resolution priority** (highest to lowest):

| Level | Source                                       |
| ----- | -------------------------------------------- |
| 1     | Voice entry with matching `"voice": N` key   |
| 2     | Voice entry at position N (no `"voice"` key) |
| 3     | `global_domain`                              |

Both `rhythm` and `pitch` are resolved independently — a voice entry that specifies only one component falls back to `global_domain` for the other.

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
- `tuplets`: Subdivision factors that **require tuplet notation** (e.g. `[3, 6, 12]` for triplets and sextuplets in a binary meter). The solver automatically extends the internal tick grid (`rhythm_base`) to accommodate each value, so tuplet-aligned duration fractions in `voices.rhythm.duration_values` resolve to exact integers.
- `beat_divisions`: All subdivision factors that exist for each beat, including both native and tuplet ones.

> **Avoid overly-fine tuplet combinations.** When multiple `tuplets` values are specified and their combined grid covers every tick position (e.g. `[5, 10]` in 4/4 with `rhythm_base = 40`), the `r-metric-hierarchy` DURATIONS_GRID constraint becomes trivially true and the solver will warn:
> `⚠️ r-metric-hierarchy (voice N): metric grid step is 1 tick — constraint is trivially true`.
> In that case, remove the redundant larger tuplet value (e.g. keep only `[5]`).

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

### 5.3a Cross-Voice Rhythm Rules (`r-rhythm-rhythm`)

`r-rhythm-rhythm` posts position-aligned constraints between exactly two voices' rhythm variables. It is **automatically targeted to rhythm engines** — do not add `target_component`.

`target_voices` must contain exactly two voice indices.

**Isorhythm — identical values including note/rest sign:**

```json
{
  "id": "homorhythm_v0_v1",
  "rule_type": "r-rhythm-rhythm",
  "constraint": "isorhythm",
  "target_voices": [0, 1]
}
```

**Absolute isorhythm — same durations, note/rest status independent:**

```json
{
  "id": "abs_iso_v0_v1",
  "rule_type": "r-rhythm-rhythm",
  "constraint": "abs_isorhythm",
  "target_voices": [0, 1]
}
```

**Augmentation — v0 durations are `ratio`× longer than v1, note/rest status must match:**

```json
{
  "id": "augment_2x",
  "rule_type": "r-rhythm-rhythm",
  "constraint": "augmentation",
  "parameters": [2],
  "target_voices": [0, 1]
}
```

The ratio can be a float. `0.5` means v0 durations are half as long as v1:

```json
{
  "id": "diminish_half",
  "rule_type": "r-rhythm-rhythm",
  "constraint": "augmentation",
  "parameters": [0.5],
  "target_voices": [0, 1]
}
```

**Diminution — v0 durations are `ratio`× shorter than v1, note/rest status must match:**

```json
{
  "id": "diminish_2x",
  "rule_type": "r-rhythm-rhythm",
  "constraint": "diminution",
  "parameters": [2],
  "target_voices": [0, 1]
}
```

**No simultaneous rests — at least one voice is active per position:**

```json
{
  "id": "no_sim_rests",
  "rule_type": "r-rhythm-rhythm",
  "constraint": "no_simultaneous_rests",
  "target_voices": [0, 1]
}
```

**Rest complement — exactly one voice rests per position:**

```json
{
  "id": "rest_complement_v0_v1",
  "rule_type": "r-rhythm-rhythm",
  "constraint": "rest_complement",
  "target_voices": [0, 1]
}
```

Use `"indices"` to restrict the constraint to specific sequence positions:

```json
{
  "id": "augment_first_4",
  "rule_type": "r-rhythm-rhythm",
  "constraint": "augmentation",
  "parameters": [2],
  "indices": [0, 1, 2, 3],
  "target_voices": [0, 1]
}
```

> **Note on sign-matching:** `augmentation` and `diminution` compare absolute durations **and** require both positions to have the same note/rest status. A note in v0 can only be matched with a note in v1, and a rest with a rest. Use `abs_isorhythm` if you want to constrain duration ratios without tying note/rest status.

> **Float ratios:** `parameters` accepts integers or floats. Common musical ratios are recognised exactly: `0.5` (1:2), `0.333…` (1:3), `1.5` (3:2), `0.75` (3:4), etc. The solver converts them internally to integer fractions (denominator ≤ 64).

| `constraint`            | Description                                       | `parameters`                        |
| ----------------------- | ------------------------------------------------- | ----------------------------------- |
| `isorhythm`             | Identical rhythm values (note/rest sign included) | —                                   |
| `abs_isorhythm`         | Same durations; note/rest status independent      | —                                   |
| `augmentation`          | v0 is ratio× longer than v1; signs must match     | `[ratio]` (int or float, default 2) |
| `diminution`            | v0 is ratio× shorter than v1; signs must match    | `[ratio]` (int or float, default 2) |
| `no_simultaneous_rests` | At least one voice is active per position         | —                                   |
| `rest_complement`       | Exactly one voice rests per position              | —                                   |

### 5.3b Cross-Voice Pitch Rules (`r-pitch-pitch`)

`r-pitch-pitch` posts position-aligned constraints between exactly two voices' pitch variables. It is **automatically targeted to pitch engines** — do not add `target_component`.

`target_voices` must contain exactly two voice indices.

#### Phase 1 — single-position harmonic constraints

These modes apply independently at each selected position.

**No unison — voices must not share the same MIDI pitch:**

```json
{
  "id": "no_unison_v0_v1",
  "rule_type": "r-pitch-pitch",
  "constraint": "no_unison",
  "target_voices": [0, 1]
}
```

**Same pitch — force identical MIDI pitch at every position:**

```json
{
  "id": "same_pitch_v0_v1",
  "rule_type": "r-pitch-pitch",
  "constraint": "same_pitch",
  "target_voices": [0, 1]
}
```

**Voice above / voice below — enforce strict vertical ordering:**

```json
{
  "id": "soprano_above_alto",
  "rule_type": "r-pitch-pitch",
  "constraint": "voice_above",
  "target_voices": [0, 1]
}
```

`"voice_below"` constrains v0 to be strictly below v1.

**Exact interval — signed semitone difference `p0 − p1 = n`:**

```json
{
  "id": "perfect_fifth_v0_v1",
  "rule_type": "r-pitch-pitch",
  "constraint": "exact_interval",
  "parameters": [7],
  "target_voices": [0, 1]
}
```

**Minimum interval — `|p0 − p1| ≥ min` semitones:**

```json
{
  "id": "min_fourth_v0_v1",
  "rule_type": "r-pitch-pitch",
  "constraint": "min_interval",
  "parameters": [5],
  "target_voices": [0, 1]
}
```

**Maximum interval — `|p0 − p1| ≤ max` semitones:**

```json
{
  "id": "max_tenth_v0_v1",
  "rule_type": "r-pitch-pitch",
  "constraint": "max_interval",
  "parameters": [16],
  "target_voices": [0, 1]
}
```

**Interval range — `min ≤ |p0 − p1| ≤ max`:**

```json
{
  "id": "third_to_octave",
  "rule_type": "r-pitch-pitch",
  "constraint": "interval_range",
  "parameters": [3, 12],
  "target_voices": [0, 1]
}
```

**Interval class — `(p0 − p1) mod 12` must be in the given set:**

```json
{
  "id": "consonances_only",
  "rule_type": "r-pitch-pitch",
  "constraint": "interval_class",
  "parameters": [0, 3, 4, 7, 8, 9],
  "target_voices": [0, 1],
  "description": "Allow only unisons, thirds, fourths/fifths, and sixths"
}
```

#### Phase 2 — consecutive-pair motion constraints

These modes look at each position pair `(i, i+1)` and enforce motion rules. The last position is skipped (no `i+1`).

**No consecutive perfect fifths — avoids two successive 7-semitone harmonic intervals:**

```json
{
  "id": "no_par_fifths_v0_v1",
  "rule_type": "r-pitch-pitch",
  "constraint": "no_consecutive_fifths",
  "target_voices": [0, 1]
}
```

**No consecutive octaves:**

```json
{
  "id": "no_par_octaves_v0_v1",
  "rule_type": "r-pitch-pitch",
  "constraint": "no_consecutive_octaves",
  "target_voices": [0, 1]
}
```

**No parallel motion — forbids both voices moving in the same direction. Oblique motion (one voice static) is allowed:**

```json
{
  "id": "no_parallel_v0_v1",
  "rule_type": "r-pitch-pitch",
  "constraint": "no_parallel_motion",
  "target_voices": [0, 1]
}
```

**Contrary motion — both voices must move in strictly opposite directions at every step:**

```json
{
  "id": "contrary_v0_v1",
  "rule_type": "r-pitch-pitch",
  "constraint": "contrary_motion",
  "target_voices": [0, 1]
}
```

#### Restricting positions with `indices`, `stride`, and `offset`

By default a rule applies at all positions. Use `"indices"` for an explicit list, or `"stride"` + `"offset"` for a regular pattern:

```json
{
  "id": "consonant_on_beats",
  "rule_type": "r-pitch-pitch",
  "constraint": "interval_class",
  "parameters": [0, 3, 4, 7, 8, 9],
  "stride": 2,
  "offset": 0,
  "target_voices": [0, 1],
  "description": "Consonance constraint at positions 0, 2, 4, 6, ..."
}
```

`"stride"` takes precedence over `"indices"`. If neither is set, all positions are used.

| `constraint`             | Type        | Description                                           | `parameters`    |
| ------------------------ | ----------- | ----------------------------------------------------- | --------------- |
| `no_unison`              | position    | `p0 ≠ p1`                                             | —               |
| `same_pitch`             | position    | `p0 = p1`                                             | —               |
| `voice_above`            | position    | `p0 > p1`                                             | —               |
| `voice_below`            | position    | `p0 < p1`                                             | —               |
| `exact_interval`         | position    | `p0 − p1 = n` (signed semitones)                      | `[n]`           |
| `min_interval`           | position    | `\|p0−p1\| ≥ min`                                     | `[min]`         |
| `max_interval`           | position    | `\|p0−p1\| ≤ max`                                     | `[max]`         |
| `interval_range`         | position    | `min ≤ \|p0−p1\| ≤ max`                               | `[min, max]`    |
| `interval_class`         | position    | `(p0−p1) mod 12 ∈ set`                                | `[c1, c2, ...]` |
| `no_consecutive_fifths`  | consec pair | NOT two successive interval-class-7 harmonies         | —               |
| `no_consecutive_octaves` | consec pair | NOT two successive interval-class-0 harmonies         | —               |
| `no_parallel_motion`     | consec pair | Voices may not both move in the same direction        | —               |
| `contrary_motion`        | consec pair | Both voices must move in strictly opposite directions | —               |

#### Soft preference mode (`"heuristic": true`)

Both `r-pitch-pitch` and `r-rhythm-rhythm` support an optional `"heuristic": true` flag that converts the rule from a **hard constraint** into a **soft preference**. The solver uses the rule to sort candidates (satisfying values are tried first) but never rejects a candidate that violates it.

```json
{
  "id": "prefer_no_unison",
  "rule_type": "r-pitch-pitch",
  "constraint": "no_unison",
  "target_voices": [0, 1],
  "heuristic": true,
  "description": "Prefer non-unison intervals, but allow them if necessary"
}
```

```json
{
  "id": "prefer_isorhythm",
  "rule_type": "r-rhythm-rhythm",
  "constraint": "isorhythm",
  "target_voices": [0, 1],
  "heuristic": true,
  "description": "Prefer homorhythm, but allow independent rhythms"
}
```

This is useful when a musical preference should guide search without pruning any solutions. Combine with `"value_order": "heuristic"` in `search_options` for best results.

### 5.4 Metric Hierarchy Rules (`r-metric-hierarchy`)

`r-metric-hierarchy` constrains rhythm values relative to the beat grid defined in `meter`. It is **automatically targeted to rhythm engines** — do not add `target_component` or `engine_type`.

**Default (durations) mode — constrain rhythm values to the metric grid:**

```json
{
  "rule_type": "r-metric-hierarchy",
  "constraint": "durations",
  "target_voices": [0]
}
```

This is the default mode (also activated when `constraint` is omitted). Both note durations and rest durations are filtered to multiples of the metric grid step derived from `meter.tuplets` and `meter.beat_divisions`. Use this to restrict a voice to only use quintuplet-aligned values, for example.

> **Grid step warning.** If the combination of `meter.tuplets` produces a 1-tick grid step, the constraint has no effect (all durations pass) and a warning is printed. See the `meter` section above for how to avoid this.

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

**Voice hierarchy (cantus firmus anchors + optional fioritura):**

```json
{
  "rule_type": "r-metric-hierarchy",
  "constraint": "hierarchical_voices",
  "parameters": ["0<-1"],
  "target_voices": [0, 1]
}
```

The `"0<-1"` parameter means voice 0 is the fine voice and voice 1 is the coarse voice.

Default behavior (when `indices` are omitted): only **beat-aligned onsets** of the coarse voice must also appear in the fine voice. This is the cantus-firmus / fioritura mode: the fine voice may use smaller values between those anchors and is **not** forced to copy the full rhythm of the coarse voice.

Example: beat-anchor fioritura

```json
{
  "rule_type": "r-metric-hierarchy",
  "constraint": "hierarchical_voices",
  "parameters": ["0<-1"],
  "target_voices": [0, 1]
}
```

In this example, voice 1 provides the cantus-firmus anchor points and voice 0 may ornament between them.

Strict behavior: add one of these tokens in `parameters` to require every coarse onset to appear in the fine voice:

- `"all_onsets"`
- `"strict_all_onsets"`
- `"all-onsets"`
- `"strict-all-onsets"`

Example: exact onset inheritance (strict)

```json
{
  "rule_type": "r-metric-hierarchy",
  "constraint": "hierarchical_voices",
  "parameters": ["0<-1", "strict_all_onsets"],
  "target_voices": [0, 1]
}
```

Partial behavior: add explicit `indices` to require onset inheritance only for selected coarse-note positions.

Example: partial cantus anchors

```json
{
  "rule_type": "r-metric-hierarchy",
  "constraint": "hierarchical_voices",
  "indices": [0, 1, 2, 3],
  "parameters": ["0<-1"],
  "target_voices": [0, 1],
  "enabled": 1,
  "description": "Only the first four coarse onsets must be inherited"
}
```

`enabled` accepts `true`/`false` and also truthy values like `1`, `yes`, and `on` in the CLI config parser.

**No syncopation (notes must not cross strong beat boundaries):**

```json
{
  "rule_type": "r-metric-hierarchy",
  "constraint": "no-syncopation",
  "target_voices": [0, 1]
}
```

Constrains every note so that it ends at or before the next beat boundary: `onset % beat_ticks + |duration| ≤ beat_ticks`. Rests are unconstrained. Subdivisions (eighth, sixteenth, triplet-eighth, etc.) are freely allowed as long as they fit within their beat cell. Only a note that **spans across** a beat boundary — i.e., a syncopation — is rejected.

The optional `beats` parameter selects which beat positions act as strong boundaries, which widens the allowed cell:

```json
{
  "rule_type": "r-metric-hierarchy",
  "constraint": "no-syncopation",
  "parameters": ["beats:1,3"],
  "target_voices": [0, 1],
  "description": "No cross-beat-3 notes; stp = 2 beats (half-note grid)"
}
```

| `parameters`      | Strong-beat period (`stp`) in 4/4            | Effect                                        |
| ----------------- | -------------------------------------------- | --------------------------------------------- |
| _(omitted)_       | `1 × beat_ticks` (e.g. 24 ticks)             | Every beat is a boundary                      |
| `"beats:1,3"`     | `min_spacing × beat_ticks` (e.g. 48 t)       | Half-note grid; notes must fit within 2 beats |
| `"beats:1"`       | `numerator × beat_ticks` (e.g. 72 t for 3/4) | Bar-boundary mode: no note crosses a barline  |
| `"beats:1,2,3,4"` | `1 × beat_ticks` (every beat)                | Same as omitting `beats`                      |

`beats` values are 1-indexed and sorted automatically. The period `stp` is derived from the **minimum spacing** between the listed strong beats, multiplied by `beat_ticks`.

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

**Bar pattern (bar-oriented metric timeline):**

```json
{
  "rule_type": "r-time-signature",
  "bar_pattern_type": "fixed",
  "bar_pattern": ["4/4", "4/4", "3/4"],
  "allow_cross_barline": false
}
```

`bar_pattern_type` values:

- `fixed`: use the pattern as written
- `repeating`: repeat pattern (optionally with `bar_pattern_repetitions`; if omitted/0, fills score)
- `random`: random draw from `bar_pattern` using `bar_pattern_count`
- `weighted`: weighted random draw using `bar_pattern_distribution` + `bar_pattern_count`

`allow_cross_barline` controls duration carry:

- `false` (default): strict bar fill
- `true`: durations may span barlines

When durations span barlines, MusicXML export writes explicit ties.

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

| Parameter                  | Type   | Description                                                                                                               |
| -------------------------- | ------ | ------------------------------------------------------------------------------------------------------------------------- |
| `rule_type`                | string | `r-one-voice`, `r-uniformity`, `r-metric-hierarchy`, `r-time-signature`, `r-rhythm-rhythm`, `r-palindrome-voice2`, etc.   |
| `constraint`               | string | Built-in function: `all_different`, `equal_values`, `palindrome_of_engine`, `min_grid`, `isorhythm`, `augmentation`, etc. |
| `parameters`               | array  | Constraint parameters (rhythm values, time signatures, engine indices)                                                    |
| `target_voices`            | array  | Multiple voices: `[0, 1]`                                                                                                 |
| `target_component`         | string | `"pitch"`, `"rhythm"`, or `"metric"`                                                                                      |
| `indices`                  | array  | Positions in sequence — omit to apply to all                                                                              |
| `timepoints`               | array  | Quarter-note positions for metric rules: `["0q", "4q"]`                                                                   |
| `bar_pattern_type`         | string | Metric pattern mode: `fixed`, `repeating`, `random`, `weighted`                                                           |
| `bar_pattern`              | array  | Time-signature list for bar-oriented metric rules                                                                         |
| `bar_pattern_count`        | int    | Number of generated bars for `random`/`weighted`                                                                          |
| `bar_pattern_repetitions`  | int    | Explicit repetition count for `repeating`                                                                                 |
| `bar_pattern_distribution` | object | Weights for `weighted` mode (signature -> probability)                                                                    |
| `allow_cross_barline`      | bool   | For `r-time-signature` bar patterns: allow duration carry over barlines                                                   |
| `enabled`                  | bool   | Enable/disable rule (default: `true`)                                                                                     |
| `priority`                 | int    | Rule priority (higher = tried first)                                                                                      |
| `description`              | string | Human-readable label                                                                                                      |

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

- **`min`** (default): Try the value with the smallest **absolute** tick count first. For rhythm variables, this means the shortest duration, with notes (positive) preferred over rests (negative) of the same length. For pitch variables, it means the lowest MIDI value. Fully deterministic.
- **`random`**: Try values in random order. Requires `random_seed > 0` for reproducibility.
- **`heuristic`**: Score candidates using `heuristic_energy` rules from `dynamic_rules`, try higher-scoring candidates first. When candidates are tied, selection depends on `random_seed` (see below).
- **`neural`**: Score pitch candidates using the MLP neural scorer — see [§8.6 Neural Pitch Scorer](#86-neural-pitch-scorer). Uses Gumbel-max sampling so different `random_seed` values produce different melodies while respecting all hard constraints. Has no effect on rhythm variables (those fall back to `min`).

> **Note on `min` and rests.** Before this change the `min` strategy always selected the most negative rhythm tick (= longest rest), causing outputs of all rests when rests were present in the domain. It now picks the shortest absolute duration. If you need the old strict-min behavior, use `value_order: "random"` with a fixed seed, or remove rest values from `duration_values`.

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

### 8.6 Neural Pitch Scorer

When `value_order` is `"neural"`, the solver scores pitch candidates using a trained
unified melodic MLP. Two models are included:

| Model file                  | Training data                  | Val accuracy                     | Chord conditioning       |
| --------------------------- | ------------------------------ | -------------------------------- | ------------------------ |
| `harmonic_weights.json`     | 67k labeled Bach chorale notes | **50% top-1** (128 MIDI classes) | ✅ `harmonic_domain` key |
| `folk_melodic_weights.json` | 455k folk + Bach notes         | —                                | ❌ melody only           |

**Architecture:** 60-dim input — 8 pitch context values + 8 rhythm context values +
8-voice one-hot + 36-class chord one-hot (12 roots × 3 qualities) → 256 hidden
neurons (ReLU) → softmax over 128 MIDI pitch classes. Trained with **Apple MLX**
on Apple Silicon.

Scoring uses **Gumbel-max sampling**: each candidate receives
`score = logit_i / T + gumbel(seed, voice, pos, cand)`, where the Gumbel term is
deterministically derived from `random_seed`. Taking the `argmax` over all
candidates is then statistically equivalent to sampling from the learned
distribution with temperature `T`. This means:

- Different `random_seed` values produce different, reproducible melodies.
- `random_seed: 0` gives a fresh random result each solve (non-reproducible).
- All hard constraints remain fully enforced — the neural scorer only changes the
  **order** in which values are tried, not which values are valid.
- When heuristic soft-rules are also active they act as lower-priority tiebreakers;
  the neural score is always the primary bucket.

```json
"search_options": {
  "value_order": "neural",
  "neural_weights_file": "datasets/weights/harmonic_weights.json",
  "neural_temperature": 0.3,
  "neural_shadow_mode": false,
  "random_seed": 0
}
```

**Neural parameters:**

| Parameter             | Type    | Default                                    | Description                                                                                                                                                       |
| --------------------- | ------- | ------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `neural_weights_file` | string  | `"datasets/weights/harmonic_weights.json"` | Path to the exported MLP weights. Relative paths are resolved from the Max patch folder when running inside Max; from the working directory when running the CLI. |
| `neural_temperature`  | float   | `1.0`                                      | Logit temperature `T`. `0.3` = chord-following / idiomatic, `1.0` = balanced, `> 1.0` = more adventurous.                                                         |
| `neural_shadow_mode`  | boolean | `false`                                    | When `true`, logs candidate probabilities and Gumbel scores to stderr without affecting search output. Useful for debugging.                                      |

**Temperature guide:**

| `neural_temperature` | Character                                                            |
| -------------------: | -------------------------------------------------------------------- |
|                  0.3 | Chord-following — highest-logit candidate wins almost every position |
|                  0.5 | Strongly idiomatic — favours the most statistically common intervals |
|                  1.0 | Balanced — reproduces the training distribution                      |
|                  2.0 | More varied — uncommon intervals appear more often                   |

#### 8.6.1 Harmonic Domain Conditioning

When using `harmonic_weights.json`, add a `harmonic_domain` array to bias the
scorer toward chord tones at each note position. Each entry specifies the chord
root and quality that applies **from that beat onward** (forward-fill until the
next entry or the end of the sequence):

```json
"harmonic_domain": [
  { "beat": 0,  "chord": "C", "quality": "major" },
  { "beat": 4,  "chord": "F", "quality": "major" },
  { "beat": 8,  "chord": "G", "quality": "dom7"  },
  { "beat": 12, "chord": "C", "quality": "major" }
]
```

The `beat` value corresponds directly to the note **position index** in the
solution (0-based). A `beat: 0` entry with no subsequent entry covers every
position in a single-chord piece. Supported qualities: `major`, `minor`, `dom7`.
Roots: `C`, `C#`/`Db`, `D`, `D#`/`Eb`, `E`, `F`, `F#`/`Gb`, `G`, `G#`/`Ab`,
`A`, `A#`/`Bb`, `B`.

**Isolation test result** (`configs/chromatic_chord_test.json`): chromatic
MIDI domain (C4–B5, 24 pitches), `C–F–G7–C` progression, no-adjacent-repeat
constraint, `neural_temperature: 0.3` —

```
Output: G E C G | A C F A | G F D G | E C G C   →  100% chord tones
```

The no-adjacent-repeat constraint forces the voice to cycle through chord tones,
naturally producing an arpeggio pattern guided entirely by the neural scorer.

**Retraining the harmonic model:**

```bash
# Rebuild dataset (fixes chord labels), retrain, sync weights everywhere
python3 scripts/build_chorale_dataset.py       # rebuilds datasets/unified_training.tsv
python3 scripts/train_unified_melodic.py --mode harmonic  # ~5000 epochs on M1
bash scripts/max_package_smoke.sh              # syncs datasets/weights/ → examples/weights/
```

Retrain folk/melody model (no chord conditioning):

```bash
python3 scripts/train_unified_melodic.py --mode folk
bash scripts/max_package_smoke.sh
```

**Verifying neural influence:**

```bash
python3 tests/test_neural_influence.py          # 30 seeds, 4 assertions
python3 tests/test_neural_influence.py --seeds 100
```

See [tests/NEURAL_TEST_RESULTS.md](tests/NEURAL_TEST_RESULTS.md) for recorded
baseline results.

## 9. Export Options

Export keys are **top-level** fields in the config — not nested under an `output_options` object.

```json
{
  "export_json": true,
  "export_txt": true,
  "export_xml": true,
  "file_name": "output/my_piece",
  "rules": [ ... ]
}
```

| Key           | Type    | Default | Description                                                      |
| ------------- | ------- | ------- | ---------------------------------------------------------------- |
| `file_name`   | string  |         | Output path + base name. Directory part is optional (see below). |
| `export_json` | boolean | `false` | Save raw solution data as JSON                                   |
| `export_txt`  | boolean | `false` | Save human-readable text summary                                 |
| `export_xml`  | boolean | `false` | Save as MusicXML (viewable in notation software)                 |

### `file_name` path resolution (Max external)

`file_name` is the single key that controls both the output folder and the base filename. The file extension is always appended automatically per format (`.xml`, `.json`, `.txt`).

| `file_name` value           | Where files are written                   |
| --------------------------- | ----------------------------------------- |
| `"my_piece"` (no directory) | Same folder as the .maxpat file           |
| `"output/my_piece"`         | `<patch-folder>/output/my_piece.xml` etc. |
| `"/absolute/path/my_piece"` | That exact directory, unchanged           |

If the patch has not been saved yet, the external falls back to Max's current default path. If `file_name` is omitted entirely, the solver uses `"max_solver"` as the base name in the patch folder.

> **Legacy `export_path` key:** still accepted for backward compatibility. When `file_name` contains a directory component, it takes precedence over `export_path`. New configs should use only `file_name`.

> **Legacy `output_options` block:** Older configs used a nested `output_options: { export_path: ..., export_json: ... }` object. The wrapper still accepts this but top-level export keys take precedence. All new configs should use top-level keys.

## 10. Complete Parameter Reference

### Top-Level

| Field             | Type    | Required | Description                                                                  |
| ----------------- | ------- | -------- | ---------------------------------------------------------------------------- |
| `solution_length` | int     | ✅       | Notes per voice                                                              |
| `num_voices`      | int     | ✅       | Number of voices                                                             |
| `voices`          | array   |          | Voice domain definitions (required unless `global_domain` set)               |
| `global_domain`   | object  |          | Shared fallback pitch/rhythm domain for all voices                           |
| `name`            | string  |          | Config label (used as output filename base)                                  |
| `description`     | string  |          | Human-readable description                                                   |
| `score_length`    | string  |          | e.g. `"8q"` = 8 quarter notes                                                |
| `meter`           | object  |          | Metric engine configuration                                                  |
| `search_options`  | object  |          | Search strategy                                                              |
| `export_json`     | boolean |          | Save JSON result                                                             |
| `export_txt`      | boolean |          | Save text summary                                                            |
| `export_xml`      | boolean |          | Save MusicXML                                                                |
| `file_name`       | string  |          | Output path + base name — directory part optional (e.g. `"output/my_piece"`) |
| `rules`           | array   |          | Built-in constraints                                                         |
| `dynamic_rules`   | array   |          | Expression-based constraints and heuristics                                  |

### `search_options` Object

| Field                  | Type    | Default                                    | Description                                                                                                  |
| ---------------------- | ------- | ------------------------------------------ | ------------------------------------------------------------------------------------------------------------ |
| `branching`            | string  | `"first_fail"`                             | Variable ordering: `"first_fail"` or `"input_order"`                                                         |
| `value_order`          | string  | `"min"`                                    | Value ordering: `"min"`, `"random"`, `"heuristic"`, or `"neural"`                                            |
| `restart_policy`       | string  | `"none"`                                   | `"none"` or `"luby"`                                                                                         |
| `timeout_ms`           | int     | `30000`                                    | Search time limit in milliseconds                                                                            |
| `max_solutions`        | int     | `1`                                        | Solutions to collect (0 = unlimited)                                                                         |
| `random_seed`          | int     | `0`                                        | `0` = fresh random each solve, `N > 0` = reproducible                                                        |
| `heuristic_top_k`      | int     | `0`                                        | Only score top K candidates with heuristic (0 = all). `"heuristic"` mode only.                               |
| `heuristic_trace`      | boolean | `false`                                    | Log heuristic scores to stdout during search                                                                 |
| `enable_metric_engine` | int     | `1`                                        | `0` disables the metric engine (no meter constraints)                                                        |
| `neural_weights_file`  | string  | `"datasets/weights/harmonic_weights.json"` | Path to MLP weights JSON. `"neural"` mode only. Relative paths resolved from patch folder in Max.            |
| `neural_temperature`   | float   | `1.0`                                      | Logit temperature. `1.0` = trained distribution, `< 1.0` = sharper, `> 1.0` = flatter. `"neural"` mode only. |
| `neural_shadow_mode`   | boolean | `false`                                    | Log scores without affecting search (debug). `"neural"` mode only.                                           |

### Rule Object

| Field                      | Type    | Description                                                                                                                                                                                                                                                                      |
| -------------------------- | ------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `rule_type`                | string  | `r-one-voice`, `r-uniformity`, `r-metric-hierarchy`, `r-time-signature`, `r-rhythm-rhythm`, `r-pitch-pitch`, `r-twelve-tone-voice1`, `r-palindrome-voice2`, `r-cross-voice-no-unisons`, `r-perfect-fifth-intervals`, `r-cross-voice-retrograde-inversion`, `wildcard_constraint` |
| `constraint`               | string  | Shorthand built-in name: `all_different`, `equal_values`, `palindrome_of_engine`, `isorhythm`, `augmentation`, `no_unison`, `voice_above`, `interval_class`, `no_consecutive_fifths`, `contrary_motion`, etc.                                                                    |
| `parameters`               | array   | Constraint-specific params (rhythm values, time signatures, semitone counts, ratios)                                                                                                                                                                                             |
| `target_voices`            | array   | Voice indices: `[0]` or `[0, 1]`                                                                                                                                                                                                                                                 |
| `target_component`         | string  | `"pitch"`, `"rhythm"`, or `"metric"` — omit for `r-rhythm-rhythm` / `r-pitch-pitch` (auto-targeted)                                                                                                                                                                              |
| `indices`                  | array   | Explicit position list — omit to apply to all positions                                                                                                                                                                                                                          |
| `stride`                   | int     | Apply every N positions starting from `offset` (takes precedence over `indices`)                                                                                                                                                                                                 |
| `offset`                   | int     | Starting position for stride-based application (default `0`)                                                                                                                                                                                                                     |
| `heuristic`                | boolean | **`r-pitch-pitch` / `r-rhythm-rhythm` only.** When `true`, converts the rule to a soft preference: values satisfying the rule are tried first, but violations are not rejected. Default `false` (hard constraint).                                                               |
| `timepoints`               | array   | Quarter-note positions for metric rules: `["0q", "4q"]`                                                                                                                                                                                                                          |
| `bar_pattern_type`         | string  | Metric pattern mode: `fixed`, `repeating`, `random`, `weighted`                                                                                                                                                                                                                  |
| `bar_pattern`              | array   | Time-signature list, e.g. `["4/4", "3/4"]`                                                                                                                                                                                                                                       |
| `bar_pattern_count`        | int     | Number of bars generated in `random`/`weighted` modes                                                                                                                                                                                                                            |
| `bar_pattern_repetitions`  | int     | Repetition count in `repeating` mode                                                                                                                                                                                                                                             |
| `bar_pattern_distribution` | object  | Weights for `weighted` mode (signature → probability)                                                                                                                                                                                                                            |
| `allow_cross_barline`      | boolean | For `r-time-signature` bar patterns: allow duration carry over barlines                                                                                                                                                                                                          |
| `enabled`                  | boolean | Enable/disable rule (default `true`). Accepts `true`/`false` and `1`/`0`/`"yes"`/`"on"`.                                                                                                                                                                                         |
| `priority`                 | int     | Higher = tried first                                                                                                                                                                                                                                                             |
| `id`                       | string  | Optional label                                                                                                                                                                                                                                                                   |
| `description`              | string  | Optional documentation                                                                                                                                                                                                                                                           |

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

### 11.1 The Search Loop

```
1. Select a variable          → branching strategy (first_fail / input_order)
2. Rank candidates            → value_order (min / random / heuristic / neural)
3. Propose best candidate     → highest-scored value is tried first
4. Check ALL constraints      → rules + dynamic_rules (hard constraints always applied)
   ✓ Passes → go deeper, select next variable
   ✗ Fails  → reject candidate, try next in ranked list
5. If all candidates fail → backtrack up the search tree
```

At step 4, hard constraints are checked regardless of how the candidate was ranked. A value that scores highly under any heuristic or neural scorer is still rejected if it violates a rule. The scorer only determines _order of proposal_, never validity.

### 11.2 Value-Order Modes

| `value_order` | Behaviour                                                                                                                                       |
| ------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| `"min"`       | Lowest MIDI pitch / shortest duration first. Fully deterministic.                                                                               |
| `"random"`    | Random permutation. Controlled by `random_seed`.                                                                                                |
| `"heuristic"` | Scored by `heuristic_energy` dynamic rules. Tied candidates break randomly when `random_seed: 0`.                                               |
| `"neural"`    | MLP neural scorer (requires `neural_weights_file`). Neural score is the **primary bucket**; symbolic soft rules are lower-priority tiebreakers. |

All four modes operate identically with respect to hard constraints: every proposed value goes through the full constraint check before it is accepted.

### 11.3 Symbolic Heuristics (`value_order: "heuristic"`)

Symbolic heuristics are `dynamic_rules` of type `"heuristic_energy"`. Each rule computes a numeric score for a candidate value; the solver tries candidates in descending score order.

Multiple heuristic rules are combined through **priority buckets**:

- Rules with higher `priority` values form a primary ranking.
- Rules with lower `priority` values break ties within the primary ranking.
- When `random_seed: 0`, unbroken ties are resolved randomly → different valid solutions per run.

Built-in soft preferences via `"heuristic": true` on `r-pitch-pitch` / `r-rhythm-rhythm` rules are inserted as tiebreaker buckets below explicit `heuristic_energy` rules.

```json
{
  "search_options": { "value_order": "heuristic", "random_seed": 0 },
  "dynamic_rules": [
    {
      "id": "prefer_stepwise",
      "type": "heuristic_energy",
      "wildcard_type": "for_all_positions",
      "expression": "max(0, 3 - abs(voice[0].pitch[i+1] - voice[0].pitch[i]))",
      "weight": 10,
      "priority": 1,
      "direction": "maximize"
    }
  ],
  "rules": [
    {
      "id": "no_unison",
      "rule_type": "r-pitch-pitch",
      "constraint": "no_unison",
      "target_voices": [0, 1],
      "heuristic": true
    }
  ]
}
```

Here `prefer_stepwise` (priority 1) ranks candidates first; `no_unison` with `heuristic: true` breaks ties. The hard constraint `r-twelve-tone-voice1` (if present) remains completely unchanged.

### 11.4 Neural Scorer (`value_order: "neural"`)

The neural scorer replaces the top-priority ranking bucket with an MLP forward pass. It does **not** change the constraint checking step.

**Required configuration:**

```json
{
  "search_options": {
    "value_order": "neural",
    "neural_weights_file": "datasets/weights/harmonic_weights.json",
    "neural_temperature": 0.3
  },
  "harmonic_domain": [
    { "beat": 0, "chord": "C", "quality": "major" },
    { "beat": 4, "chord": "F", "quality": "major" },
    { "beat": 8, "chord": "G", "quality": "dom7" },
    { "beat": 12, "chord": "C", "quality": "major" }
  ]
}
```

**How it works internally:**

1. At solver start the MLP weights file is loaded once into memory.
2. The `harmonic_domain` array is forward-filled into a per-position `harmonic_state` vector (one chord-class entry per note position).
3. At each variable assignment, every candidate pitch is scored by calling the MLP with a 60-dimensional input: `[pitch_context (8), interval_context (8), mod-octave_class (12), chord_one_hot (24), voice_id (4), position_norm (4)]`.
4. Gumbel-max sampling (`logit / T + gumbel_noise`) converts logits to a stochastic ranking. Lower `neural_temperature` = more deterministic chord-tone selection.
5. Symbolic soft rules (`"heuristic": true`) are placed in a **lower-priority tiebreaker bucket** below the neural score. They further sort candidates that the MLP scores identically.
6. Hard constraints (`rules`, `dynamic_rules` with `type: "basic_constraint"`) run unchanged after the neural ranking.

**Priority stack (highest → lowest):**

```
[Neural MLP score + Gumbel noise]   ← primary ranking
[Symbolic heuristic_energy rules]   ← tiebreaker
[heuristic:true soft built-in rules]← secondary tiebreaker
[min / random fallback]             ← final tiebreaker
```

**What the neural scorer cannot do:**

- It cannot relax or bypass any hard constraint.
- It cannot learn backtracking or domain pruning.
- It does not guarantee any specific pitch will appear — only that chord tones are tried first; if they all violate a hard constraint the solver backtracks normally.

### 11.5 Key Invariants (applies to all modes)

- Palindrome, 12-tone row, retrograde inversion, metric hierarchy — all enforced identically regardless of value_order.
- Heuristic and neural scores are recomputed at every variable: there is no persistent score state that can "override" a constraint failure.
- Disabling a hard constraint (setting `"enabled": false`) has nothing to do with heuristics — it removes the rule from the constraint check entirely.
- `heuristic_top_k` limits how many candidates are scored by expensive heuristics. Set it to `0` (all) or a value ≥ the domain size to ensure every candidate is considered. A too-small value silently ignores high-MIDI or high-duration candidates.

### 11.6 Stochastic Diversity

When `value_order: "heuristic"` or `"neural"` and `random_seed: 0`:

- Many candidates may score identically (especially when cross-voice context isn't yet assigned).
- The solver randomly selects from the tied group each run.
- This gives **different valid solutions on each run**, all satisfying all hard constraints.

```
Heuristic: prefer perfect fifths between voices
Constraint: voice 1 is palindrome of voice 0

Run 1 → [D E F# G A B C# D] + palindrome + fifths ✓
Run 2 → [A G F# E D C# B A] + palindrome + fifths ✓
Run 3 → [C D E F G A B C]   + palindrome + fifths ✓

All different sequences. All palindromes. All prefer fifths.
```

Use a fixed `random_seed > 0` for fully reproducible results.

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
- [configs/schema.json](configs/schema.json)
