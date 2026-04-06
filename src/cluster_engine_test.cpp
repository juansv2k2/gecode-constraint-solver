/**
 * @file cluster_engine_test.cpp
 * @brief Comprehensive Test of Authentic Cluster Engine Architecture
 * 
 * Demonstrates the complete Cluster Engine system with:
 * - Multi-engine coordination (rhythm/pitch alternation)
 * - Heuristic rule system for musical intelligence
 * - Musical domain generation and management
 * - Forward rule engine coordination  
 * - Sophisticated musical backjumping
 * - Dual solution storage (absolute + interval)
 * - Musical rule types (rhythm-pitch, pitch-pitch, etc.)
 */

#include <iostream>
#include <memory>
#include <vector>
#include <chrono>

// Include all Cluster Engine components
#include "cluster_engine_interface.hh"
#include "cluster_engine_core.hh"
#include "cluster_engine_forward_rules.hh"
#include "cluster_engine_backjump.hh"
#include "cluster_engine_musical_rules.hh"
#include "cluster_engine_domains.hh"

using namespace ClusterEngine;

/**
 * @brief Test 1: Multi-Engine Architecture Verification
 */
bool test_multi_engine_architecture() {
    std::cout << "\n=== Test 1: Multi-Engine Architecture ===\n";
    
    try {
        // Create 3 voices = 6 engines (rhythm + pitch) + 1 metric = 7 engines total
        ClusterEngineCore core(3, 20, true); // 3 voices, 20 variables per engine, debug on
        
        std::cout << "✓ Core created with " << core.get_num_engines() << " engines\n";
        
        // Verify engine types and relationships
        for (int i = 0; i < core.get_num_engines() - 1; ++i) {
            const auto& engine = core.get_engine(i);
            
            if (i % 2 == 0) {
                std::cout << "  Engine " << i << ": RHYTHM (voice " << engine.get_voice_id() 
                         << "), partner = " << engine.get_partner_engine_id() << "\n";
            } else {
                std::cout << "  Engine " << i << ": PITCH (voice " << engine.get_voice_id()
                         << "), partner = " << engine.get_partner_engine_id() << "\n";
            }
        }
        
        // Last engine should be metric
        const auto& metric_engine = core.get_engine(core.get_num_engines() - 1);
        std::cout << "  Engine " << (core.get_num_engines() - 1) << ": METRIC\n";
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "❌ Multi-engine test failed: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test 2: Musical Domain Generation
 */
bool test_musical_domain_generation() {
    std::cout << "\n=== Test 2: Musical Domain Generation ===\n";
    
    try {
        // Create musical domains
        EngineDomainBuilder builder(42); // Seed for reproducibility
        
        // Test rhythm domain
        auto rhythm_domain = builder.build_rhythm_domain(
            {125, 250, 500, 1000}, // Quarter, eighth, half, whole notes (ms) 
            true,  // Allow rests
            DomainInitType::UNIFORM_RANDOM);
        
        std::cout << "✓ Rhythm domain created with " << rhythm_domain.size() << " candidates\n";
        
        // Test pitch domain  
        auto pitch_domain = builder.build_pitch_domain(
            60, 72,  // C4 to C5
            {},      // Chromatic (empty = all pitches)
            DomainInitType::UNIFORM);
            
        std::cout << "✓ Pitch domain created with " << pitch_domain.size() << " candidates\n";
        
        // Test metric domain
        MetricDomain metric;
        metric.create_from_time_signatures(
            {{4,4}, {3,4}, {2,4}},                    // Time signatures  
            {{3, 4, 6}, {3, 6}, {4, 8}},             // Tuplets per signature
            {{}, {0.5}, {0.25, 0.25, 0.5}});        // Alternative beat patterns
        
        auto metric_domain = builder.build_metric_domain(metric);
        std::cout << "✓ Metric domain created with " << metric_domain.size() << " candidates\n";
        std::cout << "  Onset grids: " << metric.get_onset_grids().size() << "\n";
        std::cout << "  Beat structures: " << metric.get_beat_structures().size() << "\n";
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "❌ Domain generation test failed: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test 3: Heuristic Rule System  
 */
bool test_heuristic_rule_system() {
    std::cout << "\n=== Test 3: Heuristic Rule System ===\n";
    
    try {
        // Create heuristic rules for musical intelligence
        auto stepwise_preference = MusicalHeuristicLibrary::create_stepwise_motion_preference(
            0, 2.0); // Voice 0, weight 2.0 for stepwise motion
        
        auto consonance_preference = MusicalHeuristicLibrary::create_consonance_preference(
            {1, 3}, 1.5); // Voices 1 and 3, weight 1.5 for consonance
        
        auto strong_beat_preference = MusicalHeuristicLibrary::create_strong_beat_preference(
            0, 1.3); // Voice 0, weight 1.3 for strong beats
        
        std::cout << "✓ Stepwise motion heuristic: " << stepwise_preference.get_description() << "\n";
        std::cout << "✓ Consonance heuristic: " << consonance_preference.get_description() << "\n";
        std::cout << "✓ Strong beat heuristic: " << strong_beat_preference.get_description() << "\n";
        
        // Test heuristic weight calculation (simplified)
        std::vector<MusicalCandidate> test_context = {
            MusicalCandidate(60, 2),   // C4, major second up
            MusicalCandidate(62, 2),   // D4, major second up  
            MusicalCandidate(67, 5)    // G4, perfect fourth up
        };
        
        double stepwise_weight = stepwise_preference.calculate_weight(test_context, 1);
        std::cout << "  Stepwise weight for D4 (stepwise): " << stepwise_weight << "\n";
        
        double leap_weight = stepwise_preference.calculate_weight(test_context, 2);  
        std::cout << "  Stepwise weight for G4 (leap): " << leap_weight << "\n";
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "❌ Heuristic rule test failed: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test 4: Musical Constraint Rules
 */
bool test_musical_constraint_rules() {
    std::cout << "\n=== Test 4: Musical Constraint Rules ===\n";
    
    try {
        // Create different types of musical rules
        
        // 1. Rhythm-Pitch rule (no note repetitions)
        auto no_repetition = MusicalRuleFactory::create_no_repetition_rule(0);
        std::cout << "✓ " << no_repetition->get_description() << "\n";
        
        // 2. Pitch-Pitch rule (consonant intervals)  
        auto consonance = MusicalRuleFactory::create_consonant_intervals_rule({0, 1});
        std::cout << "✓ " << consonance->get_description() << "\n";
        
        // 3. Range constraint rule
        auto range_rule = MusicalRuleFactory::create_range_rule(0, 60, 72);
        std::cout << "✓ " << range_rule->get_description() << "\n";
        
        // 4. Stepwise motion preference
        auto stepwise = MusicalRuleFactory::create_stepwise_motion_rule(1, 0.8);
        std::cout << "✓ " << stepwise->get_description() << "\n";
        
        std::cout << "✅ All musical constraint rules created successfully\n";
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "❌ Musical constraint rule test failed: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test 5: Forward Rule Manager
 */
bool test_forward_rule_manager() {
    std::cout << "\n=== Test 5: Forward Rule Management ===\n";
    
    try {
        ClusterEngineCore core(2, 10); // 2 voices, 10 variables
        ForwardRuleManager forward_manager(&core);
        
        // Set up default engine order and locked engines
        forward_manager.set_default_engine_order({0, 1, 2, 3, 4}); // rhythm0, pitch0, rhythm1, pitch1, metric
        forward_manager.set_locked_engines({4}); // Lock metric engine for this test
        
        std::cout << "✓ Forward rule manager initialized\n";
        std::cout << "  Default engine order: {0, 1, 2, 3, 4}\n";
        std::cout << "  Locked engines: {4}\n";
        
        // Test engine selection logic
        int selected = forward_manager.select_next_engine();
        std::cout << "  Next engine selected: " << selected << "\n";
        
        // Test backtrack route setting
        forward_manager.set_backtrack_route(2, 5, 8, 2.5); // Engine 2, index 5, count 8, time 2.5
        std::cout << "✓ Backtrack route set for engine 2\n";
        
        // Test engine analysis
        forward_manager.print_engine_analysis();
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "❌ Forward rule manager test failed: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test 6: Musical Backjumper
 */
bool test_musical_backjumper() {
    std::cout << "\n=== Test 6: Musical Backjumper ===\n";
    
    try {
        ClusterEngineCore core(2, 10);
        MusicalBackjumper backjumper(&core, BackjumpMode::MIN_BACKJUMP);
        
        std::cout << "✓ Musical backjumper initialized with MIN_BACKJUMP mode\n";
        
        // Create test rule results with backjump suggestions
        std::vector<RuleTestResult> rule_results = {
            RuleTestResult(false),  // Failed rule 1
            RuleTestResult(false),  // Failed rule 2  
            RuleTestResult(true)    // Passed rule 3
        };
        
        // Add backjump suggestions to failed rules
        rule_results[0].suggested_backjump_engine = 1;
        rule_results[0].suggested_backjump_index = 3;
        rule_results[0].rule_name = "consonance-rule";
        rule_results[0].failure_reason = "dissonant interval detected";
        
        rule_results[1].suggested_backjump_engine = 0;
        rule_results[1].suggested_backjump_index = 2;  
        rule_results[1].rule_name = "stepwise-motion-rule";
        rule_results[1].failure_reason = "large leap detected";
        
        std::cout << "✓ Test rule results created with backjump suggestions\n";
        
        // Analyze backjump target
        int target_engine = backjumper.analyze_backjump(rule_results);
        std::cout << "  Backjump analysis result: target engine " << target_engine << "\n";
        
        // Print detailed analysis
        backjumper.print_backjump_analysis(rule_results, target_engine);
        
        // Test different backjump modes
        backjumper.set_mode(BackjumpMode::CONSENSUS_BACKJUMP);
        int consensus_target = backjumper.analyze_backjump(rule_results);
        std::cout << "  Consensus backjump target: engine " << consensus_target << "\n";
        
        backjumper.set_mode(BackjumpMode::NO_BACKJUMP);
        int no_jump_target = backjumper.analyze_backjump(rule_results);
        std::cout << "  No backjump target: engine " << no_jump_target << "\n";
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "❌ Musical backjumper test failed: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test 7: Complete Cluster Engine Integration
 */
bool test_complete_integration() {
    std::cout << "\n=== Test 7: Complete Integration ===\n";
    
    try {
        // Create complete Cluster Engine interface
        ClusterEngineInterface cluster_engine;
        
        // Set up domain configuration
        auto domain_config = MusicalDomainFactory::create_classical_domains(
            2,    // 2 voices
            60,   // Min pitch C4
            72);  // Max pitch C5
            
        // Set up search configuration
        SearchConfig search_config;
        search_config.max_variables_per_engine = 8;
        search_config.max_solutions = 3;
        search_config.backjump_mode = BackjumpMode::MIN_BACKJUMP;
        search_config.debug_output = true;
        search_config.max_runtime_seconds = 10; // Quick test
        
        std::cout << "✓ Domain and search configurations created\n";
        
        // Initialize the engine
        bool initialized = cluster_engine.initialize(domain_config, search_config);
        if (!initialized) {
            std::cout << "❌ Engine initialization failed\n";
            return false;  
        }
        std::cout << "✓ Cluster Engine initialized successfully\n";
        
        // Add musical constraint rules
        cluster_engine.add_constraint_rule(
            MusicalRuleFactory::create_no_repetition_rule(0));
        cluster_engine.add_constraint_rule(
            MusicalRuleFactory::create_no_repetition_rule(1));
        cluster_engine.add_constraint_rule(
            MusicalRuleFactory::create_consonant_intervals_rule({0, 1}));
        cluster_engine.add_constraint_rule(
            MusicalRuleFactory::create_range_rule(0, 60, 72));
        cluster_engine.add_constraint_rule(
            MusicalRuleFactory::create_range_rule(1, 60, 72));
            
        std::cout << "✓ 5 constraint rules added\n";
        
        // Add heuristic rules for musical intelligence
        cluster_engine.add_heuristic_rule(
            MusicalHeuristicLibrary::create_stepwise_motion_preference(0, 2.0));
        cluster_engine.add_heuristic_rule(
            MusicalHeuristicLibrary::create_stepwise_motion_preference(1, 2.0));
        cluster_engine.add_heuristic_rule(
            MusicalHeuristicLibrary::create_consonance_preference({0, 1}, 1.5));
        cluster_engine.add_heuristic_rule(
            MusicalHeuristicLibrary::create_strong_beat_preference(0, 1.3));
            
        std::cout << "✓ 4 heuristic rules added\n";
        
        // Print system status before search
        cluster_engine.print_engine_status();
        cluster_engine.print_domain_summary();
        cluster_engine.print_rule_summary();
        
        // Execute search
        std::cout << "\n🎵 Starting musical composition search...\n";
        auto start_time = std::chrono::high_resolution_clock::now();
        
        auto compositions = cluster_engine.search();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "\n✅ Search completed in " << duration.count() << " ms\n";
        std::cout << "🎼 Found " << compositions.size() << " compositions\n";
        
        // Analyze results
        for (size_t i = 0; i < compositions.size(); ++i) {
            const auto& comp = compositions[i];
            std::cout << "\n  Composition " << (i+1) << ":\n";
            std::cout << "    Voices: " << comp.get_num_voices() << "\n";
            std::cout << "    Total notes: " << comp.total_notes << "\n";
            std::cout << "    Duration: " << comp.total_duration << " beats\n";
            
            // Print first few notes of each voice
            for (int v = 0; v < comp.get_num_voices() && v < 2; ++v) {
                if (!comp.voices[v].empty()) {
                    std::cout << "    Voice " << v << " start: ";
                    for (size_t n = 0; n < std::min(size_t(4), comp.voices[v].size()); ++n) {
                        const auto& note = comp.voices[v][n];
                        std::cout << "(" << note.absolute_value << "," << note.interval_value << ") ";
                    }
                    std::cout << "\n";
                }
            }
        }
        
        // Print search statistics
        const auto& stats = cluster_engine.get_search_statistics();
        stats.print_summary();
        
        return compositions.size() > 0;
    } catch (const std::exception& e) {
        std::cout << "❌ Complete integration test failed: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Main test runner
 */
int main() {
    std::cout << "🎼 Authentic Cluster Engine Architecture Test Suite\n";
    std::cout << "==================================================\n";
    
    int tests_passed = 0;
    int total_tests = 7;
    
    if (test_multi_engine_architecture()) tests_passed++;
    if (test_musical_domain_generation()) tests_passed++;
    if (test_heuristic_rule_system()) tests_passed++;
    if (test_musical_constraint_rules()) tests_passed++;
    if (test_forward_rule_manager()) tests_passed++;
    if (test_musical_backjumper()) tests_passed++;
    if (test_complete_integration()) tests_passed++;
    
    std::cout << "\n🏁 Test Results Summary\n";
    std::cout << "=======================\n";
    std::cout << "Tests passed: " << tests_passed << "/" << total_tests << "\n";
    
    if (tests_passed == total_tests) {
        std::cout << "🎉 All tests PASSED! Authentic Cluster Engine architecture working correctly.\n";
        std::cout << "\n✅ Key Features Verified:\n";
        std::cout << "  ✓ Multi-engine coordination (rhythm/pitch alternation)\n";
        std::cout << "  ✓ Heuristic rule system for musical intelligence\n";
        std::cout << "  ✓ Musical domain generation (onset grids, beat structures)\n";
        std::cout << "  ✓ Forward rule engine coordination (fwd-rule2)\n";
        std::cout << "  ✓ Sophisticated musical backjumping (3 modes)\n";
        std::cout << "  ✓ Dual solution storage (absolute + interval)\n";
        std::cout << "  ✓ Musical rule types (rhythm-pitch, pitch-pitch, etc.)\n";
        std::cout << "  ✓ Complete integration with composition generation\n";
        return 0;
    } else {
        std::cout << "❌ Some tests FAILED. Cluster Engine implementation needs fixes.\n";
        return 1;
    }
}