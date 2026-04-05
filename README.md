# Musical Constraint Solver with Gecode

A simple musical constraint programming solver built with Gecode, inspired by the JBS-Constraints library for PWGL/OpenMusic.

## Overview

This project implements musical constraints using the Gecode constraint programming toolkit. It demonstrates how to translate musical rules from the JBS-Constraints library into efficient C++ constraint programs.

## Current Implementation

The `musical-solver.cpp` implements a basic melodic constraint solver with three models:

### Model 1: Basic (no-repetition-rule)

- **Constraint**: No repeated notes in the sequence
- **Inspired by**: `no-repetition-rule` from JBS constraints
- **Implementation**: Uses Gecode's `distinct()` constraint on MIDI note variables

### Model 2: Intervals

- **Constraints**:
  - No repeated notes
  - Maximum interval size (no larger than octave)
- **Inspired by**: `not-higher-interval-rule` from JBS constraints
- **Implementation**: Calculates intervals between consecutive notes and constrains them

### Model 3: Melodic

- **Constraints**:
  - No repeated notes
  - Reasonable melodic steps (max perfect 5th)
  - Avoid consecutive large leaps
- **Inspired by**: Various melodic movement rules from JBS constraints

## JBS Constraints Reference

The original JBS constraints (`jbs-constraints/`) include sophisticated musical rules such as:

- `no-repetition-rule`: Prevents note repetitions
- `not-higher-interval-rule`: Limits maximum interval sizes
- `no-consecutive-ascending-rule`: Controls melodic direction
- Many others for rhythm, harmony, and complex musical structures

## Building and Running

### Prerequisites

- C++ compiler with C++11 support
- Gecode constraint programming library

### Installation on macOS:

```bash
# Install Gecode via Homebrew
make install-deps-macos

# Build the solver
make

# Run tests
make test
```

### Installation on Ubuntu/Debian:

```bash
# Install Gecode
make install-deps-ubuntu

# Build the solver
make

# Run tests
make test
```

### Manual Installation:

```bash
# Check if Gecode is installed
make check-gecode

# Build manually if needed
g++ -std=c++11 -O3 musical-solver.cpp -lgecode -lgecodedriver -lgecodeminimodel -lgecodeint -lgecodesearch -lgecodekernel -lgecodesupport -o musical-solver
```

## Usage Examples

```bash
# Generate 5 solutions with basic no-repetition constraint
./musical-solver -solutions 5 -model basic

# Generate melodic sequences with interval constraints
./musical-solver -solutions 3 -model intervals

# Generate musically smooth sequences
./musical-solver -solutions 3 -model melodic

# Get help on all options
./musical-solver -help
```

## Sample Output

```
Musical Sequence (MIDI notes): 60 -> 64 -> 67 -> 71 -> 74 -> 77 -> 79 -> 83
Intervals: 4, 3, 4, 3, 3, 2, 4
Note names: C4 -> E4 -> G4 -> B4 -> D5 -> F5 -> G5 -> B5
```

## Architecture

- **`musical-solver.cpp`**: Main constraint solver implementation
- **`jbs-constraints/`**: Original Lisp-based constraint library for inspiration
- **`gecode/`**: Cloned Gecode source for reference and advanced examples
- **`Makefile`**: Build system with automatic dependency detection

## Extending the Solver

To add new musical constraints:

1. **Study JBS constraint**: Look at similar rules in `jbs-constraints/pmc-boxes.lisp`
2. **Add constraint enum**: Add new model type in `MusicalSolver::enum`
3. **Implement constraint**: Add constraint logic in the constructor switch statement
4. **Test and iterate**: Use `make test` to verify the constraint behavior

## Next Steps

Potential extensions inspired by the JBS library:

- Rhythmic constraints (duration patterns)
- Harmonic constraints (chord progressions)
- Voice leading rules (multiple melodic lines)
- Modular arithmetic constraints (pitch classes)
- Advanced search strategies and heuristics

## Comparison: JBS vs Gecode

| Feature     | JBS Constraints         | This Implementation        |
| ----------- | ----------------------- | -------------------------- |
| Language    | Common Lisp             | C++                        |
| Target      | PWGL/OpenMusic          | Standalone/embeddable      |
| Performance | Interpreted             | Compiled, optimized        |
| Constraints | 50+ musical rules       | 3 basic rules (extensible) |
| Search      | Backtracking            | Advanced Gecode search     |
| Integration | Music notation software | General purpose CP solver  |

## License

This musical constraint solver is based on Gecode examples and inspired by JBS-Constraints. See respective licenses for details.
an experimental constraint solver based on Gecode
