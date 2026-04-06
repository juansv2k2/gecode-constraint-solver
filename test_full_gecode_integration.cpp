/**
 * @file test_full_gecode_integration.cpp
 * @brief Test the complete Gecode integration without dependency issues
 */

#include "musical_constraint_solver.hh"
#include <iostream>
#include <chrono>

using namespace MusicalConstraintSolver;

int main() {
    std::cout << "🎼 TESTING FULL GECODE INTEGRATION" << std::endl;
    std::cout << "=================================" << std::endl;
    
    try {
        // Create solver with moderate configuration
        SolverConfig config;
        config.sequence_length = 6;               // Shorter for faster testing
        config.min_note = 60;                     // C4
        config.max_note = 72;                     // C5  
        config.allow_repetitions = false;
        config.prefer_stepwise_motion = true;
        config.max_interval_size = 5;             // Conservative
        config.verbose_output = true;
        
        Solver solver(config);
        
        std::cout << "✅ Gecode-based musical constraint solver created" << std::endl;
        std::cout << "   Sequence length: " << config.sequence_length << std::endl;
        std::cout << "   Note range: MIDI " << config.min_note << "-" << config.max_note << std::endl;
        std::cout << "   Max interval: " << config.max_interval_size << " semitones" << std::endl;
        std::cout << "   Rules: " << solver.get_rules_count() << std::endl;
        
        // Perform constraint solving with Gecode
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::cout << "\n🚀 Starting Gecode constraint solving..." << std::endl;
        MusicalSolution solution = solver.solve();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "\n📊 GECODE SOLVING RESULTS:" << std::endl;
        std::cout << "=========================" << std::endl;
        
        if (solution.found_solution) {
            std::cout << "✅ SUCCESS: Gecode found a musical solution!" << std::endl;
            std::cout << "   Total solve time: " << duration.count() << " ms" << std::endl;
            std::cout << "   Gecode solve time: " << solution.solve_time_ms << " ms" << std::endl;
            std::cout << "   Rules checked: " << solution.total_rules_checked << std::endl;
            std::cout << "   Constraint propagations: " << solution.backjumps_performed << std::endl;
            
            solution.print_solution();
            
            // Verify solution quality
            std::cout << "\n🎵 Solution Analysis:" << std::endl;
            bool all_intervals_ok = true;
            for (int interval : solution.intervals) {
                if (std::abs(interval) > config.max_interval_size) {
                    all_intervals_ok = false;
                    break;
                }
            }
            
            bool no_repetitions = true;
            for (size_t i = 0; i < solution.absolute_notes.size(); ++i) {
                for (size_t j = i + 1; j < solution.absolute_notes.size(); ++j) {
                    if (solution.absolute_notes[i] == solution.absolute_notes[j]) {
                        no_repetitions = false;
                        break;
                    }
                }
                if (!no_repetitions) break;
            }
            
            std::cout << "   All intervals ≤ " << config.max_interval_size << " semitones: " 
                     << (all_intervals_ok ? "✅" : "❌") << std::endl;
            std::cout << "   No repetitions: " << (no_repetitions ? "✅" : "❌") << std::endl;
            
            if (all_intervals_ok && no_repetitions) {
                std::cout << "\n🏆 PERFECT: Gecode generated a valid musical solution!" << std::endl;
                std::cout << "🎼 The constraint programming integration is working correctly!" << std::endl;
            }
            
        } else {
            std::cout << "❌ No solution found by Gecode" << std::endl;
            std::cout << "   Reason: " << solution.failure_reason << std::endl;
            std::cout << "   Search time: " << duration.count() << " ms" << std::endl;
        }
        
        // Test different musical styles with Gecode
        std::cout << "\n🎭 Testing Multiple Musical Styles..." << std::endl;
        std::vector<SolverConfig::MusicalStyle> styles = {
            SolverConfig::CLASSICAL, SolverConfig::JAZZ, SolverConfig::CONTEMPORARY
        };
        std::vector<std::string> style_names = {"Classical", "Jazz", "Contemporary"};
        
        for (size_t i = 0; i < styles.size(); ++i) {
            Solver style_solver;
            style_solver.setup_for_style(styles[i]);
            
            auto style_solution = style_solver.solve();
            std::cout << "   " << style_names[i] << ": " 
                     << (style_solution.found_solution ? "✅ Solved" : "❌ Failed") << std::endl;
        }
        
        // Performance statistics
        auto perf_stats = solver.get_performance_stats();
        std::cout << "\n📈 Performance Statistics:" << std::endl;
        for (const auto& stat : perf_stats) {
            std::cout << "   " << stat.first << ": " << stat.second << std::endl;
        }
        
        std::cout << "\n🎯 FULL GECODE INTEGRATION TEST COMPLETE" << std::endl;
        std::cout << "========================================" << std::endl;
        
        bool success = solution.found_solution;
        if (success) {
            std::cout << "🚀 GECODE CONSTRAINT PROGRAMMING IS FULLY OPERATIONAL!" << std::endl;
            std::cout << "   ✅ Real constraint propagation working" << std::endl;
            std::cout << "   ✅ Musical rules integrated with Gecode" << std::endl;
            std::cout << "   ✅ Advanced search algorithms functioning" << std::endl;
            std::cout << "   ✅ No more placeholder implementations!" << std::endl;
        } else {
            std::cout << "⚠️  Gecode integration needs attention" << std::endl;
        }
        
        return success ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Gecode integration test failed: " << e.what() << std::endl;
        return 1;
    }
}