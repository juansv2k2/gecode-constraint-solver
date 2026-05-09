# Max Usage Guide

This file mirrors the current Max usage contract for `gecode.solver`.

## Object and Messages

Object:

- `gecode.solver`

Messages:

- `config <json>`
- `config_file <absolute_path>`
- `config_dict <dict_name>`
- `config_dict_debug <dict_name>`
- `solve`
- `cancel`
- `status`
- `get_last`

## Outlets

- Left: `voice_pitch`, `voice_rhythm`
- Middle: `json` payloads
- Right: status + message

## Recommended Config Contract

Use voice-first configs:

- `solution_length`
- `num_voices`
- `voices`
- optional `meter`
- `rules` / `dynamic_rules`
- `search_options`
- `output_options`

Deprecated:

- `backtrack_method`

## Search Strategy

```json
"search_options": {
  "engine": "dfs",
  "branching": "first_fail",
  "value_order": "heuristic",
  "restart_policy": "none",
  "timeout_ms": 30000,
  "max_solutions": 1,
  "random_seed": 42
}
```

Allowed values:

- `branching`: `first_fail`, `input_order`
- `value_order`: `min`, `random`, `heuristic`
- `restart_policy`: `none`, `luby`

## File Workflow

```text
config_file "/Users/.../gecode-constraint-solver/configs/metric_domain_example.json"
solve
```

## Dict Workflow

1. Build/edit dict.
2. `config_dict <dict_name>`.
3. `solve`.

Use `config_dict_debug` to inspect normalized JSON.

## Troubleshooting

- Configure failure: check path/dict and required top-level fields.
- UNSAT: reconfigure and solve again; state is reusable.
- Long run: poll with `status`; stop with `cancel`.

## Cross-Reference

- [usage-in-max.md](usage-in-max.md)
- [USAGE.md](USAGE.md)
- [README.md](README.md)
