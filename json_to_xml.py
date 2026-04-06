#!/usr/bin/env python3
"""
JSON to MusicXML Converter for Musical Constraint Solver
Converts JSON output to proper MusicXML format using music21
"""

import sys
import os
from pathlib import Path

# Import the proper musical XML exporter
from musical_xml_exporter import MusicalXMLExporter

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 json_to_xml.py <input.json> [output_name]")
        print("Example: python3 json_to_xml.py tests/output/result.json composition")
        sys.exit(1)
    
    json_file = sys.argv[1]
    
    # Determine output name
    if len(sys.argv) >= 3:
        output_name = sys.argv[2]
    else:
        # Generate output name from input filename
        json_path = Path(json_file)
        output_name = json_path.stem
    
    # Create exporter and convert
    exporter = MusicalXMLExporter()
    
    try:
        import json
        with open(json_file, 'r') as f:
            data = json.load(f)
        
        success = exporter.export_solution_to_xml(data, output_name)
        
        if success:
            output_path = Path("tests/output") / f"{output_name}.xml"
            print(f"✅ XML exported: {output_path}")
        else:
            print("❌ XML export failed")
            sys.exit(1)
            
    except Exception as e:
        print(f"❌ Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()