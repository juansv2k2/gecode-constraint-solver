# Musical XML Export System - Corrected Guide

## Overview

The Musical Constraint Solver now exports solutions to **proper MusicXML 4.0 format** using the music21 library. All exported files are fully compatible with professional music notation software including MuseScore, Sibelius, Finale, and others.

## Quick Start

### 1. Generate Musical Solutions

```bash
# Generate a musical composition
./dynamic-solver twelve_tone_config.json

# This creates: tests/output/twelve_tone_config_result.json
```

### 2. Convert to MusicXML

```bash
# Convert JSON to proper MusicXML
python3 json_to_xml.py tests/output/twelve_tone_config_result.json

# Creates: tests/output/twelve_tone_config_result.xml (proper MusicXML)
```

### 3. Open in Music Software

The generated XML files are now in **proper MusicXML 4.0 format** and can be opened directly in:

- **MuseScore**: File → Open → Select `.xml` file
- **Sibelius**: File → Open → Select `.xml` file
- **Finale**: File → Import → Select MusicXML
- **Dorico**: File → Import → Music XML Files

## Files Generated

### Proper MusicXML Format

- `tests/output/twelve_tone_config_result.xml` - Complete MusicXML with:
  - Multiple voices/parts
  - Proper note pitch and duration information
  - Time signatures and clef information
  - Professional metadata

## Technical Details

### MusicXML Format Validation

- ✅ **DOCTYPE Declaration**: Proper MusicXML 4.0 DOCTYPE
- ✅ **Score Structure**: `<score-partwise>` with proper hierarchy
- ✅ **Part List**: Multiple voices exported as separate parts
- ✅ **Note Data**: MIDI pitches converted to proper step/octave notation
- ✅ **Durations**: Rhythm values converted to MusicXML format
- ✅ **Metadata**: Composer, software, and timestamp information

### Compatible Software Tested

- **Format**: MusicXML 4.0 Partwise
- **Encoding**: UTF-8
- **Music21 Engine**: v9.9.1 (professional music library)
- **Standard Compliance**: Full MusicXML specification compliance
- **Finale**: File → Open → MusicXML Files → Select `.xml` file

## File Types Generated

| File                     | Description          | Voices | Style        |
| ------------------------ | -------------------- | ------ | ------------ |
| `simple_melody.xml`      | Single voice melody  | 1      | Simple       |
| `two_voice_harmony.xml`  | Harmonic composition | 2      | Classical    |
| `complex_multivoice.xml` | Advanced multi-part  | 2      | Contemporary |
| `jazz_syncopation.xml`   | Jazz rhythms         | 2      | Jazz         |
| `test_composition.xml`   | From JSON interface  | 2      | Intelligent  |

## Technical Details

### XML Format Features

- ✅ **MusicXML 4.0 Standard**: Full compliance
- ✅ **Multi-Voice Support**: Proper voice separation
- ✅ **Time Signatures**: 4/4, 3/4, and custom signatures
- ✅ **Note Values**: From 64th notes to whole notes
- ✅ **Professional Metadata**: Title, composer, timestamps

### Export Methods

1. **Automatic**: Solutions automatically exported during solving
2. **Manual**: Call `export_to_xml()` method in C++ interface
3. **Batch**: Use `musical_xml_exporter.py` for multiple files
4. **Command Line**: Direct Python script usage

### Dependencies

```bash
# Required for XML export
pip install music21
```

## Usage Examples

### From C++ Interface

```cpp
#include "cluster_engine_json_interface.hh"
using namespace ClusterEngineJSON;

ClusterEngineJSONInterface interface;
interface.load_configuration("config.json");
interface.execute();
interface.export_to_xml("my_composition", "tests/output");
```

### From Command Line

```bash
# Convert JSON solution to XML
python3 musical_xml_exporter.py solution.json my_piece

# Batch convert multiple solutions
python3 musical_xml_exporter.py batch_solutions.json composition
```

## File Locations

- **Input**: JSON configuration files in project root
- **Output**: XML files in `tests/output/` directory
- **Scripts**: `musical_xml_exporter.py`, `test_xml_export.py`

## Verification

The system automatically validates all XML files for:

- Valid MusicXML structure
- Required elements (score-partwise, part-list, measure)
- Proper voice assignments
- Correct note and rhythm encoding

🎼 **All exported XML files are ready for immediate use in professional music notation software!**
