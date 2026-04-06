# 12-Tone Row Generator Usage Guide

## Quick Start

To run the 12-tone row generator with configuration file:

```bash
# Build the system
make test-twelve-tone

# Run with configuration
./test-twelve-tone
```

## What It Does

1. **Reads Configuration**: Uses `twelve_tone_config.json` to specify:
   - Two voices (original row + retrograde inversion)
   - 12-note sequences using all chromatic pitches
   - Constraint solving parameters

2. **Generates Music**: Creates a valid 12-tone row where:
   - All 12 chromatic pitch classes appear exactly once
   - No repeated notes in the sequence
   - Follows Schoenberg's twelve-tone technique

3. **Creates Retrograde Inversion**: Automatically computes the traditional transformation:
   - Backwards sequence (retrograde)
   - Inverted intervals (inversion)

4. **Exports Results**:
   - JSON format (`tests/output/twelve_tone_result.json`)
   - Human-readable text (`tests/output/twelve_tone_result.txt`)
   - Performance statistics

## Configuration File Structure

The `twelve_tone_config.json` file specifies:

- Solution length (12 notes)
- Number of voices (2)
- Rules for 12-tone composition
- Domain constraints (C4-B4 range)
- Search and output options

## System Architecture

- **Main Interface**: Uses constraint solver with JSON configuration
- **Musical Rules**: Custom 12-tone rule ensures all pitch classes used
- **Constraint Solving**: Gecode-based constraint programming
- **Musical Intelligence**: Understands serial composition techniques
- **Backtracking**: Intelligent search with musical knowledge

## Example Output

Voice 0 (Original): C4 → C#4 → D4 → D#4 → E4 → F4 → F#4 → G4 → G#4 → A4 → A#4 → B4
Voice 1 (RI): B4 → A#4 → A4 → G#4 → G4 → F#4 → F4 → E4 → D#4 → D4 → C#4 → C4

Both voices use all 12 chromatic pitch classes exactly once, following authentic twelve-tone technique.
