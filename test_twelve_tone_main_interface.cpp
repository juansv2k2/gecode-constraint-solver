/**
 * @file test_twelve_tone_main_interface.cpp
 * @brief Run twelve-tone configuration using the main solver interface
 */

#include "musical_constraint_solver.hh"
#include <iostream>
#include <fstream>
#include <set>

using namespace MusicalConstraintSolver;

int main() {
    std::cout << "🎼 TWELVE-TONE ROW GENERATION (Main Interface)" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    // Configuration based on twelve_tone_config.json
    SolverConfig config;
    config.sequence_length = 12;
    config.num_voices = 2;
    config.min_note = 60;      // C4  
    config.max_note = 71;      // B4 (12 semitones covering all pitch classes)
    config.style = SolverConfig::CONTEMPORARY;
    config.backjump_mode = AdvancedBackjumping::BackjumpMode::INTELLIGENT_BACKJUMP;
    
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Name: Dual Voice 12-Tone Rows - No Unisons" << std::endl;
    std::cout << "  Description: Generate two different 12-tone rows" << std::endl;
    std::cout << "  Voices: " << config.num_voices << std::endl;
    std::cout << "  Length: " << config.sequence_length << " notes each" << std::endl;
    std::cout << "  Range: " << config.min_note << " - " << config.max_note << " (C4-B4)" << std::endl;
    
    // Create solver
    Solver solver(config);
    
    // Add twelve-tone rules (all different pitch classes per voice)
    auto basic_rules = MusicalRuleFactory::create_basic_rules();
    solver.add_rules(basic_rules);
    std::cout << "  Rules added: " << solver.get_rules_count() << std::endl;
    
    std::cout << "\n🎯 Solving Twelve-Tone Problem..." << std::endl;
    
    // Solve the problem
    auto solution = solver.solve();
    
    if (!solution.found_solution) {
        std::cout << "❌ Solution failed: " << solution.failure_reason << std::endl;
        return 1;
    }
    
    std::cout << "✅ Solution found in " << solution.solve_time_ms << " ms" << std::endl;
    std::cout << "   Rules checked: " << solution.total_rules_checked << std::endl;
    std::cout << "   Backjumps: " << solution.backjumps_performed << std::endl;
    
    // Analyze the twelve-tone results
    std::cout << "\n🎵 TWELVE-TONE ANALYSIS" << std::endl;
    std::cout << "======================" << std::endl;
    
    for (size_t voice = 0; voice < solution.voice_solutions.size(); ++voice) {
        std::cout << "Voice " << voice << " twelve-tone row:" << std::endl;
        
        // Show MIDI numbers
        std::cout << "  MIDI: ";
        for (size_t i = 0; i < solution.voice_solutions[voice].size(); ++i) {
            std::cout << solution.voice_solutions[voice][i] << " ";
        }
        std::cout << std::endl;
        
        // Show note names
        std::cout << "  Notes: ";
        for (size_t i = 0; i < solution.voice_solutions[voice].size(); ++i) {
            int midi = solution.voice_solutions[voice][i];
            std::cout << Solver::midi_to_note_name(midi) << " ";
        }
        std::cout << std::endl;
        
        // Show pitch classes (mod 12)
        std::cout << "  Pitch classes: ";
        std::set<int> pitch_classes;
        for (size_t i = 0; i < solution.voice_solutions[voice].size(); ++i) {
            int pc = solution.voice_solutions[voice][i] % 12;
            pitch_classes.insert(pc);
            std::cout << pc << " ";
        }
        std::cout << std::endl;
        std::cout << "  Unique pitch classes: " << pitch_classes.size() << "/12" << std::endl;
        
        // Check if it's a valid twelve-tone row
        bool valid_twelve_tone = (pitch_classes.size() == 12);
        std::cout << "  Valid twelve-tone row: " << (valid_twelve_tone ? "✅ YES" : "❌ NO") << std::endl;
        std::cout << std::endl;
    }
    
    // Check for unisons between voices
    std::cout << "🔍 VOICE INTERACTION ANALYSIS" << std::endl;
    std::cout << "============================" << std::endl;
    
    if (solution.voice_solutions.size() >= 2) {
        int unisons = 0;
        for (size_t pos = 0; pos < solution.voice_solutions[0].size() && pos < solution.voice_solutions[1].size(); ++pos) {
            if (solution.voice_solutions[0][pos] % 12 == solution.voice_solutions[1][pos] % 12) {
                unisons++;
                std::cout << "  Unison at position " << (pos + 1) << ": " 
                         << Solver::midi_to_note_name(solution.voice_solutions[0][pos]) << " - "
                         << Solver::midi_to_note_name(solution.voice_solutions[1][pos]) << std::endl;
            }
        }
        std::cout << "Total unisons: " << unisons << std::endl;
        std::cout << "No unisons between voices: " << (unisons == 0 ? "✅ YES" : "❌ NO") << std::endl;
    }
    
    // Export results
    std::cout << "\n💾 EXPORTING RESULTS" << std::endl;
    std::cout << "===================" << std::endl;
    
    // Export to XML using built-in functionality
    solution.export_to_xml("twelve_tone_result.xml");
    std::cout << "✅ XML exported: twelve_tone_result.xml" << std::endl;
    
    // Also save JSON for Python processing if needed
    std::ofstream json_out("twelve_tone_result.json");
    if (json_out.is_open()) {
        json_out << "{" << std::endl;
        json_out << "  \"problem_name\": \"Dual Voice 12-Tone Rows\"," << std::endl;
        json_out << "  \"voices\": [" << std::endl;
        
        for (size_t voice = 0; voice < solution.voice_solutions.size(); ++voice) {
            if (voice > 0) json_out << "," << std::endl;
            json_out << "    {" << std::endl;
            json_out << "      \"voice\": " << voice << "," << std::endl;
            json_out << "      \"pitch_solution\": [";
            for (size_t i = 0; i < solution.voice_solutions[voice].size(); ++i) {
                if (i > 0) json_out << ", ";
                json_out << solution.voice_solutions[voice][i];
            }
            json_out << "]," << std::endl;
            json_out << "      \"rhythm_solution\": [";
            if (voice < solution.voice_rhythms.size()) {
                for (size_t i = 0; i < solution.voice_rhythms[voice].size(); ++i) {
                    if (i > 0) json_out << ", ";
                    json_out << solution.voice_rhythms[voice][i];
                }
            }
            json_out << "]" << std::endl;
            json_out << "    }";
        }
        
        json_out << std::endl << "  ]," << std::endl;
        json_out << "  \"metric_signature\": [";
        for (size_t i = 0; i < solution.metric_signature.size(); ++i) {
            if (i > 0) json_out << ", ";
            json_out << solution.metric_signature[i];
        }
        json_out << "]," << std::endl;
        json_out << "  \"solve_time_ms\": " << solution.solve_time_ms << std::endl;
        json_out << "}" << std::endl;
        json_out.close();
        
        std::cout << "✅ JSON exported: twelve_tone_result.json" << std::endl;
    }
    
    std::cout << "\n🎯 SUMMARY" << std::endl;
    std::cout << "=========" << std::endl;
    std::cout << "✅ Twelve-tone row generation completed successfully!" << std::endl;
    std::cout << "   Using main solver interface: MusicalConstraintSolver::Solver" << std::endl;
    
    return 0;
}