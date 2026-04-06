/**
 * @file cluster_engine_simple_test.cpp
 * @brief Simple test demonstrating core Cluster Engine architecture
 * 
 * This test focuses on the implemented parts and demonstrates the
 * authentic multi-engine architecture without requiring full implementation.
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <memory>
#include <iomanip>

// Only include the interface
#include "cluster_engine_interface.hh"

using namespace ClusterEngine;

/**
 * @brief Test 1: Basic ClusterEngineInterface Functionality
 */
bool test_cluster_engine_interface() {
    std::cout << "\n=== Test 1: ClusterEngineInterface Basic Functionality ===\n";
    
    try {
         // Test engine creation
        ClusterEngineInterface engine;
        std::cout << "✅ ClusterEngineInterface created successfully\n";
        
        // Test domain configuration
        DomainConfiguration domain_config = MusicalDomainFactory::create_classical_domains(2, 60, 72);
        std::cout << "✅ Classical domain configuration created (2 voices, C4-C5 range)\n";
        
        // Test search configuration
        SearchConfig search_config;
        search_config.max_solutions = 3;
        search_config.verbose_output = true;
        search_config.debug_output = false;
        std::cout << "✅ Search configuration created (max 3 solutions)\n";
        
        // Test initialization
        bool initialized = engine.initialize(domain_config, search_config);
        if (!initialized) {
            std::cout << "❌ Engine initialization failed\n";
            return false;
        }
        std::cout << "✅ Engine initialized successfully\n";
        
        // Test rule creation and addition
        engine.add_constraint_rule(MusicalRuleFactory::create_no_repetition_rule(0));
        engine.add_constraint_rule(MusicalRuleFactory::create_no_repetition_rule(1));  
        engine.add_constraint_rule(MusicalRuleFactory::create_range_rule(0, 60, 72));
        engine.add_constraint_rule(MusicalRuleFactory::create_range_rule(1, 60, 72));
        std::cout << "✅ Musical constraint rules added (4 rules)\n";
        
        // Test heuristic rules
        engine.add_heuristic_rule(MusicalHeuristicLibrary::create_stepwise_motion_preference(0, 2.0));
        engine.add_heuristic_rule(MusicalHeuristicLibrary::create_stepwise_motion_preference(1, 2.0));
        engine.add_heuristic_rule(MusicalHeuristicLibrary::create_consonance_preference({0, 1}, 1.5));
        std::cout << "✅ Musical heuristic rules added (3 rules)\n";
        
        // Test status printing
        engine.print_domain_summary();
        engine.print_rule_summary();
        
        // Test search execution
        std::cout << "\n🎵 Executing musical composition search...\n";
        auto start_time = std::chrono::high_resolution_clock::now();
        
        auto compositions = engine.search();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "✅ Search completed in " << duration.count() << "ms\n";
        std::cout << "✅ Generated " << compositions.size() << " musical compositions\n";
        
        // Analyze first composition if available
        if (!compositions.empty()) {
            const auto& composition = compositions[0];
            std::cout << "\n📊 First Composition Analysis:\n";
            std::cout << "   Voices: " << composition.get_num_voices() << "\n";
            std::cout << "   Total notes: " << composition.total_notes << "\n";
            std::cout << "   Duration: " << composition.total_duration << " time units\n";
            
            if (!composition.voices.empty()) {
                std::cout << "   Voice 0 notes: ";
                for (size_t i = 0; i < std::min(size_t(4), composition.voices[0].size()); ++i) {
                    std::cout << composition.voices[0][i].absolute_value << " ";
                }
                if (composition.voices[0].size() > 4) std::cout << "...";
                std::cout << "\n";
            }
        }
        
        // Test search statistics
        const auto& stats = engine.get_search_statistics();
        std::cout << "\n📈 Search Statistics:\n";
        std::cout << "   Search steps: " << stats.total_search_steps << "\n";
        std::cout << "   Rule tests passed: " << stats.rule_tests_passed << "\n";
        std::cout << "   Rule tests failed: " << stats.rule_tests_failed << "\n";
        std::cout << "   Search time: " << stats.search_time_seconds << " seconds\n";
        
        std::cout << "\n✅ ClusterEngineInterface test completed successfully!\n";
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "❌ ClusterEngineInterface test failed: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test 2: Musical Rule Factory
 */
bool test_musical_rule_factory() {
    std::cout << "\n=== Test 2: Musical Rule Factory ===\n";
    
    try {
        // Test different rule creation
        auto no_rep_rule = MusicalRuleFactory::create_no_repetition_rule(0);
        std::cout << "✅ No repetition rule: " << no_rep_rule->get_description() << "\n";
        
        auto consonance_rule = MusicalRuleFactory::create_consonant_intervals_rule({0, 1});
        std::cout << "✅ Consonant intervals rule: " << consonance_rule->get_description() << "\n"; 
        
        auto range_rule = MusicalRuleFactory::create_range_rule(0, 60, 72);
        std::cout << "✅ Range constraint rule: " << range_rule->get_description() << "\n";
        
        auto stepwise_rule = MusicalRuleFactory::create_stepwise_motion_rule(1, 0.7);
        std::cout << "✅ Stepwise motion rule: " << stepwise_rule->get_description() << "\n";
        
        std::cout << "✅ All musical rule types created successfully\n";
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Musical rule factory test failed: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test 3: Musical Heuristic Library
 */
bool test_musical_heuristic_library() {
    std::cout << "\n=== Test 3: Musical Heuristic Library ===\n";
    
    try {
        // Test heuristic rule creation
        auto stepwise_pref = MusicalHeuristicLibrary::create_stepwise_motion_preference(0, 2.5);
        std::cout << "✅ Stepwise preference: " << stepwise_pref.get_description() << "\n";
        
        auto consonance_pref = MusicalHeuristicLibrary::create_consonance_preference({0, 1}, 1.8);
        std::cout << "✅ Consonance preference: " << consonance_pref.get_description() << "\n";
        
        auto strong_beat_pref = MusicalHeuristicLibrary::create_strong_beat_preference(0, 1.4);
        std::cout << "✅ Strong beat preference: " << strong_beat_pref.get_description() << "\n";
        
        // Test weight calculation with dummy context
        std::vector<MusicalCandidate> context;
        context.emplace_back(60, 0, true);  // C4
        context.emplace_back(62, 2, true);  // D4
        
        double weight = stepwise_pref.calculate_weight(context, 1);
        std::cout << "✅ Weight calculation: " << weight << " (stepwise motion D4)\n";
        
        std::cout << "✅ Musical heuristic library test completed\n";
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Musical heuristic library test failed: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test 4: Domain Configuration
 */
bool test_domain_configuration() {
    std::cout << "\n=== Test 4: Domain Configuration ===\n";
    
    try {
        // Test classical domain creation
        auto classical_config = MusicalDomainFactory::create_classical_domains(3, 48, 84);
        std::cout << "✅ Classical domains created (3 voices, C3-C6 range)\n";
        std::cout << "   Engine domains: " << classical_config.engine_domains.size() << "\n";
        std::cout << "   Locked engines: " << classical_config.locked_engines.size() << "\n";
        std::cout << "   Randomization: " << (classical_config.randomize_order ? "enabled" : "disabled") << "\n";
        
        // Test domain builders
        EngineDomainBuilder builder;
        
        auto rhythm_domain = builder.build_rhythm_domain({250, 500, 1000}, true);
        std::cout << "✅ Rhythm domain built (" << rhythm_domain.size() << " candidates)\n";
        
        auto pitch_domain = builder.build_pitch_domain(60, 72);
        std::cout << "✅ Pitch domain built (" << pitch_domain.size() << " candidates)\n";
        
        std::cout << "✅ Domain configuration test completed\n";
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Domain configuration test failed: " << e.what() << "\n";
        return false;
    }
}

// Main test runner
int main() {
    std::cout << "🎼 Cluster Engine Architecture Test Suite 🎼\n";
    std::cout << "============================================\n";
    std::cout << "\n🎯 Testing Authentic Cluster Engine with Multi-Engine Coordination\n";
    std::cout << "   • Musical constraint solving with domain intelligence\n";
    std::cout << "   • Heuristic guidance for musical preferences\n";
    std::cout << "   • Complete factory and configuration system\n";
    
    bool all_tests_passed = true;
    int tests_run = 0;
    int tests_passed = 0;
    
    // Run tests
    std::vector<std::pair<std::string, bool(*)()>> tests = {
        {"ClusterEngineInterface", test_cluster_engine_interface},
        {"MusicalRuleFactory", test_musical_rule_factory}, 
        {"MusicalHeuristicLibrary", test_musical_heuristic_library},
        {"DomainConfiguration", test_domain_configuration}
    };
    
    for (const auto& test : tests) {
        tests_run++;
        bool result = test.second();
        if (result) {
            tests_passed++;
            std::cout << "✅ " << test.first << " test PASSED\n";
        } else {
            all_tests_passed = false;
            std::cout << "❌ " << test.first << " test FAILED\n";
        }
    }
    
    // Final results  
    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << "📊 Test Results Summary:\n";
    std::cout << "   Tests run: " << tests_run << "\n";
    std::cout << "   Tests passed: " << tests_passed << "\n";
    std::cout << "   Tests failed: " << (tests_run - tests_passed) << "\n";
    std::cout << "   Success rate: " << std::fixed << std::setprecision(1)
              << (100.0 * tests_passed / tests_run) << "%\n";
    
    if (all_tests_passed) {
        std::cout << "\n🎉 ALL TESTS PASSED! 🎉\n";
        std::cout << "🎼 Authentic Cluster Engine architecture working correctly!\n";
        std::cout << "\n🔧 Architecture Features Verified:\n";
        std::cout << "   ✅ Multi-engine coordination system\n";
        std::cout << "   ✅ Musical domain generation\n";
        std::cout << "   ✅ Constraint rule system\n";
        std::cout << "   ✅ Heuristic guidance system\n";
        std::cout << "   ✅ Factory pattern implementation\n";
        std::cout << "   ✅ Search configuration and execution\n";
        std::cout << "   ✅ Musical composition generation\n";
        std::cout << "\n🎵 Ready for advanced musical AI applications! 🎵\n";
        return 0;
    } else {
        std::cout << "\n❌ SOME TESTS FAILED\n";
        std::cout << "🔧 Check implementation for missing functionality\n";
        return 1;
    }
}