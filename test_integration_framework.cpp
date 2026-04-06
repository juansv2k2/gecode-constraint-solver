/**
 * @file test_rules_integration.cpp
 * @brief Test ClusterEngine Integration Framework
 * 
 * This test verifies that the ClusterEngine system works correctly
 * and demonstrates the integration framework for musical constraint rules.
 */

#include "include/cluster_engine_interface.hh"
#include <iostream>
#include <chrono>
#include <iomanip>

using namespace ClusterEngine;

int main() {
    std::cout << "🎵 Testing ClusterEngine Integration Framework\n";
    std::cout << "==============================================\n\n";

    try {
        // ===== Test 1: Basic Setup =====
        std::cout << "📋 Test 1: Basic ClusterEngine Setup\n";
        
        ClusterEngineInterface engine;
        
        // Configure domain
        DomainConfiguration domain_config = MusicalDomainFactory::create_classical_domains(
            2,      // 2 voices
            60,     // C4 (Middle C)
            72      // C5
        );
        
        // Configure search
        SearchConfig search_config;
        search_config.max_solutions = 3;
        search_config.max_search_loops = 1000;
        search_config.verbose_output = true;
        search_config.debug_output = false;
        
        // Initialize engine
        bool initialized = engine.initialize(domain_config, search_config);
        if (!initialized) {
            std::cerr << "❌ Failed to initialize ClusterEngine\n";
            return 1;
        }
        std::cout << "✅ ClusterEngine initialized successfully\n";
        
        // ===== Test 2: Add Heuristic Rules =====
        std::cout << "\n🧠 Test 2: Adding Heuristic Rules\n";
        
        // Add stepwise motion preference
        auto stepwise_heuristic = MusicalHeuristicLibrary::create_stepwise_motion_preference(0, 2.0);
        engine.add_heuristic_rule(stepwise_heuristic);
        std::cout << "✅ Added stepwise motion heuristic for voice 0\n";
        
        // Add consonance preference
        auto consonance_heuristic = MusicalHeuristicLibrary::create_consonance_preference({0, 1}, 1.5);
        engine.add_heuristic_rule(consonance_heuristic);
        std::cout << "✅ Added consonance preference heuristic\n";
        
        // ===== Test 3: Configuration Summary =====
        std::cout << "\n📊 Test 3: Current Configuration\n";
        engine.print_domain_summary();
        engine.print_rule_summary();
        
        // ===== Test 4: Execute Search =====
        std::cout << "\n🔍 Test 4: Executing Search\n";
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<MusicalComposition> compositions = engine.search();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "\n🎼 Search Results:\n";
        std::cout << "   Compositions found: " << compositions.size() << "\n";
        std::cout << "   Search time: " << duration.count() << " ms\n";
        
        // ===== Test 5: Analyze Results =====
        std::cout << "\n🔬 Test 5: Analyzing Generated Compositions\n";
        
        for (size_t i = 0; i < compositions.size(); ++i) {
            const auto& comp = compositions[i];
            std::cout << "\n  Composition " << (i + 1) << ":\n";
            std::cout << "    Voices: " << comp.get_num_voices() << "\n";
            std::cout << "    Total notes: " << comp.total_notes << "\n";
            std::cout << "    Duration: " << comp.total_duration << " beats\n";
            
            // Display note sequences
            for (int voice = 0; voice < comp.get_num_voices() && voice < 2; ++voice) {
                std::cout << "    Voice " << voice << " pitches: ";
                if (voice < static_cast<int>(comp.absolute_pitches.size())) {
                    for (size_t j = 0; j < comp.absolute_pitches[voice].size() && j < 8; ++j) {
                        std::cout << comp.absolute_pitches[voice][j] << " ";
                    }
                }
                std::cout << "\n";
            }
        }
        
        // ===== Test 6: Search Statistics =====
        std::cout << "\n📈 Test 6: Search Statistics\n";
        const auto& stats = engine.get_search_statistics();
        
        std::cout << "   Total search steps: " << stats.total_search_steps << "\n";
        std::cout << "   Rule tests passed: " << stats.rule_tests_passed << "\n";
        std::cout << "   Rule tests failed: " << stats.rule_tests_failed << "\n";
        std::cout << "   Backjump operations: " << stats.backjump_count << "\n";
        std::cout << "   Success rate: " << std::fixed << std::setprecision(1) 
                  << (stats.get_success_rate() * 100) << "%\n";
        
        // ===== Test 7: Framework Validation =====
        std::cout << "\n✅ Test 7: Integration Framework Validation\n";
        
        bool framework_ready = true;
        
        // Check that we got compositions
        if (compositions.empty()) {
            std::cout << "❌ No compositions generated\n";
            framework_ready = false;
        } else {
            std::cout << "✅ Compositions successfully generated\n";
        }
        
        // Check search performance
        if (stats.total_search_steps > 0) {
            std::cout << "✅ Search loop operational\n";
        } else {
            std::cout << "❌ Search loop not functioning\n";
            framework_ready = false;
        }
        
        // ===== Final Results =====
        std::cout << "\n🏆 INTEGRATION TEST RESULTS\n";
        std::cout << "============================\n";
        std::cout << "✅ ClusterEngine Core: OPERATIONAL\n";
        std::cout << "✅ Search System: " << (framework_ready ? "FUNCTIONAL" : "NEEDS_WORK") << "\n"; 
        std::cout << "✅ Heuristic System: INTEGRATED\n";
        std::cout << "✅ Composition Generation: " << (!compositions.empty() ? "WORKING" : "FAILED") << "\n";
        
        if (framework_ready && !compositions.empty()) {
            std::cout << "\n🎵 ClusterEngine Integration Test: SUCCESS!\n";
            std::cout << "\nThe framework is ready for Rules Interface System integration\n";
        } else {
            std::cout << "\n⚠ Integration framework needs additional work\n";
        }
        
        return framework_ready && !compositions.empty() ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}

// Build with: g++ -std=c++17 -I. test_rules_integration.cpp src/cluster_engine_implementation.cpp -o test_rules_integration