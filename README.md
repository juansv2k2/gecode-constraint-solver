# Gecode Musical Constraint Solver

Polyphonic musical constraint solving in C++17 on top of Gecode, with dynamic expression rules, wildcard rules, metric segmentation, and Max/MSP integration.

## What Is Current

- User-facing configs are voice-first: `voices` + optional `meter`.
- Built-in rule targeting is voice-based: `target_voice` or `target_voices` with `target_component`.
- Search strategy is configured via `search_options`.
- Dynamic expression parsing supports logical operators (`&&`, `||`, `!`, `not`), membership (`in`, `not_in`), and integer array literals (`[0, 5, 7, 12]`).
- `wildcard_constraint` rules now honor `indices` directly in wildcard expansion.
- Wrapper compatibility layer still accepts several legacy config shapes and normalizes them.

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
  "rules": [
    {
      "rule_type": "r-pitches-one-engine",
      "constraint": "all_different",
      "indices": [0, 1, 2, 3, 4, 5, 6, 7],
      "target_voice": 0,
      "target_component": "pitch"
    },
    {
      "rule_type": "r-rhythmic-uniformity",
      "constraint": "equal_values",
      "parameters": ["1/4"],
      "indices": [0, 1, 2, 3, 4, 5, 6, 7],
      "target_voice": 1,
      "target_component": "rhythm"
    },
    {
      "rule_type": "r-metric-signature",
      "constraint": "equal_values",
      "parameters": ["4/4", "3/4"],
      "timepoints": ["0q", "4q"]
    }
  ],
  "search_options": {
    "engine": "dfs",
    "branching": "first_fail",
    "value_order": "heuristic",
    "restart_policy": "none",
    "timeout_ms": 30000,
    "max_solutions": 1,
    "random_seed": 42
  },
  "output_options": {
    "export_path": "output",
    "export_json": true,
    "export_txt": true,
    "export_xml": false,
    "export_png": false,
    "export_midi": false
  }
}
```

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
- `value_order`: `min`, `random`, or `heuristic`
- `restart_policy`: `none` or `luby`

Notes:

- Legacy `branching: "sequential"` is normalized to `input_order` by the wrapper compatibility path.

## Metric Timepoints

`r-metric-signature` uses score-time boundaries (`timepoints`) in `q` units (quarter-note units).

- Example: `timepoints: ["0q", "4q", "7q"]`.
- Optional `score_length` defines total score span.

## Max Integration

Use `config_file` or `config_dict` with the `gecode.solver` object. For details, see:

- [USAGE.md](USAGE.md)
- [max-usage.md](max-usage.md)

## Documentation

- [Usage Guide](USAGE.md)
- [Max Usage](max-usage.md)
- [JSON Schema](configs/cluster_config_schema.json)
- [Harmonic Consonance Example](configs/harmonic_consonance_4voice.json)
- [Twelve-Tone Usage](docs/TWELVE_TONE_USAGE.md)
- [XML Export Guide](docs/XML_EXPORT_GUIDE.md)

## Development

```bash
make test-all
make clean
```
