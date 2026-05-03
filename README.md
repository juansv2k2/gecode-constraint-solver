# Gecode Musical Constraint Solver

**Polyphonic Musical Constraint System**

A C++ implementation of a polyphonic musical constraint solving with dynamic rule system, music intelligence and full arithmetic expression parsing. Inspired on the advanced capabilities of Cluster-Engine by Örjan Sandred, optimized for the Gecode constraint programming.

## Technical Architecture

## Features

- **Dynamic rules using Algebraic Expressions**: For example `voice[v].pitch[i+1] == voice[v].pitch[i] + 3`
- **Wildcard Constraint Rules**: Sliding window patterns on single- and cross-voice relationships.
- **Multi-Engine Architecture**: Separate rhythm/pitch engines per voice + global metric engine.
- **DualSolutionStorage**: Absolute and interval representation.
- **JSON Configuration**: Configuration interface with dynamic constraints.
- **MusicXML Export**: Direct export to standard notation format.
- **Fast Performance**: Sub-millisecond constraint solving.

### Core Components

- **MusicalConstraintSolver**: Main interface and configuration.
- **GecodeClusterIntegration**: Constraint programming bridge between Cluster and Gecode.
- **AdvancedBackjumping**: backjumping system (TODO).

**Test Status**: Interface Working | Configuration Working | Utilities Working | Solving Algorithm On Development.

## Cluster Engine Architecture

The implementation follows the Cluster-Engine architecture with:

- **Multi-Engine Coordination**: Rhythm and pitch engines working.
- **Musical Domain Intelligence**: Onset grids, beat structures, musical domains (TO DO).
- **Heuristic Guidance System**: Musical intelligence for candidate sorting.
- **Advanced Backjumping**: Musical context-aware search strategies.
- **Rule Interface System**: Specialized musical constraint types.

### Constraint Types

- **Single- and Multi-voice Constraints**: Sophisticated multi-voice relation rules.
- **Arithmetic Relations**: Mathematical relationships between musical elements.
- **Interval Constraints**: Control melodic steps and harmonic intervals.
- **Temporal Patterns**: Rhythmic and metric constraint systems.

## Extending the Solver

To add new musical constraints:

1. **Cluster Engine advanced rule structures**: Examine rule types in `cluster-engine-sources/`
2. **Refine constraint configuration**: Investigate rule creation in JSON configuration.
3. **Investigate heuristic constraints logic**: Add weights, perhaps corpus-based integrations.

## Next Steps

Potential extension by adding pre-compiled rules (for example JBS library):

- Generic rules (repetition, difference, etc.)
- Interval rules
- Pitch/MOD rules
- Shaping rules
- Distance rules
- Structure rules
- Matrix rules
- Markov rules

## Repository Structure

```
├── bin/                   # Compiled executables
├── configs/               # JSON configuration files
├── docs/                  # Documentation and guides
├── include/               # Header files
├── output/                # Generated XML and result files
├── scripts/               # Python utilities (JSON→XML export)
├── src/                   # C++ source files
└── tests/                 # Test source files
```

## Quick Start

### Prerequisites

#### Core System Requirements

- **C++ Compiler**: g++ with C++17 support (GCC 7+ or Clang 5+)
- **Build System**: GNU Make
- **Package Manager**: pkg-config for library detection

#### Required Dependencies

- **Gecode**: Constraint programming library (version 6.0+)
  - Libraries: libgecodeminimodel, libgecodeint, libgecodesearch, libgecodekernel, libgecodesupport, libgecodeflatzinc

#### Python Dependencies (for XML export)

- **Python 3**: Version 3.7 or higher
- **music21**: Python library for music analysis and generation
  ```bash
  pip install music21
  ```

#### Installation Commands

**macOS (Homebrew):**

```bash
# Install Gecode
brew install gecode

# Install Python dependencies
pip3 install music21
```

**Ubuntu/Debian:**

```bash
# Install build tools and Gecode
sudo apt-get update
sudo apt-get install build-essential pkg-config libgecode-dev

# Install Python and dependencies
sudo apt-get install python3 python3-pip
pip3 install music21
```

**Fedora/Red Hat:**

```bash
# Install build tools and Gecode
sudo dnf install gcc-c++ make pkg-config gecode-devel

# Install Python dependencies
sudo dnf install python3 python3-pip
pip3 install music21
```

#### Verification

Test your installation:

```bash
# Check Gecode installation
pkg-config --exists gecode && echo "Gecode found" || echo "Gecode not found"

# Check Python dependencies
python3 -c "import music21; print('music21 available')" 2>/dev/null || echo "music21 not found"

# Check build system
make --version && echo "Make available" || echo "Make not found"
```

### Build & Run

````bash
# Build
make bin/dynamic-solver

# Test arithmetic constraints
bin/dynamic-solver configs/direct_arithmetic_test.json

# Test complex progressions
bin/dynamic-solver configs/multiple_arithmetic_test.json

## Configuration Format

```json
{
  "configuration": {
    "num_voices": 2,
    "sequence_length": 12,
    "pitch_range": [60, 80]
  },
  "rules": [
    {
      "rule_type": "wildcard_constraint",
      "wildcard_type": "sliding_window",
      "constraint": "voice[v].pitch[i+1] == voice[v].pitch[i] + 3",
      "description": "Each note +3 semitones higher"
    }
  ]
}
````

## Examples

### Arithmetic Constraints

```bash
bin/dynamic-solver configs/direct_arithmetic_test.json
# Output: C4 → D#4 (60 → 63)
```

### Musical Progressions

```bash
bin/dynamic-solver configs/multiple_arithmetic_test.json
# Output: C4 → E4 → G4 → C5 (major chord + octave)
```

### 12-Tone Generation

```bash
bin/dynamic-solver configs/twelve_tone_config.json
# Output: Complete chromatic twelve-tone row with counterpoint
```

## Performance

| Operation          | Typical Time |
| ------------------ | ------------ |
| Constraint parsing | <1ms         |
| Arithmetic solving | 0-6ms        |
| XML export         | <10ms        |

## Benchmark Results (May 2026)

The following results were measured from logs in `benchmark-test/`.

### Cross-Engine Benchmarks

| Benchmark Scenario                                                | Gecode Solver                                   | Lisp Cluster Engine                                                          |
| ----------------------------------------------------------------- | ----------------------------------------------- | ---------------------------------------------------------------------------- |
| Equivalent benchmark (single solution)                            | 4 ms (`gecode_equiv.log`)                       | 0.002 s = 2 ms engine time (`lisp_equiv.log`)                                |
| 12-tone retrograde random benchmark (exact retrograde, v2 script) | 5 ms (`gecode_retrograde_random_benchmark.log`) | 6.653 s = 6653 ms engine time (`cluster_retrograde_random_benchmark_v2.log`) |
| 2-voice global signature stress (60s timeout, matched)            | 13 ms (`gecode_two_voice_global_signature_stress.log`) | 188 steps, 0.009 s real (`cluster_two_voice_global_signature_stress.log`) |

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

```
Voice 0 (prime):          (62 60 57 58 61 59 63 65 66 64 67 56)
Voice 1 (retrograde):     (56 67 64 66 65 63 59 61 58 57 60 62)
Voice 2 (inversion):      (61 63 66 65 62 64 60 58 57 59 56 67)
Voice 3 (retro-inversion):(67 56 59 57 58 60 64 62 65 66 63 61)
```

All four voices verified: 12 distinct pitches each ✓, retrograde/inversion/RI relationships exact ✓.

## Documentation

- [Twelve-Tone Usage Guide](docs/TWELVE_TONE_USAGE.md)
- [XML Export Guide](docs/XML_EXPORT_GUIDE.md)
- [JSON Schema](configs/cluster_config_schema.json)

## Development

```bash
make bin/dynamic-solver     # Main interface
make test-all               # Run all tests
make clean                  # Clean build artifacts
```

### Working Tests (SAFE TO RUN)

```bash
# Interface functionality test (Comprehensive, Safe)
make test-main-interface && ./test-main-interface

# Interface examples (Comprehensive Examples)
make main-interface-example && ./main-interface-example

# Core validation (Basic Integration Test)
make validate-production && ./simple-gecode-cluster-validation
```

### Tests with Known Issues

```bash
# Has solver bounds issues - fixable
make test-production && ./test-musical-constraint-solver
```

### Test Coverage

- **Interface Setup & Configuration**: All working
- **Rule Factory & Management**: Complete coverage
- **Utilities & Analysis**: MIDI/interval conversion working
- **Performance Monitoring**: Statistics and timing working
- **Integration Architecture**: Gecode-cluster bridge ready
- **Actual Solving**: Has bounds checking issue (debugging needed)

### Latest Achievements

- **Arithmetic Constraints**: Native `voice[v].pitch[i] + 3` support
- **Pattern Variables**: Dynamic `i`, `v` substitution
- **Sliding Window**: Sequential constraint application

### Complete Usage Examples

- **[test_main_interface.cpp](test_main_interface.cpp)** - Interface test covering all functionality
- **[main_interface_example.cpp](main_interface_example.cpp)** - Comprehensive usage examples
- **[simple_gecode_cluster_validation.cpp](simple_gecode_cluster_validation.cpp)** - Core validation

### Adding Musical Rules

```cpp
// Implement MusicalRule interface
class CustomRule : public MusicalRule {
    bool check(const MusicalSolution& solution) override;
    std::string description() override;
};
```

### Adding Backjumping Strategies

```cpp
// Implement BackjumpStrategy interface
class CustomStrategy : public BackjumpStrategy {
    bool should_backjump(const SearchState& state) override;
    int determine_backjump_level(const SearchState& state) override;
};
```

## License

This musical constraint solver is based on Gecode examples and inspired by JBS-Constraints. See respective licenses for details.
an experimental constraint solver based on Gecode
