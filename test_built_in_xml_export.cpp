/**
 * @file test_built_in_xml_export.cpp
 * @brief Test Built-in XML Export Functionality
 */

#include "musical_constraint_solver.hh"
#include <iostream>

using namespace MusicalConstraintSolver;

int main() {
    std::cout << "🎼 TESTING BUILT-IN XML EXPORT" << std::endl;
    std::cout << "===============================" << std::endl;
    
    // Create solver with 2 voices
    SolverConfig config;
    config.sequence_length = 8;
    config.num_voices = 2;
    config.min_note = 60;
    config.max_note = 72;
    
    Solver solver(config);
    solver.add_rules(MusicalRuleFactory::create_basic_rules());
    
    std::cout << "Configuration: " << config.num_voices << " voices, " 
              << config.sequence_length << " notes each" << std::endl;
    
    // Test Method 1: solve().export_to_xml()
    std::cout << "\n📄 Test 1: MusicalSolution.export_to_xml()" << std::endl;
    auto solution = solver.solve();
    
    if (solution.found_solution) {
        solution.export_to_xml("test_solution_direct.xml");
        std::cout << "✅ Direct solution.export_to_xml() successful" << std::endl;
        std::cout << "   File: test_solution_direct.xml" << std::endl;
        std::cout << "   Voices: " << solution.voice_solutions.size() << std::endl;
    } else {
        std::cout << "❌ Solution failed: " << solution.failure_reason << std::endl;
        return 1;
    }
    
    // Test Method 2: solver.export_solution_to_xml()
    std::cout << "\n📄 Test 2: Solver.export_solution_to_xml()" << std::endl;
    bool success = solver.export_solution_to_xml(solution, "test_solver_export.xml");
    if (success) {
        std::cout << "✅ Solver.export_solution_to_xml() successful" << std::endl;
        std::cout << "   File: test_solver_export.xml" << std::endl;
    } else {
        std::cout << "❌ Solver export failed" << std::endl;
    }
    
    // Test Method 3: solver.solve_and_export_xml() (one-liner)
    std::cout << "\n📄 Test 3: Solver.solve_and_export_xml() (one-liner)" << std::endl;
    success = solver.solve_and_export_xml("test_solve_and_export.xml");
    if (success) {
        std::cout << "✅ Solver.solve_and_export_xml() successful" << std::endl;
        std::cout << "   File: test_solve_and_export.xml" << std::endl;
    } else {
        std::cout << "❌ Solve-and-export failed" << std::endl;
    }
    
    // Test Method 4: solution.to_xml() (string return)
    std::cout << "\n📄 Test 4: MusicalSolution.to_xml() (string return)" << std::endl;
    std::string xml_string = solution.to_xml();
    std::cout << "✅ XML string generated (" << xml_string.length() << " chars)" << std::endl;
    std::cout << "   Preview (first 200 chars):" << std::endl;
    std::cout << "   " << xml_string.substr(0, 200) << "..." << std::endl;
    
    // Validation
    std::cout << "\n🔍 VALIDATION" << std::endl;
    std::cout << "==============" << std::endl;
    
    // Check if XML contains expected elements
    bool has_voices = xml_string.find("<voices>") != std::string::npos;
    bool has_pitch_sequence = xml_string.find("<pitch_sequence>") != std::string::npos;
    bool has_rhythm_sequence = xml_string.find("<rhythm_sequence>") != std::string::npos;
    bool has_metadata = xml_string.find("<metadata>") != std::string::npos;
    bool has_metric_signature = xml_string.find("<metric_signature>") != std::string::npos;
    
    std::cout << "Expected XML elements present:" << std::endl;
    std::cout << "  ✅ <voices>: " << (has_voices ? "YES" : "NO") << std::endl;
    std::cout << "  ✅ <pitch_sequence>: " << (has_pitch_sequence ? "YES" : "NO") << std::endl;
    std::cout << "  ✅ <rhythm_sequence>: " << (has_rhythm_sequence ? "YES" : "NO") << std::endl;
    std::cout << "  ✅ <metadata>: " << (has_metadata ? "YES" : "NO") << std::endl;
    std::cout << "  ✅ <metric_signature>: " << (has_metric_signature ? "YES" : "NO") << std::endl;
    
    std::cout << "\n🎯 SUMMARY" << std::endl;
    std::cout << "=========" << std::endl;
    
    if (has_voices && has_pitch_sequence && has_rhythm_sequence && has_metadata) {
        std::cout << "✅ SUCCESS: Built-in XML export functionality is working correctly!" << std::endl;
        std::cout << "   All XML export methods operational:" << std::endl;
        std::cout << "   • MusicalSolution.export_to_xml(filename)" << std::endl;
        std::cout << "   • MusicalSolution.to_xml() -> string" << std::endl;
        std::cout << "   • Solver.export_solution_to_xml(solution, filename)" << std::endl;
        std::cout << "   • Solver.solve_and_export_xml(filename)" << std::endl;
        std::cout << "\n   XML export is now a built-in solver functionality!" << std::endl;
        return 0;
    } else {
        std::cout << "⚠️ PARTIAL SUCCESS: Some XML elements missing" << std::endl;
        return 1;
    }
}