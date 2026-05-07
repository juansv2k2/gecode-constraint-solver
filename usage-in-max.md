# Using the Solver in Max

This guide explains how to run the `gecode.solver` Max external with file-based and dict-based configuration, how to read outlets, and how to troubleshoot common failures.

## 1. Object and Message Surface

Create the object:

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

## 2. Outlet Behavior

The object has three outlets:

- Left outlet: list messages `voice_pitch` and `voice_rhythm`
- Middle outlet: `json` payload (status snapshots and solve result payloads)
- Right outlet: status messages (`unconfigured`, `idle`, `running`, `success`, `failed`, `cancelled`) with a human-readable message

Typical solve lifecycle:

1. Configure (`config_file` or `config_dict`)
2. Receive status `idle`
3. Send `solve`
4. Receive status `running`
5. Receive result lists and JSON, then final status `success` or `failed`

## 3. Recommended Configuration Workflows

### A) File-based (stable baseline)

Use an absolute path:

- `config_file "/Users/.../gecode-constraint-solver/configs/stress_sat_24.json"`

Then:

- `solve`

### B) Dict-based (best for live edits in patchers)

1. Create `dict demo_cfg`
2. Read config into dict:
   - `read "/Users/.../configs/twelve_tone_palindrome_config.json"`
3. Apply edits with `set ...`
4. Send:
   - `config_dict demo_cfg`
5. Send:
   - `solve`

Use `config_dict_debug demo_cfg` when debugging schema translation. It emits normalized JSON so you can inspect what the solver actually receives.

## 4. Message Examples

Run a config from file:

- `config_file "/Users/.../configs/stress_near_unsat_18.json"`
- `solve`

Request status snapshot at any time:

- `status`

Cancel a long solve:

- `cancel`

Get the last completed result again:

- `get_last`

## 5. Understanding Result Payloads

Pitch and rhythm arrive both as list messages and JSON.

- `voice_pitch <voice_index> ...`
- `voice_rhythm <voice_index> ...`

Rhythms are exposed in musical fraction form for Max consumers. JSON payloads may also include raw rhythm ticks for compatibility.

## 6. Troubleshooting

### Configuration fails immediately

Symptoms:

- Status becomes `failed` after `config_*`

Checks:

- File path is absolute and readable (for `config_file`)
- Dict name exists (for `config_dict`)
- Required schema sections are present (`engine_domains`, valid rules)

Tip:

- Use `config_dict_debug <dict>` to inspect flattened or transformed dict fields before solving.

### Solve returns UNSAT, then later SAT should still work

Expected behavior:

- An UNSAT run does not permanently poison the object state.
- Reconfigure and solve again in the same object instance.

Recommended sequence:

1. `config_file` (or `config_dict`) for new scenario
2. Wait for `idle`
3. `solve`

### Long-running benchmark in Max

Use:

- `status` periodically for snapshots
- `cancel` to stop the current run

After cancel:

- Reconfigure (optional but recommended for clean intent)
- Run `solve` again

### Duplicate/noisy errors in patching flow

Prefer:

- One explicit configure step
- Then one solve step

Avoid sending overlapping `solve` commands while state is already `running`.

## 7. Practical Benchmark Loop in Max

A reliable loop for comparative tests:

1. Load config A (`config_file ...`)
2. `solve`
3. Capture final JSON and status
4. Load config B
5. `solve`
6. Repeat

For parameter sweeps, switch to dict workflow:

1. Keep one base dict loaded
2. Apply one `set ...` mutation
3. `config_dict ...`
4. `solve`
5. Record output

### Dict Array Fields

When using `config_dict`, the external automatically detects keys that should be JSON arrays (`dynamic_rules`, `rules`) and handles both single-item and multi-item dict entries correctly. If you store a single rule object in the dict at key `dynamic_rules`, it will be properly converted to `"dynamic_rules": [{...}]` in the internal JSON representation.

**Example:** Setting a single heuristic rule via dict:

```
dict ruledict @name dynamic_rules[0]
# This creates a dict entry named "dynamic_rules[0]"
dict ruledict set id prefer_parallel_fifths
dict ruledict set type heuristic_energy
dict ruledict set mode real_heuristic
dict ruledict set expression "24 - abs((voice[1].pitch[i] - voice[0].pitch[i]) - 7)"
dict ruledict set candidate_voice 1
dict ruledict set weight 10

# Then pass to solver:
config_dict ruledict
```

This is properly converted to the canonical array structure internally.

## 8. Front-End Consistency Note

`gecode.solver` (Max), `bin/test-max-wrapper`, and `bin/dynamic-solver` are front-ends over the same core solver engine. If behavior diverges, first compare normalized JSON payloads and runtime seed/search options to confirm the inputs are actually equivalent.
