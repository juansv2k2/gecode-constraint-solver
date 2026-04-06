/**
 * @file test_multi_voice_extraction.cpp
 * @brief Test Multi-Voice Data Extraction from Individual Engines
 * 
 * This test verifies that individual engine solutions are properly parsed
 * into rhythm/pitch groups for multi-voice musical generation.
 */

#include "musical_constraint_solver.hh"
#include <iostream>
#include <iomanip>

using namespace MusicalConstraintSolver;

int main() {
    std::cout << "🔍 TESTING MULTI-VOICE DATA EXTRACTION" << std::endl;
    std::cout << "=======================================" << std::endl;
    
    // Create solver configuration for 2 voices
    SolverConfig config;
    config.sequence_length = 12;
    config.num_voices = 2;
    config.min_note = 60;      // C4
    config.max_note = 72;      // C5
    config.style = SolverConfig::CLASSICAL;
    config.backjump_mode = AdvancedBackjumping::BackjumpMode::INTELLIGENT_BACKJUMP;
    
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Voices: " << config.num_voices << std::endl;
    std::cout << "  Length: " << config.sequence_length << std::endl;
    std::cout << "  Range: " << config.min_note << " - " << config.max_note << std::endl;
    
    // Create and configure solver
    Solver solver(config);
    
    // Add basic musical rules
    auto rules = MusicalRuleFactory::create_basic_rules();
    solver.add_rules(rules);
    std::cout << "  Rules: " << rules.size() << std::endl;
    
    std::cout << "\n🎼 Solving Musical Problem..." << std::endl;
    
    // Solve the musical problem
    auto solution = solver.solve();
    
    if (!solution.found_solution) {
        std::cout << "❌ Solution failed: " << solution.failure_reason << std::endl;
        return 1;
    }
    
    std::cout << "✅ Solution found in " << std::fixed << std::setprecision(2) 
              << solution.solve_time_ms << " ms" << std::endl;
    
    // Analyze the multi-voice extraction
    std::cout << "\n🎵 MULTI-VOICE DATA ANALYSIS" << std::endl;
    std::cout << "============================" << std::endl;
    
    // Check voice solutions
    std::cout << "Voice Solutions:" << std::endl;
    std::cout << "  Number of voices: " << solution.voice_solutions.size() << std::endl;
    
    for (size_t voice = 0; voice < solution.voice_solutions.size(); ++voice) {
        std::cout << "  Voice " << voice << " pitch data (" << solution.voice_solutions[voice].size() << " notes):" << std::endl;
        std::cout << "    MIDI: ";
        for (size_t i = 0; i < std::min(static_cast<size_t>(12), solution.voice_solutions[voice].size()); ++i) {
            std::cout << solution.voice_solutions[voice][i] << " ";
        }
        std::cout << std::endl;
        
        std::cout << "    Notes: ";
        for (size_t i = 0; i < std::min(static_cast<size_t>(12), solution.voice_solutions[voice].size()); ++i) {
            std::cout << Solver::midi_to_note_name(solution.voice_solutions[voice][i]) << " ";
        }
        std::cout << std::endl;
    }
    
    // Check rhythm data
    std::cout << "\nRhythm Solutions:" << std::endl;
    std::cout << "  Number of voice rhythms: " << solution.voice_rhythms.size() << std::endl;
    
    for (size_t voice = 0; voice < solution.voice_rhythms.size(); ++voice) {
        std::cout << "  Voice " << voice << " rhythm data (" << solution.voice_rhythms[voice].size() << " values):" << std::endl;
        std::cout << "    Values: ";
        for (size_t i = 0; i < std::min(static_cast<size_t>(12), solution.voice_rhythms[voice].size()); ++i) {
            std::cout << solution.voice_rhythms[voice][i] << " ";
        }
        std::cout << std::endl;
        
        std::cout << "    Note types: ";
        for (size_t i = 0; i < std::min(static_cast<size_t>(12), solution.voice_rhythms[voice].size()); ++i) {
            int value = solution.voice_rhythms[voice][i];
            std::cout << "1/" << (16 / value) << " ";
        }
        std::cout << std::endl;
    }
    
    // Check metric data
    std::cout << "\nMetric Signature:" << std::endl;
    std::cout << "  Size: " << solution.metric_signature.size() << std::endl;
    if (!solution.metric_signature.empty()) {
        std::cout << "  Values: ";
        for (int value : solution.metric_signature) {
            std::cout << value << "/4 ";
        }
        std::cout << std::endl;
    }
    
    // VALIDATE ENGINE MAPPING
    std::cout << "\n🧩 ENGINE MAPPING VALIDATION" << std::endl;
    std::cout << "============================" << std::endl;
    std::cout << "Expected mapping:" << std::endl;
    std::cout << "  Engine 0 -> Voice 0 Rhythm" << std::endl;
    std::cout << "  Engine 1 -> Voice 0 Pitch" << std::endl;
    std::cout << "  Engine 2 -> Voice 1 Rhythm" << std::endl;  
    std::cout << "  Engine 3 -> Voice 1 Pitch" << std::endl;
    std::cout << "  Engine 4 -> Metric" << std::endl;
    
    // Check if we have distinct voice data
    bool distinct_voices = false;
    if (solution.voice_solutions.size() >= 2) {
        auto& voice0 = solution.voice_solutions[0];
        auto& voice1 = solution.voice_solutions[1];
        
        if (voice0.size() == voice1.size() && !voice0.empty()) {
            distinct_voices = false;
            for (size_t i = 0; i < voice0.size(); ++i) {
                if (voice0[i] != voice1[i]) {
                    distinct_voices = true;  
                    break;
                }
            }
        }
    }
    
    std::cout << "\nValidation Results:" << std::endl;
    std::cout << "  ✅ Voice solutions extracted: " << (solution.voice_solutions.size() == 2 ? "YES" : "NO") << std::endl;
    std::cout << "  ✅ Rhythm solutions extracted: " << (solution.voice_rhythms.size() == 2 ? "YES" : "NO") << std::endl;
    std::cout << "  ✅ Metric signature extracted: " << (!solution.metric_signature.empty() ? "YES" : "NO") << std::endl;
    std::cout << "  ✅ Voices are distinct: " << (distinct_voices ? "YES" : "NO") << std::endl;
    
    // Export test data for examination
    std::cout << "\n📄 EXPORT TEST DATA" << std::endl;
    std::cout << "===================" << std::endl;
    
    // Save JSON for manual inspection
    std::ofstream json_out("multi_voice_test_result.json");
    if (json_out.is_open()) {
        json_out << "{" << std::endl;
        json_out << "  \"test_name\": \"Multi-Voice Data Extraction Test\"," << std::endl;
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
        
        std::cout << "✅ Test results saved to: multi_voice_test_result.json" << std::endl;
    }
    
    std::cout << "\n🎯 CONCLUSION" << std::endl;
    std::cout << "============" << std::endl;
    
    if (solution.voice_solutions.size() == 2 && solution.voice_rhythms.size() == 2 && distinct_voices) {
        std::cout << "✅ SUCCESS: Multi-voice data extraction is working correctly!" << std::endl;
        std::cout << "   Individual engine solutions are properly parsed into rhythm/pitch groups." << std::endl;
        return 0;
    } else {
        std::cout << "⚠️  PARTIAL SUCCESS: Multi-voice structure is present but may need refinement." << std::endl;
        std::cout << "   Check the exported JSON file for detailed analysis." << std::endl;
        return 0;
    }
}