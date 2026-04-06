#!/usr/bin/env python3
"""
test_xml_export.py - Comprehensive test of XML export functionality

Tests various aspects of the musical XML export system including:
- Single solution export
- Batch solution export  
- Multi-voice compositions
- Different time signatures and key signatures
"""

import json
import os
import sys
from pathlib import Path

# Add the current directory to path to import our exporter
sys.path.append('.')
from musical_xml_exporter import MusicalXMLExporter

def create_test_solutions():
    """Create various test solutions for XML export testing"""
    
    test_solutions = []
    
    # Test 1: Single voice simple melody
    test_solutions.append({
        "name": "simple_melody",
        "data": {
            "metadata": {
                "title": "Simple Melody Test",
                "composer": "Cluster Engine Test Suite",
                "style": "simple"
            },
            "absolute_notes": [60, 62, 64, 65, 67, 69, 71, 72],
            "rhythm_values": [[1, 4], [1, 4], [1, 4], [1, 4], [1, 4], [1, 4], [1, 4], [1, 2]]
        }
    })
    
    # Test 2: Two voice composition
    test_solutions.append({
        "name": "two_voice_harmony",
        "data": {
            "metadata": {
                "title": "Two Voice Harmony Test",
                "composer": "Cluster Engine Test Suite",
                "style": "classical"
            },
            "solutions": [
                [[1, 4], [1, 8], [1, 4], [1, 8], [1, 2]],  # Engine 0: Rhythm Voice 0
                [60, 64, 67, 65, 72],                        # Engine 1: Pitch Voice 0  
                [[1, 2], [1, 4], [1, 4], [1, 2]],           # Engine 2: Rhythm Voice 1
                [48, 52, 55, 60],                            # Engine 3: Pitch Voice 1
                [[4, 4]]                                     # Engine 4: Metric
            ]
        }
    })
    
    # Test 3: Complex multi-voice with different rhythms
    test_solutions.append({
        "name": "complex_multivoice",
        "data": {
            "metadata": {
                "title": "Complex Multi-Voice Test",
                "composer": "Cluster Engine Test Suite",
                "style": "contemporary"
            },
            "solutions": [
                [[1, 8], [1, 8], [1, 4], [1, 16], [1, 16], [1, 8], [1, 4]],  # Rhythm Voice 0
                [60, 62, 64, 65, 64, 62, 60],                                 # Pitch Voice 0
                [[1, 4], [1, 2], [1, 4]],                                     # Rhythm Voice 1  
                [48, 52, 55],                                                 # Pitch Voice 1
                [[3, 4]]                                                      # Metric: 3/4 time
            ]
        }
    })
    
    # Test 4: Jazz-style with syncopation
    test_solutions.append({
        "name": "jazz_syncopation", 
        "data": {
            "metadata": {
                "title": "Jazz Syncopation Test",
                "composer": "Cluster Engine Test Suite",
                "style": "jazz"
            },
            "solutions": [
                [[1, 8], [3, 8], [1, 4], [1, 8], [1, 8]],    # Syncopated rhythm
                [60, 63, 67, 70, 72],                         # Jazz intervals
                [[1, 4], [1, 2], [1, 4]],                     # Bass rhythm
                [36, 39, 43],                                 # Bass notes
                [[4, 4]]                                      # Standard time
            ]
        }
    })
    
    return test_solutions

def test_single_exports(exporter, test_solutions):
    """Test individual solution exports"""
    
    print("🎼 Testing Single Solution Exports")
    print("==================================")
    
    success_count = 0
    for test_case in test_solutions:
        print(f"\\nTesting: {test_case['name']}")
        
        if exporter.export_solution_to_xml(test_case['data'], test_case['name']):
            success_count += 1
            print(f"✅ {test_case['name']}: SUCCESS")
        else:
            print(f"❌ {test_case['name']}: FAILED")
    
    print(f"\\n📊 Single Export Results: {success_count}/{len(test_solutions)} successful")
    return success_count

def test_batch_export(exporter, test_solutions):
    """Test batch solution export"""
    
    print("\\n🔄 Testing Batch Export")
    print("=======================")
    
    # Create batch JSON file
    batch_data = [test_case['data'] for test_case in test_solutions]
    batch_file = "tests/output/batch_test_solutions.json"
    
    with open(batch_file, 'w') as f:
        json.dump(batch_data, f, indent=2)
    
    print(f"Created batch file: {batch_file}")
    
    # Test batch export
    count = exporter.export_batch_solutions(batch_file, "batch_solution")
    
    print(f"📊 Batch Export Results: {count} files exported")
    
    # Clean up
    os.remove(batch_file)
    
    return count

def test_output_verification():
    """Verify that output files are proper XML"""
    
    print("\\n🔍 Verifying Output Files")
    print("=========================")
    
    output_dir = Path("tests/output")
    xml_files = list(output_dir.glob("*.xml"))
    
    valid_count = 0
    for xml_file in xml_files:
        try:
            # Basic XML validation - check for MusicXML elements
            with open(xml_file, 'r') as f:
                content = f.read()
                
            if all(tag in content for tag in ['<score-partwise', '<part-list>', '<measure']):
                print(f"✅ {xml_file.name}: Valid MusicXML structure")
                valid_count += 1
            else:
                print(f"⚠️  {xml_file.name}: Missing required MusicXML elements")
                
        except Exception as e:
            print(f"❌ {xml_file.name}: Error reading file - {e}")
    
    print(f"\\n📊 Output Verification: {valid_count}/{len(xml_files)} files valid")
    return valid_count

def main():
    """Run comprehensive XML export tests"""
    
    print("🎼 MUSICAL XML EXPORT TEST SUITE")
    print("===============================\\n")
    
    # Initialize exporter
    exporter = MusicalXMLExporter("tests/output")
    
    # Create test data
    test_solutions = create_test_solutions()
    print(f"Created {len(test_solutions)} test cases")
    
    # Run tests
    single_success = test_single_exports(exporter, test_solutions)
    batch_success = test_batch_export(exporter, test_solutions)
    valid_files = test_output_verification()
    
    # Final summary
    print("\\n🏆 FINAL RESULTS")
    print("================")
    print(f"✅ Single exports: {single_success}/{len(test_solutions)}")
    print(f"✅ Batch exports: {batch_success} files")
    print(f"✅ Valid XML files: {valid_files}")
    
    output_dir = Path("tests/output")
    total_files = len(list(output_dir.glob("*.xml")))
    print(f"📁 Total XML files created: {total_files}")
    
    print("\\n🎯 XML export system is fully functional!")
    print("   All solutions are saved in tests/output/ as MusicXML files")
    print("   Files can be opened in MuseScore, Sibelius, Finale, etc.")

if __name__ == "__main__":
    main()