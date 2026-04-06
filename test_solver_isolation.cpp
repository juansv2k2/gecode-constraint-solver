/**
 * @file test_solver_isolation.cpp
 * @brief Test Solver creation without complex operations
 */

#include "musical_constraint_solver.hh"
#include <iostream>

using namespace MusicalConstraintSolver;

int main() {
    std::cout << "🔧 TESTING SOLVER IN ISOLATION" << std::endl;
    std::cout << "==============================" << std::endl;
    
    try {
        std::cout << "📝 Creating simple config..." << std::endl;
        SolverConfig config;
        config.sequence_length = 3;
        config.min_note = 60;
        config.max_note = 64;
        config.num_voices = 1;                         // Simple single voice
        config.backjump_mode = AdvancedBackjumping::BackjumpMode::NO_BACKJUMPING; // Simplest
        config.allow_repetitions = true;
        config.verbose_output = false;                 // Reduce noise
        
        std::cout << "✅ Config created" << std::endl;
        
        std::cout << "📝 Creating Solver with config..." << std::endl;
        Solver* solver = new Solver(config);
        
        std::cout << "✅ Solver created successfully" << std::endl;
        std::cout << "   Rules count: " << solver->get_rules_count() << std::endl;
        
        std::cout << "📝 Clearing rules to avoid rule initialization issues..." << std::endl;
        solver->clear_rules();
        std::cout << "✅ Rules cleared" << std::endl;
        
        std::cout << "📝 Adding simple manual rule..." << std::endl;
        // Don't add complex rules, keep it simple
        std::cout << "✅ Simple setup complete" << std::endl;
        
        std::cout << "📝 Testing basic solve (should find solution or fail gracefully)..." << std::endl;
        MusicalSolution solution = solver->solve();
        
        std::cout << "✅ Solve completed!" << std::endl;
        std::cout << "   Found solution: " << (solution.found_solution ? "YES" : "NO") << std::endl;
        
        if (solution.found_solution) {
            std::cout << "   Notes: ";
            for (int note : solution.absolute_notes) {
                std::cout << note << " ";
            }
            std::cout << std::endl;
        } else {
            std::cout << "   Failure reason: " << solution.failure_reason << std::endl;
        }
        
        delete solver;
        std::cout << "✅ SOLVER ISOLATION TEST COMPLETE - NO CRASHES!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown exception" << std::endl;
        return 1;
    }
}