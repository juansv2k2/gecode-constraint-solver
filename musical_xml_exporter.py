#!/usr/bin/env python3
"""
musical_xml_exporter.py - Export musical solutions to XML using music21

Converts JSON solution data from the Musical Constraint Solver to proper 
musical XML format using the music21 library.
"""

import json
import sys
import os
from typing import List, Dict, Any, Optional
from pathlib import Path

try:
    from music21 import stream, note, duration, meter, key, pitch, metadata, tempo
    MUSIC21_AVAILABLE = True
except ImportError:
    MUSIC21_AVAILABLE = False
    print("Warning: music21 not available. Install with: pip install music21")

class MusicalXMLExporter:
    """Export musical constraint solver solutions to XML using music21"""
    
    def __init__(self, output_dir: str = "tests/output"):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
    def export_solution_to_xml(self, solution_data: Dict[str, Any], filename: str) -> bool:
        """
        Export a musical solution to XML format
        
        Args:
            solution_data: Dictionary containing solution data
            filename: Output XML filename (without extension)
            
        Returns:
            True if export successful, False otherwise
        """
        if not MUSIC21_AVAILABLE:
            print("Error: music21 library not available")
            return False
            
        try:
            # Create music21 score
            score = stream.Score()
            
            # Add metadata
            if "metadata" in solution_data:
                meta = metadata.Metadata()
                meta.title = solution_data["metadata"].get("title", "Generated Musical Sequence")
                meta.composer = solution_data["metadata"].get("composer", "Musical Constraint Solver")
                score.append(meta)
            
            # Set default tempo
            score.append(tempo.TempoIndication(number=120))
            
            # Process voices/engines
            if "voices" in solution_data:
                # New format from our constraint solver
                self._process_voices_solution(score, solution_data["voices"])
            elif "solutions" in solution_data:
                self._process_multi_voice_solution(score, solution_data["solutions"])
            elif "absolute_notes" in solution_data:
                # Single voice solution
                self._process_single_voice_solution(score, solution_data)
            
            # Write XML file
            xml_path = self.output_dir / f"{filename}.xml" 
            score.write('musicxml', fp=str(xml_path))
            
            print(f"✅ XML exported successfully: {xml_path}")
            return True
            
        except Exception as e:
            print(f"❌ Error exporting XML: {e}")
            return False
    
    def _process_voices_solution(self, score: stream.Score, voices_data: List[Dict]):
        """Process voices solution data from our constraint solver format"""
        
        for voice_data in voices_data:
            voice_id = voice_data.get("voice", 0)
            part = stream.Part()
            part.partName = f"Voice {voice_id + 1}"
            
            # Add time signature (default 4/4)
            part.append(meter.TimeSignature('4/4'))
            
            # Get pitch and rhythm data
            pitch_solution = voice_data.get("pitch_solution", [])
            rhythm_solution = voice_data.get("rhythm_solution", [])
            
            # Convert rhythm values to proper format for _add_notes_to_part
            # Our rhythm_solution contains values like 4 (quarter note), 8 (eighth), etc.
            rhythm_fractions = []
            for r in rhythm_solution:
                if r == 4:
                    rhythm_fractions.append([1, 4])  # Quarter note
                elif r == 8:
                    rhythm_fractions.append([1, 8])  # Eighth note
                elif r == 2:
                    rhythm_fractions.append([1, 2])  # Half note
                else:
                    rhythm_fractions.append([1, 4])  # Default to quarter note
            
            self._add_notes_to_part(part, pitch_solution, rhythm_fractions)
            score.append(part)
    
    def _process_multi_voice_solution(self, score: stream.Score, solutions: List[List[int]]):
        """Process multi-voice solution data"""
        
        # Determine number of voices from engine count
        num_engines = len(solutions)
        if num_engines == 0:
            return
            
        # Calculate voices (engines = 2*voices + 1, last engine is metric)
        num_voices = (num_engines - 1) // 2
        
        for voice_idx in range(num_voices):
            rhythm_engine = voice_idx * 2
            pitch_engine = voice_idx * 2 + 1
            
            if pitch_engine < len(solutions):
                part = stream.Part()
                part.partName = f"Voice {voice_idx + 1}"
                
                # Add time signature (default 4/4)
                part.append(meter.TimeSignature('4/4'))
                
                # Process notes
                pitch_data = solutions[pitch_engine]
                rhythm_data = solutions[rhythm_engine] if rhythm_engine < len(solutions) else []
                
                self._add_notes_to_part(part, pitch_data, rhythm_data)
                score.append(part)
    
    def _process_single_voice_solution(self, score: stream.Score, solution_data: Dict[str, Any]):
        """Process single voice solution data"""
        
        part = stream.Part()
        part.partName = "Generated Voice"
        
        # Add time signature
        part.append(meter.TimeSignature('4/4'))
        
        # Get note data
        note_data = solution_data.get("absolute_notes", [])
        rhythm_data = solution_data.get("rhythm_values", [])
        
        self._add_notes_to_part(part, note_data, rhythm_data)
        score.append(part)
    
    def _add_notes_to_part(self, part: stream.Part, pitch_data: List[int], rhythm_data: List[Any]):
        """Add notes to a music21 part"""
        
        for i, midi_pitch in enumerate(pitch_data):
            if midi_pitch <= 0:
                continue
                
            # Create note
            n = note.Note(midi=midi_pitch)
            
            # Set duration
            if i < len(rhythm_data):
                dur_value = self._parse_duration(rhythm_data[i])
                n.duration = duration.Duration(quarterLength=dur_value)
            else:
                n.duration = duration.Duration(quarterLength=1.0)  # Default quarter note
                
            part.append(n)
    
    def _parse_duration(self, dur_data: Any) -> float:
        """Parse duration data to quarter note length"""
        
        if isinstance(dur_data, list) and len(dur_data) == 2:
            # Fraction format [numerator, denominator] represents note value
            # [1, 4] = quarter note = 1.0 quarterLength
            # [1, 8] = eighth note = 0.5 quarterLength  
            # [1, 16] = sixteenth note = 0.25 quarterLength
            numerator = float(dur_data[0])
            denominator = float(dur_data[1])
            return numerator * (4.0 / denominator)  # Convert note value to quarter-note duration
        elif isinstance(dur_data, str):
            # String fraction like "1/4"  
            if '/' in dur_data:
                num, den = dur_data.split('/')
                numerator = float(num)
                denominator = float(den)
                return numerator * (4.0 / denominator)
        elif isinstance(dur_data, (int, float)):
            return float(dur_data)
            
        return 1.0  # Default quarter note
    
    def export_batch_solutions(self, json_file: str, output_prefix: str = "solution") -> int:
        """
        Export multiple solutions from a JSON file
        
        Args:
            json_file: Path to JSON file containing solutions
            output_prefix: Prefix for output filenames
            
        Returns:
            Number of successfully exported files
        """
        try:
            with open(json_file, 'r') as f:
                data = json.load(f)
            
            exported_count = 0
            
            if isinstance(data, list):
                # Multiple solutions
                for i, solution in enumerate(data):
                    filename = f"{output_prefix}_{i:03d}"
                    if self.export_solution_to_xml(solution, filename):
                        exported_count += 1
            else:
                # Single solution
                if self.export_solution_to_xml(data, output_prefix):
                    exported_count = 1
            
            print(f"✅ Exported {exported_count} XML files to {self.output_dir}")
            return exported_count
            
        except Exception as e:
            print(f"❌ Error processing JSON file: {e}")
            return 0

def main():
    """Command line interface for XML export"""
    
    if len(sys.argv) < 2:
        print("Usage: python musical_xml_exporter.py <input_json> [output_prefix]")
        print("Example: python musical_xml_exporter.py solution.json my_composition")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_prefix = sys.argv[2] if len(sys.argv) > 2 else "solution"
    
    if not os.path.exists(input_file):
        print(f"❌ Error: Input file '{input_file}' not found")
        sys.exit(1)
    
    exporter = MusicalXMLExporter()
    count = exporter.export_batch_solutions(input_file, output_prefix)
    
    if count > 0:
        print(f"🎼 Successfully exported {count} musical XML files!")
        sys.exit(0)
    else:
        print("❌ No files exported")
        sys.exit(1)

if __name__ == "__main__":
    main()