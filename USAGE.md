# Usage Guide

This guide describes the current, supported configuration contract for the solver and wrapper.

## 1. Run Commands

CLI:

```bash
bin/dynamic-solver configs/metric_domain_example.json
```

Wrapper harness (same path used by Max normalization):

```bash
./bin/test-max-wrapper configs/metric_domain_example.json
```

## 2. Current Top-Level Shape

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

Required for modern configs:

- `solution_length`
- `num_voices`
- `voices`

## 3. Voices and Meter

### 3.1 Voices

`voices` is a contiguous voice list (array preferred):

```json
"voices": [
  {
    "rhythm": { "duration_values": ["1/8", "1/4"] },
    "pitch": { "midi_values": [60, 62, 64, 65, 67, 69, 71, 72] }
  },
  {
    "rhythm": { "duration_values": ["1/4"] },
    "pitch": { "midi_values": [55, 57, 59, 60, 62, 64, 65, 67] }
  }
]
```

Rules:

- Voice indices are contiguous `0..N-1`.
- Each voice should define both pitch and rhythm domains.
- Rhythm rests are supported with negative durations, for example `"-1/4"`.

### 3.2 Meter

```json
"meter": {
  "time_signatures": ["4/4", "3/4", "6/8"],
  "tuplets": [3],
  "beat_divisions": [2, 3]
}
```

`r-metric-signature` targeting is implicit; do not set voice/engine targets for metric signature rules.

## 4. Rules

### 4.1 Built-in Rules (`rules`)

Modern built-in rule targets are voice-based:

- `target_voice` or `target_voices`
- `target_component`: `pitch` or `rhythm`

Pitch example:

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

Rhythm example:

```json
{
  "rule_type": "r-rhythmic-uniformity",
  "constraint_function": {
    "type": "builtin",
    "function": "equal_values",
    "parameters": ["1/4"]
  },
  "indices": [0, 1, 2, 3, 4, 5, 6, 7],
  "target_voice": 1,
  "target_component": "rhythm"
}
```

Metric example:

```json
{
  "rule_type": "r-metric-signature",
  "constraint_function": {
    "type": "builtin",
    "function": "equal_values",
    "parameters": ["4/4", "3/4", "6/8"]
  },
  "timepoints": ["0q", "4q", "7q"]
}
```

### 4.2 Dynamic Rules (`dynamic_rules`)

Dynamic rules support hard constraints and heuristics.

Common categories:

- `basic_constraint` + `mode: "true_false"`
- `heuristic_preference` + `mode: "heur_switch"`
- `heuristic_energy` + `mode: "real_heuristic"`

Example:

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

## 5. Search Options

Search behavior is configured via `search_options`.

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

Important:

- `backtrack_method` is deprecated.
- Legacy `branching: "sequential"` is normalized to `input_order` in wrapper compatibility mode.

## 6. Output Options

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

## 7. Legacy Compatibility Notes

The wrapper currently normalizes several legacy patterns to the current contract, including:

- old domain containers (`engine_domains`, `domains.voice_domains`, `note_domain`)
- old wrappers (`configuration` blocks)
- legacy rule targeting (`voice`, wildcard scopes, and inferred voice references)

For new configs, use the voice-first contract directly.

## 8. Common Errors

- `missing target_voice/target_voices`
  - Add `target_voice` or `target_voices` plus `target_component` for built-in non-metric rules.
- `Missing required object: engine_domains`
  - Indicates legacy shape that was not normalized for that input path; migrate to `voices`.
- metric timepoint errors
  - Ensure `timepoints` are strictly increasing and within `score_length`.

## 9. References

- [README.md](README.md)
- [max-usage.md](max-usage.md)
- [configs/cluster_config_schema.json](configs/cluster_config_schema.json)
