/**
 * @file test_solver_solve_only.cpp
 * @brief Test solver.solve() directly
 */

#include "musical_constraint_solver.hh"
#include <iostream>

using namespace MusicalConstraintSolver;

int main() {
    std::cout << "🔧 TESTING SOLVER.SOLVE() DIRECTLY" << std::endl;
    std::cout << "==================================" << std::endl;
    
    try {
        std::cout << "📝 Creating solver..." << std::endl;
        SolverConfig config;
        config.sequence_length = 3;
        config.min_note = 60;
        config.max_note = 64;
        config.num_voices = 1;
        config.style = SolverConfig::MINIMAL;
        config.backjump_mode = AdvancedBackjumping::BackjumpMode::NO_BACKJUMPING;
        config.allow_repetitions = true;
        config.verbose_output = false;
        
        Solver solver(config);
        std::cout << "✅ Solver created with " << solver.get_rules_count() << " rules" << std::endl;
        
        std::cout << "📝 Calling solver.solve()..." << std::endl;
        MusicalSolution solution = solver.solve();
        std::cout << "✅ solver.solve() completed!" << std::endl;
        
        std::cout << "Found solution: " << (solution.found_solution ? "YES" : "NO") << std::endl;
        if (solution.found_solution) {
            std::cout << "Notes: ";
            for (int note : solution.absolute_notes) {
                std::cout << note << " ";
            }
            std::cout << std::endl;
        } else {
            std::cout << "Reason: " << solution.failure_reason << std::endl;
        }
        
        std::cout << "✅ SOLVER.SOLVE() TEST COMPLETE - NO CRASHES!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown exception" << std::endl;
        return 1;
    }
}