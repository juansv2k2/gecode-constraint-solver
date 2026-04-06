#!/usr/bin/env python3
"""
XML Export Post-Processor for Musical Constraint Solver
Converts JSON output to structured XML format
"""

import json
import sys
import os

def midi_to_note_name(midi):
    """Convert MIDI note number to note name"""
    note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    octave = (midi // 12) - 1
    note = midi % 12
    return f"{note_names[note]}{octave}"

def export_to_xml(json_file, xml_file):
    """Convert JSON output to XML format"""
    try:
        with open(json_file, 'r') as f:
            data = json.load(f)
        
        with open(xml_file, 'w') as xml:
            xml.write('<?xml version="1.0" encoding="UTF-8"?>\n')
            xml.write('<musical_solution>\n')
            
            # Metadata
            xml.write('  <metadata>\n')
            xml.write(f'    <config_file>{data.get("config_file", "")}</config_file>\n')
            xml.write(f'    <problem_name>{data.get("problem_name", "")}</problem_name>\n')
            xml.write(f'    <solve_time_ms>{data.get("solve_time_ms", 0)}</solve_time_ms>\n')
            xml.write(f'    <rules_applied>{data.get("rules_applied", 0)}</rules_applied>\n')
            xml.write(f'    <rules_checked>{data.get("rules_checked", 0)}</rules_checked>\n')
            xml.write('  </metadata>\n')
            
            # Voices
            if "voices" in data:
                xml.write('  <voices>\n')
                for voice in data["voices"]:
                    voice_id = voice["voice"]
                    xml.write(f'    <voice id="{voice_id}">\n')
                    
                    # Pitch sequence
                    xml.write('      <pitch_sequence>\n')
                    if "pitch_solution" in voice:
                        for i, midi in enumerate(voice["pitch_solution"]):
                            note_name = midi_to_note_name(midi)
                            xml.write(f'        <note position="{i+1}" midi="{midi}" name="{note_name}"/>\n')
                    xml.write('      </pitch_sequence>\n')
                    
                    # Rhythm sequence
                    if "rhythm_solution" in voice:
                        xml.write('      <rhythm_sequence>\n')
                        for i, duration in enumerate(voice["rhythm_solution"]):
                            note_type = f"1/{16 // duration}"
                            xml.write(f'        <duration position="{i+1}" value="{duration}" note_type="{note_type}"/>\n')
                        xml.write('      </rhythm_sequence>\n')
                    
                    xml.write('    </voice>\n')
                xml.write('  </voices>\n')
            
            # Metric signature
            if "metric_signature" in data:
                xml.write('  <metric_signature>\n')
                for metric in data["metric_signature"]:
                    xml.write(f'    <time_signature numerator="{metric}" denominator="4"/>\n')
                xml.write('  </metric_signature>\n')
            
            xml.write('</musical_solution>\n')
        
        print(f"✅ XML exported: {xml_file}")
        
    except Exception as e:
        print(f"❌ Error creating XML: {e}")

def main():
    """Main function"""
    if len(sys.argv) < 2:
        print("Usage: python3 json_to_xml.py <json_file>")
        sys.exit(1)
    
    json_file = sys.argv[1]
    xml_file = os.path.splitext(json_file)[0] + ".xml"
    
    export_to_xml(json_file, xml_file)

if __name__ == "__main__":
    main()