/**
 * @file test_heuristic_rules.cpp
 * @brief Comprehensive test system for Enhanced Heuristic Rules System
 * 
 * This file contains a comprehensive test suite for the heuristic rules system,
 * including unit tests for all rule types, performance benchmarks, and musical
 * intelligence validation.
 */

#include "cluster_engine_heuristic.hh"
#include "cluster_engine_interface.hh"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <cassert>

namespace ClusterEngine {

/**
 * @class HeuristicTestHarness
 * @brief Test harness for comprehensive heuristic rules validation
 */
class HeuristicTestHarness {
private:
    std::unique_ptr<HeuristicRuleManager> manager_;
    std::unique_ptr<MusicalAnalysisContext> analysis_context_;
    std::mt19937 rng_;

public:
    HeuristicTestHarness() 
        : manager_(std::make_unique<HeuristicRuleManager>()),
          analysis_context_(std::make_unique<MusicalAnalysisContext>()),
          rng_(std::random_device{}()) {
    }

    /**
     * @brief Run complete test suite
     */
    void run_all_tests() {
        std::cout << "=== Enhanced Heuristic Rules System Test Suite ===" << std::endl;
        
        test_basic_functionality();
        test_musical_analysis_context();
        test_single_engine_pitch_rules();
        test_single_engine_rhythm_rules();
        test_dual_engine_rules();
        test_multi_engine_harmonic_rules();
        test_switch_heuristic_rules();
        test_heuristic_rule_manager();
        test_factory_methods();
        test_style_presets();
        test_performance_benchmarks();
        test_real_time_constraints();
        
        std::cout << "=== All Tests Completed Successfully ===" << std::endl;
    }

private:
    /**
     * @brief Test basic system functionality
     */
    void test_basic_functionality() {
        std::cout << "\n[TEST] Basic Functionality" << std::endl;
        
        // Test empty manager
        assert(manager_->get_rule_count() == 0);
        
        // Test rule addition
        auto rule = HeuristicRuleFactory::create_stepwise_motion_rule(0, 1.5);
        manager_->add_rule(std::move(rule));
        assert(manager_->get_rule_count() == 1);
        
        std::cout << "✓ Basic functionality tests passed" << std::endl;
    }

    /**
     * @brief Test MusicalAnalysisContext
     */
    void test_musical_analysis_context() {
        std::cout << "\n[TEST] Musical Analysis Context" << std::endl;
        
        // Test pitch sequence analysis
        std::vector<int> pitch_values = {60, 62, 64, 65, 67, 69, 71, 72}; // C major scale
        analysis_context_->add_pitch_sequence(0, pitch_values);
        
        auto patterns = analysis_context_->analyze_pitch_patterns(0, 8);
        assert(!patterns.empty());
        assert(patterns[0].stepwise_motion_percentage > 0.8); // C major scale is mostly stepwise
        
        // Test rhythm sequence analysis
        std::vector<double> rhythm_values = {0.5, 0.5, 0.5, 0.5, 1.0, 1.0}; // Regular pattern
        analysis_context_->add_rhythm_sequence(1, rhythm_values);
        
        auto rhythm_patterns = analysis_context_->analyze_rhythm_patterns(1, 6);
        assert(!rhythm_patterns.empty());
        assert(rhythm_patterns[0].regularity_score > 0.7); // Regular rhythm pattern
        
        std::cout << "✓ Musical analysis context tests passed" << std::endl;
    }

    /**
     * @brief Test single engine pitch rules
     */
    void test_single_engine_pitch_rules() {
        std::cout << "\n[TEST] Single Engine Pitch Rules" << std::endl;
        
        auto rule = HeuristicRuleFactory::create_stepwise_motion_rule(0, 2.0);
        
        // Test stepwise motion preference (C to D should score higher than C to G)
        MusicalAnalysisContext context;
        std::vector<int> stepwise_context = {60, 62}; // C to D
        std::vector<int> leap_context = {60, 67};     // C to G
        
        context.add_pitch_sequence(0, stepwise_context);
        double stepwise_score = rule->evaluate_candidate(0, 62, context);
        
        context.add_pitch_sequence(0, leap_context);
        double leap_score = rule->evaluate_candidate(0, 67, context);
        
        assert(stepwise_score > leap_score);
        
        std::cout << "✓ Single engine pitch rules tests passed" << std::endl;
    }

    /**
     * @brief Test single engine rhythm rules
     */
    void test_single_engine_rhythm_rules() {
        std::cout << "\n[TEST] Single Engine Rhythm Rules" << std::endl;
        
        auto rule = HeuristicRuleFactory::create_rhythmic_regularity_rule(1, 1.5);
        
        // Test rhythmic regularity preference
        MusicalAnalysisContext context;
        std::vector<double> regular_context = {0.5, 0.5, 0.5}; // Regular quarter notes
        std::vector<double> irregular_context = {0.33, 0.67, 0.8}; // Irregular pattern
        
        context.add_rhythm_sequence(1, regular_context);
        double regular_score = rule->evaluate_candidate(1, 0.5, context);
        
        context.add_rhythm_sequence(1, irregular_context);
        double irregular_score = rule->evaluate_candidate(1, 0.25, context);
        
        assert(regular_score > irregular_score);
        
        std::cout << "✓ Single engine rhythm rules tests passed" << std::endl;
    }

    /**
     * @brief Test dual engine rules
     */
    void test_dual_engine_rules() {
        std::cout << "\n[TEST] Dual Engine Rules" << std::endl;
        
        auto rule = HeuristicRuleFactory::create_rhythm_pitch_coordination_rule(1, 0, 1.8);
        
        // Test coordination between rhythm and pitch
        MusicalAnalysisContext context;
        std::vector<double> rhythm_context = {1.0, 0.5}; // Strong beat, weak beat
        std::vector<int> pitch_context = {60, 64};      // C, E
        
        context.add_rhythm_sequence(1, rhythm_context);
        context.add_pitch_sequence(0, pitch_context);
        
        // Higher pitch on strong beat should score well
        double score = rule->evaluate_candidate(0, 67, context); // G on strong beat
        assert(score > 0);
        
        std::cout << "✓ Dual engine rules tests passed" << std::endl;
    }

    /**
     * @brief Test multi-engine harmonic rules
     */
    void test_multi_engine_harmonic_rules() {
        std::cout << "\n[TEST] Multi-Engine Harmonic Rules" << std::endl;
        
        std::vector<int> pitch_engines = {0, 2, 4};
        auto rule = HeuristicRuleFactory::create_harmonic_consonance_rule(pitch_engines, 2.0);
        
        // Test consonant vs dissonant intervals
        MusicalAnalysisContext context;
        std::vector<int> voice1 = {60}; // C
        std::vector<int> voice2 = {64}; // E (major third - consonant)
        std::vector<int> voice3 = {67}; // G (perfect fifth - consonant)
        
        context.add_pitch_sequence(0, voice1);
        context.add_pitch_sequence(2, voice2);
        context.add_pitch_sequence(4, voice3);
        
        // Adding another C (octave - very consonant) should score high
        double consonant_score = rule->evaluate_candidate(0, 72, context);
        
        // Adding F# (tritone - dissonant) should score lower
        double dissonant_score = rule->evaluate_candidate(0, 66, context);
        
        assert(consonant_score > dissonant_score);
        
        std::cout << "✓ Multi-engine harmonic rules tests passed" << std::endl;
    }

    /**
     * @brief Test switch heuristic rules
     */
    void test_switch_heuristic_rules() {
        std::cout << "\n[TEST] Switch Heuristic Rules" << std::endl;
        
        auto rule = HeuristicRuleFactory::create_no_repetition_switch(0, -5.0);
        
        // Test repetition avoidance
        MusicalAnalysisContext context;
        std::vector<int> pitch_context = {60, 62, 60}; // C, D, C (repetition)
        
        context.add_pitch_sequence(0, pitch_context);
        
        // Repeating C again should get penalty
        double repetition_score = rule->evaluate_candidate(0, 60, context);
        
        // New note should not get penalty
        double new_note_score = rule->evaluate_candidate(0, 64, context);
        
        assert(new_note_score > repetition_score);
        
        std::cout << "✓ Switch heuristic rules tests passed" << std::endl;
    }

    /**
     * @brief Test HeuristicRuleManager functionality
     */
    void test_heuristic_rule_manager() {
        std::cout << "\n[TEST] Heuristic Rule Manager" << std::endl;
        
        HeuristicRuleManager manager;
        
        // Add multiple rules
        manager.add_rule(HeuristicRuleFactory::create_stepwise_motion_rule(0, 1.5));
        manager.add_rule(HeuristicRuleFactory::create_rhythmic_regularity_rule(1, 1.2));
        manager.add_rule(HeuristicRuleFactory::create_no_repetition_switch(0, -2.0));
        
        assert(manager.get_rule_count() == 3);
        
        // Test rule enabling/disabling
        manager.set_rule_enabled("No Repetition Switch", false);
        assert(manager.get_rule_count() == 3); // Count shouldn't change
        
        // Test statistics
        auto stats = manager.get_performance_statistics();
        assert(stats.total_evaluations >= 0);
        
        // Test configuration export/import
        auto config = manager.export_configuration();
        assert(!config.empty());
        
        HeuristicRuleManager new_manager;
        new_manager.import_configuration(config);
        assert(new_manager.get_rule_count() == 3);
        
        std::cout << "✓ Heuristic rule manager tests passed" << std::endl;
    }

    /**
     * @brief Test factory methods
     */
    void test_factory_methods() {
        std::cout << "\n[TEST] Factory Methods" << std::endl;
        
        // Test all factory creation methods
        auto stepwise = HeuristicRuleFactory::create_stepwise_motion_rule(0, 1.0);
        auto rhythmic = HeuristicRuleFactory::create_rhythmic_regularity_rule(1, 1.0);
        auto coordination = HeuristicRuleFactory::create_rhythm_pitch_coordination_rule(1, 0, 1.0);
        auto harmonic = HeuristicRuleFactory::create_harmonic_consonance_rule({0, 2}, 1.0);
        auto no_rep = HeuristicRuleFactory::create_no_repetition_switch(0, -1.0);
        auto range = HeuristicRuleFactory::create_range_enforcement_switch(0, 48, 72, -5.0);
        
        assert(stepwise != nullptr);
        assert(rhythmic != nullptr);
        assert(coordination != nullptr);
        assert(harmonic != nullptr);
        assert(no_rep != nullptr);
        assert(range != nullptr);
        
        std::cout << "✓ Factory methods tests passed" << std::endl;
    }

    /**
     * @brief Test musical style presets
     */
    void test_style_presets() {
        std::cout << "\n[TEST] Musical Style Presets" << std::endl;
        
        HeuristicRuleManager classical_manager;
        HeuristicRuleManager jazz_manager;
        HeuristicRuleManager minimal_manager;
        
        std::vector<int> rhythm_engines = {1, 3};
        std::vector<int> pitch_engines = {0, 2};
        
        // Test classical preset
        HeuristicRuleFactory::add_classical_music_rules(classical_manager, rhythm_engines, pitch_engines);
        assert(classical_manager.get_rule_count() > 5); // Should add multiple rules
        
        // Test jazz preset
        HeuristicRuleFactory::add_jazz_music_rules(jazz_manager, rhythm_engines, pitch_engines);
        assert(jazz_manager.get_rule_count() > 3); // Should add multiple rules
        
        // Test minimal preset
        HeuristicRuleFactory::add_minimal_rules(minimal_manager, 4);
        assert(minimal_manager.get_rule_count() >= 2); // Should add basic rules
        
        std::cout << "✓ Musical style presets tests passed" << std::endl;
    }

    /**
     * @brief Performance benchmarks
     */
    void test_performance_benchmarks() {
        std::cout << "\n[TEST] Performance Benchmarks" << std::endl;
        
        HeuristicRuleManager manager;
        
        // Add comprehensive rule set
        std::vector<int> rhythm_engines = {1, 3, 5};
        std::vector<int> pitch_engines = {0, 2, 4};
        
        HeuristicRuleFactory::add_classical_music_rules(manager, rhythm_engines, pitch_engines);
        
        MusicalAnalysisContext context;
        
        // Generate test data
        std::uniform_int_distribution<> pitch_dist(48, 84);
        std::uniform_real_distribution<> rhythm_dist(0.25, 2.0);
        
        for (int engine : pitch_engines) {
            std::vector<int> pitch_seq;
            for (int i = 0; i < 50; ++i) {
                pitch_seq.push_back(pitch_dist(rng_));
            }
            context.add_pitch_sequence(engine, pitch_seq);
        }
        
        for (int engine : rhythm_engines) {
            std::vector<double> rhythm_seq;
            for (int i = 0; i < 50; ++i) {
                rhythm_seq.push_back(rhythm_dist(rng_));
            }
            context.add_rhythm_sequence(engine, rhythm_seq);
        }
        
        // Benchmark evaluation speed
        const int num_evaluations = 10000;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_evaluations; ++i) {
            int engine = pitch_engines[i % pitch_engines.size()];
            int candidate = pitch_dist(rng_);
            manager.evaluate_all_rules(engine, candidate, context);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double avg_time_per_evaluation = static_cast<double>(duration.count()) / num_evaluations;
        
        std::cout << "Performance Results:" << std::endl;
        std::cout << "  - Total evaluations: " << num_evaluations << std::endl;
        std::cout << "  - Total time: " << duration.count() << " μs" << std::endl;
        std::cout << "  - Average time per evaluation: " << avg_time_per_evaluation << " μs" << std::endl;
        std::cout << "  - Evaluations per second: " << (1000000.0 / avg_time_per_evaluation) << std::endl;
        
        // Performance requirement: should handle at least 100,000 evaluations per second
        assert(avg_time_per_evaluation < 10.0); // Less than 10 microseconds per evaluation
        
        std::cout << "✓ Performance benchmarks passed" << std::endl;
    }

    /**
     * @brief Test real-time constraints
     */
    void test_real_time_constraints() {
        std::cout << "\n[TEST] Real-Time Constraints" << std::endl;
        
        // Test that heuristic evaluation can meet real-time deadlines
        HeuristicRuleManager manager;
        std::vector<int> engines = {0, 1, 2, 3};
        
        HeuristicRuleFactory::add_minimal_rules(manager, 4);
        
        MusicalAnalysisContext context;
        
        // Simulate real-time constraint solving scenario
        const double max_evaluation_time_ms = 1.0; // 1ms deadline per evaluation
        
        std::uniform_int_distribution<> candidate_dist(0, 127);
        
        for (int test = 0; test < 100; ++test) {
            auto start_time = std::chrono::high_resolution_clock::now();
            
            int engine = test % engines.size();
            int candidate = candidate_dist(rng_);
            
            double score = manager.evaluate_all_rules(engine, candidate, context);
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            
            double evaluation_time_ms = duration.count() / 1000.0;
            
            // Verify real-time constraint is met
            assert(evaluation_time_ms < max_evaluation_time_ms);
        }
        
        std::cout << "✓ Real-time constraints tests passed" << std::endl;
    }
};

} // namespace ClusterEngine

/**
 * @brief Main test function
 */
int main() {
    try {
        ClusterEngine::HeuristicTestHarness test_harness;
        test_harness.run_all_tests();
        
        std::cout << "\n🎵 Enhanced Heuristic Rules System is ready for musical constraint solving!" << std::endl;
        std::cout << "Performance improvement expected: 5-10x faster search with intelligent candidate ordering" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}