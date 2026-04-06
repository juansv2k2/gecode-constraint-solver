/**
 * @file test_minimal_musical.cpp
 * @brief Absolute minimal musical constraint test
 */

#include "musical_constraint_solver.hh"
#include <iostream>

using namespace MusicalConstraintSolver;

int main() {
    std::cout << "🔧 MINIMAL MUSICAL CONSTRAINT TEST" << std::endl;
    std::cout << "==================================" << std::endl;
    
    try {
        // Minimal configuration
        std::cout << "📝 Creating minimal config..." << std::endl;
        SolverConfig config;
        config.sequence_length = 3;
        config.min_note = 60;
        config.max_note = 64;
        config.num_voices = 1;
        config.allow_repetitions = true;      // Allow everything
        config.verbose_output = false;
        config.style = SolverConfig::MINIMAL; // Use minimal style
        config.backjump_mode = AdvancedBackjumping::BackjumpMode::NO_BACKJUMPING;
        
        std::cout << "✅ Config created" << std::endl;
        
        std::cout << "📝 Creating Solver..." << std::endl;
        Solver solver(config);
        std::cout << "✅ Solver created, rules: " << solver.get_rules_count() << std::endl;
        
        std::cout << "📝 Solving..." << std::endl;
        MusicalSolution solution = solver.solve();
        std::cout << "✅ Solve completed" << std::endl;
        
        std::cout << "Result: " << (solution.found_solution ? "SUCCESS" : "FAILED") << std::endl;
        if (!solution.found_solution) {
            std::cout << "Reason: " << solution.failure_reason << std::endl;
        } else {
            std::cout << "Notes: ";
            for (int note : solution.absolute_notes) {
                std::cout << note << " ";
            }
            std::cout << std::endl;
        }
        
        std::cout << "✅ MINIMAL TEST COMPLETE!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown exception" << std::endl;
        return 1;
    }
}