/**
 * @file test_simple_gecode.cpp
 * @brief Minimal test to isolate Gecode integration issues
 */

#include "musical_constraint_solver.hh"
#include <iostream>

using namespace MusicalConstraintSolver;

int main() {
    std::cout << "🔧 MINIMAL GECODE INTEGRATION TEST" << std::endl;
    std::cout << "==================================" << std::endl;
    
    try {
        // Test simple solver creation
        std::cout << "📝 Creating simple solver config..." << std::endl;
        
        SolverConfig config;
        config.sequence_length = 3;      // Very short
        config.min_note = 60;
        config.max_note = 64;             // Very small range
        config.allow_repetitions = true;
        config.verbose_output = true;
        
        std::cout << "✅ Config created successfully" << std::endl;
        
        // Test solver instantiation
        std::cout << "📝 Creating Solver instance..." << std::endl;
        Solver* solver = nullptr;
        
        solver = new Solver(config);
        std::cout << "✅ Solver created successfully" << std::endl;
        
        // Test basic properties
        std::cout << "📝 Testing basic properties..." << std::endl;
        int rules_count = solver->get_rules_count();
        std::cout << "   Rules count: " << rules_count << std::endl;
        
        // Test solving
        std::cout << "📝 Attempting basic solve..." << std::endl;
        MusicalSolution solution = solver->solve();
        
        std::cout << "✅ Solve completed" << std::endl;
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
        std::cout << "✅ MINIMAL TEST COMPLETE - NO CRASHES!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception caught: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown exception caught" << std::endl;
        return 1;
    }
}