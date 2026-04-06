/**
 * @file phase5_demo.cpp
 * @brief Phase 5 Demo - Professional Musical AI Studio Workflow
 */

#include <iostream>
#include <string>

#include "professional_workflow.hh"
#include "export_import_system.hh"
#include "advanced_configuration.hh"

using namespace MusicalConstraints;

int main() {
    std::cout << "\n🎼 Professional Musical AI Studio Workflow Demo\n";
    std::cout << "================================================\n";

    // Professional Setup
    MasterConfigurationManager config;
    WorkflowManager workflow;
    
    // Create a musical project
    std::cout << "\n🏢 Creating Professional Studio Project...\n";
    workflow.create_project("Symphonic Composition", "AI Composer");

    // Configure for symphonic work
    std::cout << "\n🎛️ Configuring for Classical Music...\n";
    config.presets().apply_preset("Classical Composition", config.performance());
    
    // Demonstrate batch processing capability
    std::cout << "\n⚙️ Setting up Batch Processing...\n";
    BatchProcessor processor;
    processor.process_all();

    // Create musical compositions for export
    std::cout << "\n🎵 Creating Musical Compositions...\n";
    
    ExportComposition movement1("Symphony No.1 - Movement I", "AI Composer");
    // Add a simple C major scale
    movement1.add_note(ExportNote(60, 0.0, 0.5));  // C
    movement1.add_note(ExportNote(62, 0.5, 0.5));  // D
    movement1.add_note(ExportNote(64, 1.0, 0.5));  // E
    movement1.add_note(ExportNote(65, 1.5, 0.5));  // F
    movement1.add_note(ExportNote(67, 2.0, 0.5));  // G
    movement1.add_note(ExportNote(69, 2.5, 0.5));  // A
    movement1.add_note(ExportNote(71, 3.0, 0.5));  // B
    movement1.add_note(ExportNote(72, 3.5, 1.0));  // C

    ExportComposition movement2("Symphony No.1 - Movement II", "AI Composer");
    // Add a harmony progression (C major triad)
    movement2.add_note(ExportNote(60, 0.0, 2.0));  // C
    movement2.add_note(ExportNote(64, 0.0, 2.0));  // E
    movement2.add_note(ExportNote(67, 0.0, 2.0));  // G
    
    std::cout << "   Movement I: " << movement1.notes.size() << " notes\n";
    std::cout << "   Movement II: " << movement2.notes.size() << " notes\n";

    // Professional Export System
    std::cout << "\n📁 Professional Export System...\n";
    MidiExporter midi_exporter;
    MusicXMLExporter xml_exporter;
    AnalysisExporter analyzer;
    
    // Export in multiple formats
    midi_exporter.export_midi(movement1, "symphony_mv1.mid");
    xml_exporter.export_musicxml(movement1, "symphony_mv1.xml");
    midi_exporter.export_midi(movement2, "symphony_mv2.mid");
    xml_exporter.export_musicxml(movement2, "symphony_mv2.xml");

    // Advanced analysis export
    std::cout << "\n📊 Advanced Musical Analysis...\n";
    analyzer.export_analysis(movement1, "mv1_analysis.json");

    // Collaboration features
    std::cout << "\n🤝 Collaboration Features...\n";
    CollaborationManager collab("AI Composer");
    collab.add_collaborator("Studio Engineer");
    collab.record_change("Symphony No.1", "Created symphonic movements with AI assistance");
    collab.show_history("Symphony No.1");
    
    // Final summary
    std::cout << "\n✨ Professional Studio Workflow Complete!\n";
    std::cout << "=========================================\n";
    std::cout << "🎼 Created: Symphony with 2 movements\n";
    std::cout << "📁 Exported: MIDI and MusicXML formats\n";
    std::cout << "📊 Analysis: Comprehensive musical analysis\n";
    std::cout << "🤝 Collaboration: Team management with change tracking\n";
    std::cout << "⚙️ Batch Processing: Enhanced workflow automation\n";
    std::cout << "🎛️ Configuration: Classical composition preset applied\n";
    
    std::cout << "\n🚀 Ready for professional music production!\n";

    return 0;
}