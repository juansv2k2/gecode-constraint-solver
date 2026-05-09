# Using the Solver in Max

This guide covers the current `gecode.solver` Max external workflow and the updated config contract.

## 1. Object and Messages

Object:

- `gecode.solver`

Supported inlet messages:

- `config <json>`
- `config_file <absolute_path>`
- `config_dict <dict_name>`
- `config_dict_debug <dict_name>`
- `solve`
- `cancel`
- `status`
- `get_last`

## 2. Outlets

- Left outlet: list messages `voice_pitch` and `voice_rhythm`
- Middle outlet: `json` payload (status snapshots and result payloads)
- Right outlet: status updates (`unconfigured`, `idle`, `running`, `success`, `failed`, `cancelled`) plus message text

Typical lifecycle:

1. Configure (`config_file` or `config_dict`)
2. Wait for `idle`
3. Send `solve`
4. Observe `running`
5. Read result payload and terminal status

## 3. Current Config Guidance

Prefer voice-first configs:

- `voices` (+ optional `meter`)
- built-in rules with `target_voice` / `target_voices` + `target_component`
- `search_options` for search strategy

Deprecated:

- `backtrack_method`

The wrapper still normalizes many legacy config shapes, but new Max patches should emit current fields.

## 4. File-Based Workflow

```text
config_file "/Users/.../gecode-constraint-solver/configs/metric_domain_example.json"
solve
```

Use absolute paths.

## 5. Dict-Based Workflow

1. Create/load a dict.
2. Edit fields (`set ...`).
3. Send `config_dict <dict_name>`.
4. Send `solve`.

For debugging normalization:

- `config_dict_debug <dict_name>`

This emits the normalized JSON that the solver receives.

## 6. Search Strategy in Max Configs

Use `search_options`:

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

## 7. Result Payload Notes

- `voice_pitch <voice_index> ...`
- `voice_rhythm <voice_index> ...`

Rhythm lists are emitted as musical fractions. JSON may also include rhythm ticks for compatibility.

## 8. Troubleshooting

### Configure fails immediately

Check:

- absolute path correctness (`config_file`)
- dict existence (`config_dict`)
- required config sections (`solution_length`, `num_voices`, `voices`)

Use `config_dict_debug` to inspect normalized payloads.

### Solve is UNSAT

UNSAT does not poison the object.

Recommended sequence:

1. Reconfigure
2. Wait for `idle`
3. Solve again

### Long runs

- use `status` snapshots
- use `cancel` to stop current run

## 9. Consistency Note

`gecode.solver` (Max), `bin/test-max-wrapper`, and `bin/dynamic-solver` are front-ends over the same core solver. If behavior diverges, compare normalized JSON and `search_options` first.
