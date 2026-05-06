# Benchmarks

This document collects benchmark and performance comparison results that were previously embedded in the main README.

## Benchmark Results (May 2026)

The following results were measured from logs in `benchmark-test/`.

### Cross-Engine Benchmarks

| Benchmark Scenario                                                | Gecode Solver                                          | Lisp Cluster Engine                                                          |
| ----------------------------------------------------------------- | ------------------------------------------------------ | ---------------------------------------------------------------------------- |
| Equivalent benchmark (single solution)                            | 4 ms (`gecode_equiv.log`)                              | 0.002 s = 2 ms engine time (`lisp_equiv.log`)                                |
| 12-tone retrograde random benchmark (exact retrograde, v2 script) | 5 ms (`gecode_retrograde_random_benchmark.log`)        | 6.653 s = 6653 ms engine time (`cluster_retrograde_random_benchmark_v2.log`) |
| 2-voice global signature stress (60s timeout, matched)            | 13 ms (`gecode_two_voice_global_signature_stress.log`) | 188 steps, 0.009 s real (`cluster_two_voice_global_signature_stress.log`)    |

### Stress and Repeated Runs (Gecode)

| Benchmark                                                            | Result                        |
| -------------------------------------------------------------------- | ----------------------------- |
| Stress run (`gecode_stress_run.log`)                                 | 29 ms                         |
| Balanced stress run (`gecode_balanced.log`)                          | 17 ms                         |
| Balanced repeated runs (`gecode_bal_1.log` ... `gecode_bal_5.log`)   | 9 ms each run (avg 9 ms)      |
| Polyphonic repeated runs (`gecode_run_1.log` ... `gecode_run_5.log`) | 3, 2, 2, 2, 3 ms (avg 2.4 ms) |

### Matched Two-Voice Global Signature Stress (60s Timeout)

This benchmark is designed to force deeper candidate iteration with a late-pruning global rule per voice:

$$
\left(\sum_{i=0}^{10} (i+1)\,|x_{i+1}-x_i|\right)\bmod 53 = 13
$$

Both engines use:

- 2 voices, length 12, pitch domain 60-71
- per-voice all-different (strict)
- no aligned unisons across voices
- 60s timeout

Artifacts:

- Gecode config/log: `benchmark-test/gecode_two_voice_global_signature_stress.json`, `benchmark-test/gecode_two_voice_global_signature_stress.log`
- Cluster script/log: `benchmark-test/cluster_two_voice_global_signature_stress.lisp`, `benchmark-test/cluster_two_voice_global_signature_stress.log`

Observed run:

- Gecode: ~141–200 ms solve time (global `mod()` constraints fully enforced via structured AST)
- Cluster Engine: 188 steps, 0.009 s real time

### Seeded Randomization Validation (Gecode Retrograde)

`search_options.random_seed` changes candidate order and produces different valid retrograde rows:

| Seed   | Solve Time | Log                                |
| ------ | ---------- | ---------------------------------- |
| 1      | 5 ms       | `gecode_retrograde_seed1.log`      |
| 2      | 0 ms       | `gecode_retrograde_seed2.log`      |
| 424242 | 4 ms       | `gecode_retrograde_seed424242.log` |

Reference solutions for seed runs are recorded in each seed log. A Lisp solution dump is also available in `cluster_retrograde_solution_dump.log`.

### Four-Voice Transformational 12-Tone Benchmark

4 voices: V0 = random 12-tone row, V1 = retrograde of V0, V2 = inversion of V0, V3 = retrograde-inversion of V0.  
Pitch domain: 56–67 (G#3–G4). Inversion axis: D#4/E♭4 (61.5). Configs: `benchmark-test/gecode_four_voice_transform_benchmark.json` / `benchmark-test/cluster_four_voice_transform_benchmark.lisp`.

#### Gecode — Multi-Seed Comparison

| Seed   | Solve Time | V0 (prime row)                           | Log                                         |
| ------ | ---------- | ---------------------------------------- | ------------------------------------------- |
| 424242 | 9 ms       | F#4 A3 D4 G#3 E4 C4 C#4 B3 G4 A#3 F4 D#4 | `gecode_four_voice_transform_benchmark.log` |
| 777    | 9 ms       | D#4 F#4 A3 G#3 G4 B3 A#3 F4 D4 C#4 C4 E4 | `gecode_four_voice_seed777.log`             |

Each seed yields a different valid 12-tone row. V1 is the exact retrograde, V2 the inversion, V3 the retrograde-inversion — all verified by 102 Gecode constraints.

#### Lisp Cluster Engine

| Benchmark                                          | Engine Time | Real Time | Log                                          |
| -------------------------------------------------- | ----------- | --------- | -------------------------------------------- |
| Four-voice transformational 12-tone (single solve) | ~1 ms       | ~1 ms     | `cluster_four_voice_transform_benchmark.log` |

**Approach:** ClusterEngine's backtracking solves V0 (all-different 12-tone row in 28 steps, ~1 ms engine time). V1/V2/V3 are derived directly: retrograde = `reverse(V0)`, inversion = `123 - V0[i]`, retro-inversion = `reverse(inversion)`. This is the correct Cluster Engine strategy — the engine's backtracking enforces the hard constraint (all-different), and the deterministic transformations follow analytically.

**Valid solutions produced (sample run):**

```text
Voice 0 (prime):          (62 60 57 58 61 59 63 65 66 64 67 56)
Voice 1 (retrograde):     (56 67 64 66 65 63 59 61 58 57 60 62)
Voice 2 (inversion):      (61 63 66 65 62 64 60 58 57 59 56 67)
Voice 3 (retro-inversion):(67 56 59 57 58 60 64 62 65 66 63 61)
```

All four voices verified: 12 distinct pitches each, retrograde/inversion/RI relationships exact.

## Heuristic Wildcard Validation (May 2026)

Recent heuristic wildcard extensions were validated with `bin/test-max-wrapper`.

### Single-Voice Wildcard Heuristic

Config: `configs/heuristic_wildcard_stepwise_8x1.json`

Observed:

- wildcard expansion: 7 concrete heuristic rules
- total registered heuristics: 8
- status: success, found_solution=true

### Pairwise Cross-Voice Wildcard Heuristic

Config: `configs/heuristic_wildcard_crossvoice_pairs_8x2.json`

Observed:

- wildcard expansion: 8 concrete heuristic rules (1 voice pair x 8 positions)
- total registered heuristics: 9
- status: success, found_solution=true

### Regression Checks

Configs:

- `configs/heuristic_smoke_8x1.json`
- `configs/stress_hard_32x3_dense.json`

Observed:

- both succeeded after wildcard-heuristic integration.
