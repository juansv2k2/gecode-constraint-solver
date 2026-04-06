/**
 * @file test_solve_step_by_step.cpp
 * @brief Step by step test of solve_internal to find crash location
 */

#include "musical_constraint_solver.hh"
#include "gecode_cluster_integration.hh"
#include <iostream>

using namespace MusicalConstraintSolver;

int main() {
    std::cout << "🔧 STEP-BY-STEP SOLVE TEST" << std::endl;
    std::cout << "==========================" << std::endl;
    
    try {
        std::cout << "📝 Step 1: Creating Solver..." << std::endl;
        SolverConfig config;
        config.sequence_length = 3;
        config.min_note = 60;
        config.max_note = 64;
        config.num_voices = 1;
        config.style = SolverConfig::MINIMAL;
        config.backjump_mode = AdvancedBackjumping::BackjumpMode::NO_BACKJUMPING;
        
        Solver solver(config);
        std::cout << "✅ Solver created with " << solver.get_rules_count() << " rules" << std::endl;
        
        std::cout << "📝 Step 2: Creating IntegratedMusicalSpace directly..." << std::endl;
        auto gecode_space = std::make_unique<GecodeClusterIntegration::IntegratedMusicalSpace>(
            config.sequence_length, config.num_voices, config.backjump_mode);
        std::cout << "✅ IntegratedMusicalSpace created" << std::endl;
        
        std::cout << "📝 Step 3: Adding note range constraints..." << std::endl;
        gecode_space->constrain_note_range(config.min_note, config.max_note);
        std::cout << "✅ Note range constraints added" << std::endl;
        
        std::cout << "📝 Step 4: Setting up search options..." << std::endl;
        Gecode::Search::Options search_opts;
        search_opts.threads = 1;
        search_opts.nogoods_limit = 128;
        std::cout << "✅ Search options configured" << std::endl;
        
        std::cout << "📝 Step 5: Creating DFS search engine..." << std::endl;
        auto raw_space = gecode_space.release();
        std::cout << "   Raw space pointer: " << raw_space << std::endl;
        
        Gecode::DFS<GecodeClusterIntegration::IntegratedMusicalSpace> search_engine(raw_space, search_opts);
        std::cout << "✅ Search engine created" << std::endl;
        
        std::cout << "📝 Step 6: Calling search_engine.next()..." << std::endl;
        auto solved_space = search_engine.next();
        std::cout << "✅ Search completed" << std::endl;
        
        if (solved_space) {
            std::cout << "📝 Step 7: Extracting solution..." << std::endl;
            auto sequence = solved_space->get_absolute_sequence();
            std::cout << "✅ Solution extracted: ";
            for (int note : sequence) {
                std::cout << note << " ";
            }
            std::cout << std::endl;
            delete solved_space;
        } else {
            std::cout << "⚠️  No solution found" << std::endl;
        }
        
        std::cout << "✅ STEP-BY-STEP TEST COMPLETE - NO CRASHES!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown exception" << std::endl;
        return 1;
    }
}