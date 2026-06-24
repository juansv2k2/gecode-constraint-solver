# Neural Pitch Scorer — Test Results

Results are appended chronologically. Each entry records the test date, model
configuration, and assertion outcomes so regressions are immediately visible.

---

## Run 1 — 2026-06-24

### Setup

| Field         | Value                                                                                  |
| ------------- | -------------------------------------------------------------------------------------- |
| Script        | `scripts/test_neural_influence.py`                                                     |
| Solver        | `bin/dynamic-solver`                                                                   |
| Model         | `datasets/pitch_mlp_weights.json` — **classification**, context=4, hidden=32, vocab=28 |
| Scoring       | Gumbel-max: `score = logit_i / T + gumbel(seed, voice, pos, cand)`                     |
| Temperature   | 1.0 (trained probabilities as-is)                                                      |
| Config        | `configs/neural_pitch_folk_16x1.json` — 16 notes, 1 voice, MIDI 55–81                  |
| Seeds         | 1 – 30 (480 notes per mode)                                                            |
| Constraints   | `abs(pitch[i+1] − pitch[i]) ≤ 7`, `pitch[i] ≠ pitch[i+1]`                              |
| Baseline      | `value_order: "random"` with same seeds                                                |
| Training data | `datasets/pitch_<100.txt` — 4334 Weimar folk melody pitches, mean 68.5                 |

---

### Assertions

| #   | Property                                         |  Result  | Neural         | Random / threshold |
| --- | ------------------------------------------------ | :------: | -------------- | ------------------ |
| 1   | Out-of-vocab pitches (56, 80) never selected     | **PASS** | 0 / 480 (0.0%) | 25 / 480 (5.2%)    |
| 2   | Random baseline selects out-of-vocab pitches     | **PASS** | —              | 25 occurrences     |
| 3   | Neural mean pitch closer to training mean (68.5) | **PASS** | 68.69 (Δ=0.19) | 67.13 (Δ=1.37)     |
| 4   | Variety: ≥ 50% unique melodies across seeds      | **PASS** | 30 / 30 (100%) | threshold 50%      |

**Overall: PASS** (4/4)

---

### Distribution Analysis

480 notes collected per mode (30 seeds × 16 notes).

#### Summary Statistics

| Metric                |      Neural | Random | Training data |
| --------------------- | ----------: | -----: | ------------: |
| Notes collected       |         480 |    480 |         4 327 |
| Mean MIDI pitch       |   **68.69** |  67.13 |         68.57 |
| \|mean − 68.5\|       |    **0.19** |   1.37 |          0.07 |
| Out-of-vocab count    |       **0** |     25 |             — |
| Out-of-vocab rate     |    **0.0%** |   5.2% |             — |
| Pearson r vs training |   **0.983** | −0.205 |         1.000 |
| Unique melodies       | **30 / 30** |      — |             — |

The Pearson correlation of **r = 0.983** between neural and training distributions
(vs **r = −0.205** for random) is the key indicator: the model has learned the
folk-melody pitch profile and reproduces it under constraint.

---

#### Pitch-by-Pitch Distribution

Bar width represents percentage of total notes (scale: each `█` ≈ 1%).
`✗` marks pitches absent from the training vocabulary; those receive a hard
floor score of −20 so they are never selected by the neural mode.

```
MIDI  Note    Vocab   Neural %  [── 20 chars ───────]  Random %  [── 20 chars ───────]  Train %
────  ──────  ──────  ────────  ────────────────────   ────────  ────────────────────   ───────
 55   G3        ✓      0.4 %   ░░░░░░░░░░░░░░░░░░░░    2.1 %   ██░░░░░░░░░░░░░░░░░░    0.5 %
 56   G#3       ✗ OOV  0.0 %   ····················    3.1 %   ███░░░░░░░░░░░░░░░░░░    —
 57   A3        ✓      0.2 %   ░░░░░░░░░░░░░░░░░░░░    3.8 %   ████░░░░░░░░░░░░░░░░    0.8 %
 58   A#3       ✓      0.0 %   ░░░░░░░░░░░░░░░░░░░░    3.8 %   ████░░░░░░░░░░░░░░░░    0.1 %
 59   B3        ✓      0.8 %   █░░░░░░░░░░░░░░░░░░░    4.6 %   █████░░░░░░░░░░░░░░░    0.9 %
 60   C4        ✓      0.8 %   █░░░░░░░░░░░░░░░░░░░    4.0 %   ████░░░░░░░░░░░░░░░░    3.2 %
 61   C#4       ✓      0.6 %   █░░░░░░░░░░░░░░░░░░░    6.3 %   ██████░░░░░░░░░░░░░░    0.2 %
 62   D4        ✓      3.3 %   ███░░░░░░░░░░░░░░░░░    5.0 %   █████░░░░░░░░░░░░░░░    4.8 %
 63   D#4       ✓      0.0 %   ░░░░░░░░░░░░░░░░░░░░    5.4 %   █████░░░░░░░░░░░░░░░    0.1 %
 64   E4        ✓      4.2 %   ████░░░░░░░░░░░░░░░░    4.6 %   █████░░░░░░░░░░░░░░░    5.0 %
 65   F4        ✓     11.9 %   ████████████░░░░░░░░    3.3 %   ███░░░░░░░░░░░░░░░░░    9.7 %
 66   F#4       ✓      1.0 %   █░░░░░░░░░░░░░░░░░░░    5.0 %   █████░░░░░░░░░░░░░░░    0.5 %
 67   G4        ✓     17.5 %   █████████████████░░░    4.2 %   ████░░░░░░░░░░░░░░░░   15.0 %
 68   G#4       ✓      0.4 %   ░░░░░░░░░░░░░░░░░░░░    4.0 %   ████░░░░░░░░░░░░░░░░    0.2 %
 69   A4        ✓     19.4 %   ████████████████████    3.5 %   ████░░░░░░░░░░░░░░░░   18.7 %
 70   A#4       ✓     10.6 %   ███████████░░░░░░░░░    3.5 %   ████░░░░░░░░░░░░░░░░    8.9 %
 71   B4        ✓      5.0 %   █████░░░░░░░░░░░░░░░    3.1 %   ███░░░░░░░░░░░░░░░░░    4.2 %
 72   C5        ✓     10.8 %   ███████████░░░░░░░░░    2.3 %   ██░░░░░░░░░░░░░░░░░░   12.8 %
 73   C#5       ✓      0.0 %   ░░░░░░░░░░░░░░░░░░░░    4.2 %   ████░░░░░░░░░░░░░░░░    0.1 %
 74   D5        ✓      8.8 %   █████████░░░░░░░░░░░    2.9 %   ███░░░░░░░░░░░░░░░░░    9.1 %
 75   D#5       ✓      0.4 %   ░░░░░░░░░░░░░░░░░░░░    4.4 %   █████░░░░░░░░░░░░░░░    0.3 %
 76   E5        ✓      2.1 %   ██░░░░░░░░░░░░░░░░░░    3.5 %   ████░░░░░░░░░░░░░░░░    2.8 %
 77   F5        ✓      1.3 %   █░░░░░░░░░░░░░░░░░░░    3.3 %   ███░░░░░░░░░░░░░░░░░    1.7 %
 78   F#5       ✓      0.0 %   ░░░░░░░░░░░░░░░░░░░░    2.9 %   ███░░░░░░░░░░░░░░░░░    0.0 %
 79   G5        ✓      0.4 %   ░░░░░░░░░░░░░░░░░░░░    2.9 %   ███░░░░░░░░░░░░░░░░░    0.5 %
 80   G#5       ✗ OOV  0.0 %   ····················    2.1 %   ██░░░░░░░░░░░░░░░░░░    —
 81   A5        ✓      0.0 %   ░░░░░░░░░░░░░░░░░░░░    2.3 %   ██░░░░░░░░░░░░░░░░░░    0.1 %
```

Key observations:

- **A4 (69)** and **G4 (67)** are the top two neural choices — matching the folk dataset where they are also the top two
- **G#3 (56)** and **G#5 (80)** (out-of-vocab, marked `✗`) score **0 occurrences** in neural vs 25 in random
- Random distribution is nearly flat (~3–6% per pitch), while neural closely mirrors the training distribution peak at A4/G4/C5/F4

#### Correlation Plot — Neural vs Training (in-vocab pitches, domain 55–81)

```
Neural %
20 │                          ● A4(69)
   │
18 │                       ● G4(67)
   │
16 │
   │
14 │
   │
12 │     ● C5(72)
   │
10 │  ● F4(65)
   │
 8 │                    ● D5(74)
   │
 6 │          ● A#4(70)
   │
 4 │    ● E4(64)   ● B4(71)
   │
 2 │● D4(62) ● E5(76)
   │
 0 ┼───────────────────────────────────────── Training %
   0   2   4   6   8  10  12  14  16  18  20

   Pearson r = 0.983   (random baseline r = −0.205)
```

The tight linear relationship between neural output frequency and training
frequency confirms the model is reproducing the learned pitch distribution, not
sampling uniformly or following only the constraint structure.

---

### What This Test Does NOT Verify

- **Melodic context sensitivity**: the model uses 4-note context but this test
  only measures marginal pitch distributions. A context-sensitivity test would
  check that the model adjusts probabilities given a specific melodic trajectory
  (e.g., ascending run → next note more likely to continue up).
- **Out-of-constraint notes**: all counts are post-constraint; some training
  pitches are never reachable due to the ≤7-semitone rule.
- **Max external**: the same weights and scoring path are used in
  `gecode.solver.mxo`; a Max-specific smoke test is in `scripts/max_package_smoke.sh`.

---

<!-- To add a new run, copy the "## Run N" section below and fill in values. -->
