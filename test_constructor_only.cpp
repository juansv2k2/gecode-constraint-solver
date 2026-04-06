/**
 * @file test_constructor_only.cpp
 * @brief Test just constructor creation to isolate crash
 */

#include "musical_constraint_solver.hh"
#include <iostream>

using namespace MusicalConstraintSolver;

int main() {
    std::cout << "🔧 CONSTRUCTOR ONLY TEST" << std::endl;
    std::cout << "========================" << std::endl;
    
    try {
        std::cout << "📝 Step 1: Creating SolverConfig..." << std::endl;
        SolverConfig config;
        
        std::cout << "📝 Step 2: Setting minimal config..." << std::endl; 
        config.sequence_length = 3;
        config.min_note = 60;
        config.max_note = 64;
        config.num_voices = 1;
        config.style = SolverConfig::MINIMAL;
        config.backjump_mode = AdvancedBackjumping::BackjumpMode::NO_BACKJUMPING;
        
        std::cout << "✅ Config setup complete" << std::endl;
        
        std::cout << "📝 Step 3: Creating Solver..." << std::endl;
        Solver* solver = nullptr;
        solver = new Solver(config);
        
        std::cout << "✅ Solver constructor completed!" << std::endl;
        
        std::cout << "📝 Step 4: Getting rules count..." << std::endl;
        int rules = solver->get_rules_count();
        std::cout << "✅ Rules count: " << rules << std::endl;
        
        std::cout << "📝 Step 5: Cleaning up..." << std::endl;
        delete solver;
        std::cout << "✅ CONSTRUCTOR TEST COMPLETE - NO CRASHES!" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown exception" << std::endl;
        return 1;
    }
}