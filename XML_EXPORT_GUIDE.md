# Musical XML Export System - Quick Start Guide

## Overview

The Musical Constraint Solver now exports solutions directly to **MusicXML format** in the `tests/output/` directory. All exported files are compatible with professional music notation software including MuseScore, Sibelius, Finale, and others.

## Quick Start

### 1. Test the Complete System

```bash
# Test JSON interface with XML export
make test-json-interface

# Run comprehensive XML export tests
make test-xml-export
```

### 2. Check Output Files

```bash
# View exported XML files
ls tests/output/
# Files: test_composition.xml, simple_melody.xml, two_voice_harmony.xml, etc.
```

### 3. Open in Music Software

- **MuseScore**: File → Open → Select `.xml` file
- **Sibelius**: File → Open → Select `.xml` file
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
