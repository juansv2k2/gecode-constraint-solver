# Usage Guide

This is the full, current guide for authoring configs for the solver.

The short version:

- Use voice-first configs (`voices`, optional `meter`).
- Use simple expression strings for dynamic rules.
- Use simple built-in shorthand: `"constraint": "all_different"`.
- Keep `search_options` for search behavior.

The advanced JSON-AST expression form is still supported, but no longer the recommended default authoring style.

## 1. Quick Start

CLI:

```bash
bin/dynamic-solver configs/metric_domain_example.json
```

Wrapper path (same normalization used by Max):

```bash
./bin/test-max-wrapper configs/metric_domain_example.json
```

## 2. Top-Level Structure

```json
{
  "name": "My Config",
  "description": "...",
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

Required in modern configs:

- `solution_length`
- `num_voices`
- `voices`

## 3. Voices

```json
"voices": [
  {
    "rhythm": { "duration_values": ["1/8", "1/4"] },
    "pitch": { "midi_values": [60, 62, 64, 65, 67, 69, 71, 72] }
  },
  {
    "rhythm": { "duration_values": ["1/4", "-1/4"] },
    "pitch": { "midi_values": [55, 57, 59, 60, 62, 64, 65, 67] }
  }
]
```

Notes:

- Voices are contiguous `0..N-1`.
- Each voice should define pitch and rhythm.
- Negative rhythm values are rests (`"-1/4"`).

## 4. Meter

```json
"meter": {
  "time_signatures": ["4/4", "3/4", "6/8"],
  "tuplets": [3],
  "beat_divisions": [2, 3]
}
```

Metric signature rules are implicitly metric-targeted.

## 5. Built-in Rules (`rules`)

You can now write built-ins in two equivalent ways.

### 5.1 Preferred shorthand

```json
{
  "rule_type": "r-pitches-one-engine",
  "constraint": "all_different",
  "indices": [0, 1, 2, 3, 4, 5, 6, 7],
  "target_voice": 0,
  "target_component": "pitch"
}
```

### 5.2 Verbose form (still supported)

```json
{
  "rule_type": "r-pitches-one-engine",
  "constraint_function": {
    "type": "builtin",
    "function": "all_different",
    "parameters": []
  },
  "indices": [0, 1, 2, 3, 4, 5, 6, 7],
  "target_voice": 0,
  "target_component": "pitch"
}
```

### 5.3 More built-in examples

Rhythm uniformity:

```json
{
  "rule_type": "r-rhythmic-uniformity",
  "constraint": "equal_values",
  "parameters": ["1/4"],
  "indices": [0, 1, 2, 3, 4, 5, 6, 7],
  "target_voice": 1,
  "target_component": "rhythm"
}
```

Cross-voice no unisons:

```json
{
  "rule_type": "r-cross-voice-no-unisons",
  "constraint": "no_unisons_between_engines",
  "target_voices": [0, 1],
  "target_component": "pitch",
  "indices": [0, 1, 2, 3, 4, 5, 6, 7]
}
```

Metric signature with timepoints:

```json
{
  "rule_type": "r-metric-signature",
  "constraint": "equal_values",
  "parameters": ["4/4", "3/4", "6/8"],
  "timepoints": ["0q", "4q", "7q"]
}
```

## 6. Dynamic Rules (`dynamic_rules`)

Dynamic rules are expression-based and support hard constraints plus heuristics.

### 6.1 Recommended style: expression as real string

Hard constraint:

```json
{
  "id": "step_by_two",
  "type": "basic_constraint",
  "mode": "true_false",
  "expression": "abs(voice[0].pitch[i+1] - voice[0].pitch[i]) <= 2"
}
```

Heuristic preference:

```json
{
  "id": "prefer_center",
  "type": "heuristic_energy",
  "mode": "real_heuristic",
  "weight": 2,
  "expression": "24 - abs(?current - 66)"
}
```

### 6.2 Alias: `constraint` instead of `expression`

This is also accepted:

```json
{
  "id": "simple_plus3",
  "type": "basic_constraint",
  "mode": "true_false",
  "constraint": "voice[v].pitch[i+1] == voice[v].pitch[i] + 3"
}
```

The wrapper normalizes `constraint` to `expression` for dynamic rules.

### 6.3 Advanced AST object form (optional)

Still supported, but not required:

```json
{
  "id": "stepwise_motion",
  "type": "heuristic_preference",
  "mode": "heur_switch",
  "weight": 5,
  "expression": {
    "operator": "<=",
    "left": {
      "function": "abs",
      "args": [
        {
          "operator": "-",
          "left": "?current",
          "right": "voice[0].pitch[i-1]"
        }
      ]
    },
    "right": 2
  }
}
```

## 7. Expression Examples (String Form)

Single voice melodic limits:

```text
abs(voice[0].pitch[i+1] - voice[0].pitch[i]) <= 2
```

Parallel motion between voices:

```text
voice[1].pitch[i] == voice[0].pitch[i] + 7
```

Wildcard voice placeholder:

```text
voice[v].pitch[i+1] != voice[v].pitch[i]
```

Cross-voice pair placeholder style:

```text
abs(voice[v1].pitch[i] - voice[v2].pitch[i]) >= 3
```

Rhythm coupling:

```text
voice[1].rhythm[i] == voice[0].rhythm[i]
```

## 8. Search Options

```json
"search_options": {
  "engine": "dfs",
  "branching": "first_fail",
  "value_order": "heuristic",
  "restart_policy": "none",
  "timeout_ms": 30000,
  "max_solutions": 1,
  "random_seed": 42,
  "heuristic_top_k": 0,
  "heuristic_trace": false
}
```

Allowed values:

- `branching`: `first_fail`, `input_order`
- `value_order`: `min`, `random`, `heuristic`
- `restart_policy`: `none`, `luby`

## 9. Output Options

```json
"output_options": {
  "export_path": "output",
  "export_filename": "my_run",
  "export_json": true,
  "export_txt": true,
  "export_xml": false,
  "export_png": false,
  "export_midi": false,
  "show_statistics": true,
  "include_analysis": true
}
```

If export flags are enabled, provide `output_options.export_path`.

## 10. Legacy Compatibility

The wrapper still normalizes legacy patterns:

- `engine_domains`, `domains.voice_domains`, `note_domain`
- `configuration` blocks
- legacy rule targeting (`voice`, wildcard scopes, inferred voice refs)
- built-in shorthand `constraint` and dynamic rule `constraint` alias

Use the modern voice-first contract for all new configs.

## 11. Common Errors

- `missing target_voice/target_voices`
  - Add `target_voice` or `target_voices` and `target_component` for non-metric built-ins.
- `Missing required object: engine_domains`
  - Legacy shape not normalized in that path; migrate to `voices`.
- metric timepoint errors
  - Ensure `timepoints` are strictly increasing and within `score_length`.

## 12. References

- [README.md](README.md)
- [max-usage.md](max-usage.md)
- [configs/cluster_config_schema.json](configs/cluster_config_schema.json)
