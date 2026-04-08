# Gecode Musical Constraint Solver

**Polyphonic Musical Constraint System**

A C++ implementation of a polyphonic musical constraint solving with dynamic rule system, music intelligence and full arithmetic expression parsing. Inspired on the advanced capabilities of Cluster-Engine by Örjan Sandred, optimized for the Gecode constraint programming.

## Technical Architecture

## Features

- **Dynamic rules using Algebraic Expressions**: For example `voice[v].pitch[i+1] == voice[v].pitch[i] + 3`
- **Wildcard Constraint Rules**: Sliding window patterns on single- and cross-voice relationships.
- **Multi-Engine Architecture**: Separate rhythm/pitch engines per voice + global metric engine.
- **DualSolutionStorage**: Absolute and interval representation
- **JSON Configuration**: Configuration interface with dynamic constraints
- **MusicXML Export**: Direct export to standard notation format
- **Fast Performance**: Sub-millisecond constraint solving

### Core Components

- **MusicalConstraintSolver**: Main interface and configuration
- **GecodeClusterIntegration**: Constraint programming bridge between Cluster and Gecode
- **AdvancedBackjumping**: backjumping system (TODO)

**Test Status**: Interface Working | Configuration Working | Utilities Working | Solving Algorithm On Development

## Cluster Engine Architecture

The implementation follows the authentic Cluster-Engine architecture with:

- **Multi-Engine Coordination**: Rhythm and pitch engines working
- **Musical Domain Intelligence**: Onset grids, beat structures, musical domains (TO DO)
- **Heuristic Guidance System**: Musical intelligence for candidate sorting
- **Advanced Backjumping**: Musical context-aware search strategies
- **Rule Interface System**: Specialized musical constraint types

### Constraint Types

- **Single- and Multi-voice Constraints**: Sophisticated multi-voice relation rules.
- **Arithmetic Relations**: Mathematical relationships between musical elements.
- **Interval Constraints**: Control melodic steps and harmonic intervals.
- **Temporal Patterns**: Rhythmic and metric constraint systems

## Extending the Solver

To add new musical constraints:

1. **Cluster Engine advanced rule structures**: Examine rule types in `cluster-engine-sources/`
2. **Refine constraint configuration**: Investigate rule creation in JSON configuration.
3. **Investigate heuristic constraints logic**: Add weights, perhaps corpus-based integrations.

## Next Steps

Potential extension by adding pre-compiled rules (for example JBS library)

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
