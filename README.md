# Gecode Musical Constraint Solver

Polyphonic musical constraint solving in C++17 on top of Gecode. The engine is inspired on the logic of the musical constraint solver Cluster-Engine, developed by Örjan Sandred, in particular the use of polyphonic dynamic constraints in the form of index and wildcard rules, metric segmentation and hierarchy. The system currently has a Max/MSP integration as an Max external package.

## Front Ends

- `bin/dynamic-solver`: CLI runner.
- `bin/test-max-wrapper`: wrapper harness (same normalization path used by Max).
- `bin/gecode.solver.mxo`: Max external.

All three use the same core solver.

## Quick Start

### Build

```bash
make bin/dynamic-solver
make bin/test-max-wrapper
make max-external
```

### Run

```bash
bin/dynamic-solver configs/metric_domain_example.json
./bin/test-max-wrapper configs/metric_domain_example.json
```

## Current Config Shape

```json
{
  "name": "Example",
  "solution_length": 8,
  "num_voices": 2,
  "voices": [
    {
      "rhythm": { "duration_values": ["1/8", "1/4"] },
      "pitch": { "midi_values": [60, 62, 64, 65, 67, 69, 71, 72] }
    },
    {
      "rhythm": { "duration_values": ["1/4"] },
      "pitch": { "midi_values": [55, 57, 59, 60, 62, 64, 65, 67] }
    }
  ],
  "meter": {
    "time_signatures": ["4/4", "3/4"],
    "beat_divisions": [2, 3]
  },
  "search_options": {
    "engine": "dfs",
    "branching": "first_fail",
    "value_order": "heuristic",
    "restart_policy": "none",
    "timeout_ms": 30000,
    "max_solutions": 1,
    "random_seed": 42
  },
  "export_json": true,
  "export_txt": true,
  "export_xml": true,
  "file_name": "output/my_piece",
  "rules": [
    {
      "rule_type": "r-one-voice",
      "constraint": "all_different",
      "indices": [0, 1, 2, 3, 4, 5, 6, 7],
      "target_voices": [0],
      "target_component": "pitch"
    },
    {
      "rule_type": "r-uniformity",
      "constraint": "equal_values",
      "parameters": ["1/4"],
      "indices": [0, 1, 2, 3, 4, 5, 6, 7],
      "target_voices": [1],
      "target_component": "rhythm"
    },
    {
      "rule_type": "r-time-signature",
      "constraint": "equal_values",
      "parameters": ["4/4", "3/4"],
      "timepoints": ["0q", "4q"]
    }
  ]
}
```

> **`file_name` path resolution:** `"output/my_piece"` saves files as `<patch-folder>/output/my_piece.xml` (etc.). A plain name like `"my_piece"` puts files directly in the patch folder. An absolute path is used as-is. Extension is always appended automatically. `export_path` is still accepted as a legacy fallback.

## Wildcard Membership Example

You can now write compact interval-class constraints with membership sets:

```json
{
  "rule_type": "wildcard_constraint",
  "wildcard_type": "for_all_positions",
  "constraint": "abs(voice[1].pitch[i] - voice[0].pitch[i]) in [0, 5, 7, 12]",
  "indices": [0, 4, 8, 12],
  "target_voices": [0, 1],
  "target_component": "pitch"
}
```

Notes:

- `indices` restricts the wildcard to only those positions.
- Use `not_in` for exclusion sets.
- `in` / `not_in` require an integer array literal on the right-hand side.

## Search Strategy

Use `search_options`:

- `branching`: `first_fail` or `input_order`
- `value_order`: `min`, `random`, `heuristic`, or `neural`  
  `min` for rhythm variables uses absolute-value minimum (shortest duration, notes before same-length rests).  
  `neural` activates the MLP pitch scorer — see [Neural Pitch Scorer](#neural-pitch-scorer) below.
- `restart_policy`: `none` or `luby`

Notes:

- Legacy `branching: "sequential"` is normalized to `input_order` by the wrapper compatibility path.

## Metric Rules

`r-time-signature` supports two equivalent ways to define the metric timeline:

- `timepoints` + `parameters` in `q` units (quarter-note units)
- `bar_pattern` for bar-oriented writing (`fixed`, `repeating`, `random`, `weighted`)

Bar-pattern example:

```json
{
  "rule_type": "r-time-signature",
  "bar_pattern_type": "fixed",
  "bar_pattern": ["4/4", "4/4", "3/4"],
  "allow_cross_barline": true
}
```

Notes:

- `allow_cross_barline: false` enforces strict per-bar fill.
- `allow_cross_barline: true` allows carried duration across barlines.
- MusicXML export now writes ties (`<tie>` and `<notations><tied>`) when notes are split at barlines.

## Neural Pitch Scorer

Set `"value_order": "neural"` to guide pitch selection using an MLP trained on
Weimar folk melodies (4334 notes, MIDI 52–81, mean A4). Architecture: 8-note
context window → 256 hidden neurons (ReLU) → softmax over 28 pitch classes.
Trained with **Apple MLX** (~15 s on M1) to **27% top-1 accuracy** — 7.5× above
random baseline, with no train/val gap.

The scorer uses **Gumbel-max sampling**: each candidate receives
`logit_i / T + gumbel(seed, voice, pos, cand)`, so `argmax` over candidates is
equivalent to sampling from the learned distribution. Different `random_seed`
values produce different but reproducible melodies; `T < 1.0` sharpens
(more folk-like), `T > 1.0` flattens (more adventurous).

```json
"search_options": {
  "value_order": "neural",
  "neural_weights_file": "datasets/pitch_mlp_weights.json",
  "neural_temperature": 1.0,
  "neural_shadow_mode": false,
  "random_seed": 0
}
```

| Parameter             | Default                           | Effect                                                                                |
| --------------------- | --------------------------------- | ------------------------------------------------------------------------------------- |
| `neural_weights_file` | `datasets/pitch_mlp_weights.json` | Path to weights JSON (relative paths resolved from patch folder in Max)               |
| `neural_temperature`  | `1.0`                             | Logit temperature. `1.0` = trained distribution, `< 1.0` = sharper, `> 1.0` = flatter |
| `neural_shadow_mode`  | `false`                           | Log scores to stderr without affecting search (debugging)                             |
| `random_seed`         | `0`                               | `0` = fresh random per solve, `N > 0` = reproducible                                  |

Retrain the model (requires Apple MLX — `pip3 install mlx`):

```bash
# defaults: hidden=256, epochs=5000, context=8  (~15 s on M1)
python3 scripts/train_pitch_mlp.py
cp datasets/pitch_mlp_weights.json max-package/gecode-solver/examples/weights/
bash scripts/max_package_smoke.sh
```

Verify neural influence:

```bash
python3 tests/test_neural_influence.py       # runs 30 seeds, checks 4 properties
```

See [tests/NEURAL_TEST_RESULTS.md](tests/NEURAL_TEST_RESULTS.md) for baseline results.

## Max Integration

Use `config_file` or `config_dict` with the `gecode.solver` object. For details, see:

- [USAGE.md](USAGE.md)
- [max-usage.md](max-usage.md)

## Documentation

- [Usage Guide](USAGE.md)
- [Max Usage](max-usage.md)
- [JSON Schema](configs/schema.json)
- [Harmonic Consonance Example](configs/harmonic_consonance_4voice.json)
- [Twelve-Tone Usage](docs/TWELVE_TONE_USAGE.md)
- [XML Export Guide](docs/XML_EXPORT_GUIDE.md)
- [Neural Pitch Scorer Test Results](tests/NEURAL_TEST_RESULTS.md)

## What Is Current

- User-facing configs are voice-first: `voices` + optional `meter`.
- Built-in rule targeting is voice-based: `target_voices` with `target_component`.
- Export settings are **top-level keys** (`export_json`, `export_txt`, `export_xml`, `file_name`) — not nested inside an `output_options` block. `file_name` carries both the subdirectory and base name: `"output/my_piece"` writes to `<patch-folder>/output/my_piece.xml`. A plain name like `"my_piece"` writes to the patch folder directly.
- Search strategy is configured via `search_options`.
- Dynamic expression parsing supports logical operators (`&&`, `||`, `!`, `not`), membership (`in`, `not_in`), and integer array literals (`[0, 5, 7, 12]`).
- `wildcard_constraint` rules now honor `indices` directly in wildcard expansion.
- Wrapper compatibility layer still accepts several legacy config shapes and normalizes them.
- `rhythm_base` is automatically extended to cover all tuplet denominators declared in `meter.tuplets`, so tuplet-aligned duration values (e.g. `"1/10"`, `"1/20"` for quintuplets in 4/4) work without manual configuration.
- `r-metric-hierarchy` DURATIONS_GRID mode now applies the grid filter to rest durations as well as note durations; previously rests always bypassed the filter. A warning is emitted when the combination of tuplet values produces a 1-tick (trivially fine) grid step.
- Rhythm variable branching uses absolute-value minimum by default: the solver picks the shortest duration first (notes preferred over same-length rests) rather than the most negative value (longest rest). `value_order: "random"` is unchanged.
- `global_domain` is an optional top-level key that defines shared pitch and/or rhythm domains used as a fallback for any voice that does not declare its own. The `voices` array can be shorter than `num_voices` (or omitted entirely) when `global_domain` fully covers the remaining voices. Per-voice entries can override only `pitch`, only `rhythm`, or both; the other component falls back to `global_domain`. Use the optional `"voice": N` key inside a voice entry to target a specific voice index non-positionally.
- `r-rhythm-rhythm` is a cross-voice rhythm constraint. It posts position-aligned Gecode constraints between two voices' rhythm variables. Modes: `isorhythm` (identical values), `abs_isorhythm` (same durations, independent note/rest status), `augmentation` (v0 is ratio× longer than v1; note/rest status must match), `diminution` (v0 is ratio× shorter than v1; note/rest status must match), `no_simultaneous_rests` (at least one voice active per position), `rest_complement` (exactly one voice rests per position). Requires `target_voices: [A, B]`; automatically targets rhythm engines. The `parameters` ratio accepts integers and floats (`0.5`, `1.5`, `0.75`, etc.), converted internally to exact integer fractions. Supports `"heuristic": true` to convert the rule into a soft preference.
- `r-pitch-pitch` is a cross-voice pitch constraint. It posts position-aligned Gecode constraints between two voices' pitch variables. **Phase 1 modes** (per position): `no_unison`, `same_pitch`, `voice_above`, `voice_below`, `exact_interval` (signed semitones, requires `parameters: [n]`), `min_interval`, `max_interval`, `interval_range` (requires `parameters: [min, max]`), `interval_class` (requires `parameters: [c1, c2, ...]`, applies `(p0−p1) mod 12`). **Phase 2 modes** (consecutive pairs): `no_consecutive_fifths`, `no_consecutive_octaves`, `no_parallel_motion`, `contrary_motion`. Requires `target_voices: [A, B]`; automatically targets pitch engines. Use `"stride": N` and `"offset": K` to apply the rule every N positions starting from K, or `"indices": [...]` for an explicit position list. Supports `"heuristic": true` to convert the rule into a soft preference.
- Legacy configs that used `output_options` blocks or `dynamic_rules`-only format are preserved in `configs/legacy/`.

## Development

```bash
make test-all
make clean
```
