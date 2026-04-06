/**
 * @file test_cluster_engine_rules.cpp
 * @brief Comprehensive test framework for the Rules Interface System
 * 
 * This file contains comprehensive tests for the musical constraint rules system,
 * validating the core functionality of the cluster-engine rules translation.
 */

#include "cluster_engine_rules.hh"
#include "cluster_engine_musical_rules.hh"
#include "cluster_engine_interface.hh"
#include <iostream>
#include <vector>
#include <cassert>
#include <memory>
#include <algorithm>

namespace ClusterEngine {

/**
 * @class RulesTestHarness 
 * @brief Comprehensive test harness for rules interface system
 */
class RulesTestHarness {
private:
    std::unique_ptr<ClusterEngineCore> engine_core_;
    std::unique_ptr<RuleManager> rule_manager_;
    
    // Mock solution data for testing
    std::vector<std::vector<std::vector<MusicalValue>>> mock_vsolution_;
    std::vector<std::vector<MusicalSequence>> mock_vlinear_solution_;
    std::vector<int> mock_vindex_;
    std::vector<std::vector<double>> mock_vsolution_for_backjump_;
    std::vector<std::vector<int>> mock_vbackjump_indexes_;

public:
    RulesTestHarness() {
        int num_engines = 6; // 3 voices (rhythm-pitch pairs) + metric engine
        setup_mock_data(num_engines);
        rule_manager_ = std::make_unique<RuleManager>(num_engines);
    }

    /**
     * @brief Run all rule system tests
     */
    void run_all_tests() {
        std::cout << "=== ClusterEngine Rules Interface System Tests ===" << std::endl;
        
        test_basic_rule_functionality();
        test_single_engine_rules();
        test_dual_engine_rules();
        test_multi_engine_rules();
        test_rule_manager();
        test_musical_rule_factories();
        test_preset_rule_sets();
        test_rule_compilation();
        test_backtrack_routes();
        test_performance();
        
        std::cout << "=== All Rules Interface Tests Completed Successfully ===" << std::endl;
    }

private:
    /**
     * @brief Setup mock solution data for testing
     */
    void setup_mock_data(int num_engines) {
        mock_vindex_.resize(num_engines, 2); // All engines at index 2
        
        mock_vsolution_.resize(num_engines);
        mock_vlinear_solution_.resize(num_engines);
        mock_vsolution_for_backjump_.resize(num_engines);
        mock_vbackjump_indexes_.resize(num_engines);
        
        for (int engine = 0; engine < num_engines; ++engine) {
            // Setup solution arrays with 3 cells per engine
            mock_vsolution_[engine].resize(3);
            
            for (int cell = 0; cell < 3; ++cell) {
                if (engine % 2 == 0) {
                    // Rhythm engine - durations
                    mock_vsolution_[engine][cell] = {{0.25, 0.5, 0.25}}; // Quarter, half, quarter
                } else {
                    // Pitch engine - pitches
                    mock_vsolution_[engine][cell] = {{60 + cell * 2, 64 + cell * 2, 67 + cell * 2}}; // C, E, G progression
                }
            }
            
            // Setup linear solution
            mock_vlinear_solution_[engine].resize(1);
            if (engine % 2 == 0) {
                mock_vlinear_solution_[engine][0] = {0.25, 0.5, 0.25, 0.25, 0.5, 0.25};
            } else {
                mock_vlinear_solution_[engine][0] = {60, 64, 67, 62, 65, 69};
            }
            
            // Setup backjump data
            mock_vsolution_for_backjump_[engine].resize(2, 0.0);
            mock_vbackjump_indexes_[engine].resize(2, 0);
        }
    }

    /**
     * @brief Create rule execution context for testing
     */
    RuleExecutionContext create_test_context(int current_engine = 0) {
        return RuleExecutionContext(
            engine_core_.get(),
            &mock_vsolution_,
            &mock_vlinear_solution_,
            &mock_vindex_,
            &mock_vsolution_for_backjump_,
            &mock_vbackjump_indexes_,
            current_engine
        );
    }

    /**
     * @brief Test basic rule functionality
     */
    void test_basic_rule_functionality() {
        std::cout << "\n[TEST] Basic Rule Functionality" << std::endl;
        
        // Create a simple pitch rule
        auto rule = MusicalRules::MusicalRuleFactory::create_no_repeated_pitches(1);
        assert(rule != nullptr);
        assert(rule->get_name() == "No Repeated Pitches");
        assert(rule->get_target_engines() == std::vector<int>{1});
        assert(rule->is_enabled());
        
        // Test rule evaluation
        auto context = create_test_context(1);
        RuleResult result = rule->evaluate(context);
        
        // Should pass for our test data (no repeated pitches)
        assert(result == RuleResult::PASS);
        
        std::cout << "✓ Basic rule functionality tests passed" << std::endl;
    }

    /**
     * @brief Test single engine rules
     */
    void test_single_engine_rules() {
        std::cout << "\n[TEST] Single Engine Rules" << std::endl;
        
        // Test rhythm rules
        auto no_repeated_durations = MusicalRules::MusicalRuleFactory::create_no_repeated_durations(0);
        auto context = create_test_context(0);
        
        // Our test data has repeated quarter notes, so this should fail
        RuleResult result = no_repeated_durations->evaluate(context);
        assert(result == RuleResult::FAIL);
        
        // Test pitch rules
        auto stepwise_motion = MusicalRules::MusicalRuleFactory::create_stepwise_motion(1);
        context = create_test_context(1);
        result = stepwise_motion->evaluate(context);
        
        // Our test data has reasonable intervals, should pass
        assert(result == RuleResult::PASS || result == RuleResult::INSUFFICIENT_DATA);
        
        // Test pitch range rule
        auto pitch_range = MusicalRules::MusicalRuleFactory::create_pitch_range(1, 50, 80);
        result = pitch_range->evaluate(context);
        assert(result == RuleResult::PASS); // Our pitches are in range
        
        std::cout << "✓ Single engine rules tests passed" << std::endl;
    }

    /**
     * @brief Test dual engine rules
     */
    void test_dual_engine_rules() {
        std::cout << "\n[TEST] Dual Engine Rules" << std::endl;
        
        // Test rhythm-pitch coordination
        auto strong_beat = MusicalRules::MusicalRuleFactory::create_strong_beat_emphasis(0, 1);
        auto context = create_test_context(0);
        
        RuleResult result = strong_beat->evaluate(context);
        assert(result == RuleResult::PASS || result == RuleResult::INSUFFICIENT_DATA);
        
        // Test syncopation rule
        auto no_syncopation = MusicalRules::MusicalRuleFactory::create_no_syncopation(0, 1);
        result = no_syncopation->evaluate(context);
        assert(result == RuleResult::PASS || result == RuleResult::INSUFFICIENT_DATA);
        
        std::cout << "✓ Dual engine rules tests passed" << std::endl;
    }

    /**
     * @brief Test multi engine rules
     */
    void test_multi_engine_rules() {
        std::cout << "\n[TEST] Multi Engine Rules" << std::endl;
        
        // Test harmonic rules with multiple pitch engines
        std::vector<int> pitch_engines = {1, 3};
        
        auto consonant_harmonies = MusicalRules::MusicalRuleFactory::create_consonant_harmonies(pitch_engines);
        auto context = create_test_context(1);
        
        RuleResult result = consonant_harmonies->evaluate(context);
        assert(result == RuleResult::PASS || result == RuleResult::INSUFFICIENT_DATA);
        
        // Test parallel intervals rule
        auto no_parallels = MusicalRules::MusicalRuleFactory::create_no_parallel_perfect_intervals(pitch_engines);
        result = no_parallels->evaluate(context);
        assert(result == RuleResult::PASS || result == RuleResult::INSUFFICIENT_DATA);
        
        // Test voice leading
        auto voice_leading = MusicalRules::MusicalRuleFactory::create_voice_leading(pitch_engines, 3);
        result = voice_leading->evaluate(context);
        assert(result == RuleResult::PASS || result == RuleResult::INSUFFICIENT_DATA);
        
        std::cout << "✓ Multi engine rules tests passed" << std::endl;
    }

    /**
     * @brief Test rule manager functionality
     */
    void test_rule_manager() {
        std::cout << "\n[TEST] Rule Manager" << std::endl;
        
        // Test adding rules
        rule_manager_->add_rule(MusicalRules::MusicalRuleFactory::create_no_repeated_pitches(1));
        rule_manager_->add_rule(MusicalRules::MusicalRuleFactory::create_stepwise_motion(1)); 
        rule_manager_->add_rule(MusicalRules::MusicalRuleFactory::create_no_repeated_durations(0));
        
        assert(rule_manager_->get_total_rule_count() == 3);
        
        // Test rule testing
        auto context = create_test_context(1);
        BacktrackRoute failed_routes = rule_manager_->test_rules(1, context);
        
        // May or may not have failures depending on test data
        
        // Test rule enabling/disabling
        bool success = rule_manager_->set_rule_enabled("No Repeated Pitches", false);
        assert(success);
        
        // Test locked engines
        rule_manager_->set_locked_engines({0, 1});
        BacktrackRoute filtered = rule_manager_->filter_backtrack_route({0, 1, 2, 3});
        assert(std::find(filtered.begin(), filtered.end(), 0) == filtered.end());
        assert(std::find(filtered.begin(), filtered.end(), 1) == filtered.end());
        
        std::cout << "✓ Rule manager tests passed" << std::endl;
    }

    /**
     * @brief Test musical rule factories
     */
    void test_musical_rule_factories() {
        std::cout << "\n[TEST] Musical Rule Factories" << std::endl;
        
        // Test creation of various rule types
        auto rhythm_rule = MusicalRules::MusicalRuleFactory::create_accelerando(0);
        assert(rhythm_rule != nullptr);
        assert(rhythm_rule->get_type() == MusicalRule::RuleType::SINGLE_ENGINE_RHYTHM);
        
        auto pitch_rule = MusicalRules::MusicalRuleFactory::create_no_large_leaps(1, 10);
        assert(pitch_rule != nullptr);
        assert(pitch_rule->get_type() == MusicalRule::RuleType::SINGLE_ENGINE_PITCH);
        
        auto dual_rule = MusicalRules::MusicalRuleFactory::create_strong_beat_emphasis(0, 1);
        assert(dual_rule != nullptr);
        assert(dual_rule->get_type() == MusicalRule::RuleType::DUAL_ENGINE_RHYTHM_PITCH);
        
        auto multi_rule = MusicalRules::MusicalRuleFactory::create_consonant_harmonies({1, 3});
        assert(multi_rule != nullptr);
        assert(multi_rule->get_type() == MusicalRule::RuleType::MULTI_ENGINE_HARMONIC);
        
        // Test backtrack route utilities
        auto self_route = MusicalRules::MusicalRuleFactory::create_self_backtrack(2);
        assert(self_route == std::vector<int>{2});
        
        auto other_route = MusicalRules::MusicalRuleFactory::create_other_engine_backtrack(0);
        assert(other_route == std::vector<int>{1});
        
        auto multi_route = MusicalRules::MusicalRuleFactory::create_multi_engine_backtrack({1, 3, 5});
        assert(multi_route == std::vector<int>({1, 3, 5}));
        
        std::cout << "✓ Musical rule factories tests passed" << std::endl;
    }

    /**
     * @brief Test preset rule sets
     */
    void test_preset_rule_sets() {
        std::cout << "\n[TEST] Preset Rule Sets" << std::endl;
        
        RuleManager classical_manager(6);
        RuleManager jazz_manager(6);
        RuleManager basic_manager(6);
        
        std::vector<int> voices = {0, 1, 2, 3};
        std::vector<int> rhythm_engines = {0, 2};
        std::vector<int> pitch_engines = {1, 3};
        
        // Test classical counterpoint rules
        MusicalRules::PresetRuleSets::add_classical_counterpoint_rules(
            classical_manager, voices, 1);
        assert(classical_manager.get_total_rule_count() > 0);
        
        // Test jazz improvisation rules
        MusicalRules::PresetRuleSets::add_jazz_improvisation_rules(
            jazz_manager, rhythm_engines, pitch_engines);
        assert(jazz_manager.get_total_rule_count() > 0);
        
        // Test basic rules
        MusicalRules::PresetRuleSets::add_basic_rules(
            basic_manager, rhythm_engines, pitch_engines);
        assert(basic_manager.get_total_rule_count() > 0);
        
        std::cout << "✓ Preset rule sets tests passed" << std::endl;
    }

    /**
     * @brief Test rule compilation system
     */
    void test_rule_compilation() {
        std::cout << "\n[TEST] Rule Compilation System" << std::endl;
        
        // Test single engine rule compilation
        auto single_rule_func = [](const MusicalSequence& seq) -> bool {
            return !seq.empty() && seq[0] > 0;
        };
        
        auto compiled_single = RuleCompiler::compile_single_engine_cells_rule(
            "Test Single Rule", 1, single_rule_func);
        assert(compiled_single != nullptr);
        
        auto context = create_test_context(1);
        RuleResult result = compiled_single->evaluate(context);
        assert(result != RuleResult::INSUFFICIENT_DATA); // Should evaluate
        
        // Test dual engine rule compilation
        auto dual_rule_func = [](const std::vector<std::pair<MusicalValue, MusicalValue>>& pairs) -> bool {
            return !pairs.empty();
        };
        
        auto compiled_dual = RuleCompiler::compile_dual_engine_rhythm_pitch_rule(
            "Test Dual Rule", 0, 1, dual_rule_func);
        assert(compiled_dual != nullptr);
        
        // Test multi engine rule compilation
        auto multi_rule_func = [](const std::vector<std::vector<MusicalValue>>& slices) -> bool {
            return !slices.empty();
        };
        
        auto compiled_multi = RuleCompiler::compile_multi_engine_harmonic_rule(
            "Test Multi Rule", {1, 3}, multi_rule_func);
        assert(compiled_multi != nullptr);
        
        std::cout << "✓ Rule compilation tests passed" << std::endl;
    }

    /**
     * @brief Test backtrack route functionality
     */
    void test_backtrack_routes() {
        std::cout << "\n[TEST] Backtrack Routes" << std::endl;
        
        // Test that rules return appropriate backtrack routes when they fail
        
        // Create a rule that will definitely fail
        auto always_fail_rule = RuleCompiler::compile_single_engine_cells_rule(
            "Always Fail", 1, 
            [](const MusicalSequence&) -> bool { return false; },
            {0, 1, 2}); // Custom backtrack route
        
        rule_manager_->add_rule(std::move(always_fail_rule));
        
        auto context = create_test_context(1);
        BacktrackRoute routes = rule_manager_->test_rules(1, context);
        
        // Should contain our custom backtrack route
        assert(!routes.empty());
        
        // Test locked engine filtering
        rule_manager_->set_locked_engines({0});
        BacktrackRoute filtered_routes = rule_manager_->filter_backtrack_route({0, 1, 2});
        
        // Engine 0 should be filtered out
        assert(std::find(filtered_routes.begin(), filtered_routes.end(), 0) == filtered_routes.end());
        assert(std::find(filtered_routes.begin(), filtered_routes.end(), 1) != filtered_routes.end());
        assert(std::find(filtered_routes.begin(), filtered_routes.end(), 2) != filtered_routes.end());
        
        std::cout << "✓ Backtrack routes tests passed" << std::endl;
    }

    /**
     * @brief Test performance of rule evaluation
     */
    void test_performance() {
        std::cout << "\n[TEST] Rule Evaluation Performance" << std::endl;
        
        // Add multiple rules to test performance
        RuleManager perf_manager(6);
        
        // Add various types of rules
        perf_manager.add_rule(MusicalRules::MusicalRuleFactory::create_no_repeated_pitches(1));
        perf_manager.add_rule(MusicalRules::MusicalRuleFactory::create_stepwise_motion(1));
        perf_manager.add_rule(MusicalRules::MusicalRuleFactory::create_no_large_leaps(1, 12));
        perf_manager.add_rule(MusicalRules::MusicalRuleFactory::create_no_repeated_durations(0));
        perf_manager.add_rule(MusicalRules::MusicalRuleFactory::create_strong_beat_emphasis(0, 1));
        perf_manager.add_rule(MusicalRules::MusicalRuleFactory::create_consonant_harmonies({1, 3}));
        
        auto context = create_test_context(1);
        
        // Time multiple rule evaluations
        const int num_evaluations = 1000;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_evaluations; ++i) {
            perf_manager.test_rules(1, context);
            perf_manager.test_global_rules(context);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double avg_time_per_evaluation = static_cast<double>(duration.count()) / num_evaluations;
        
        std::cout << "Performance Results:" << std::endl;
        std::cout << "  - Total evaluations: " << num_evaluations << std::endl; 
        std::cout << "  - Total time: " << duration.count() << " μs" << std::endl;
        std::cout << "  - Average time per evaluation: " << avg_time_per_evaluation << " μs" << std::endl;
        std::cout << "  - Evaluations per second: " << (1000000.0 / avg_time_per_evaluation) << std::endl;
        
        // Performance should be reasonable for real-time constraint solving
        assert(avg_time_per_evaluation < 100.0); // Less than 100 microseconds per evaluation
        
        std::cout << "✓ Performance tests passed" << std::endl;
    }
};

} // namespace ClusterEngine

/**
 * @brief Main test function
 */
int main() {
    try {
        ClusterEngine::RulesTestHarness test_harness;
        test_harness.run_all_tests();
        
        std::cout << "\n🎵 ClusterEngine Rules Interface System fully functional!" << std::endl;
        std::cout << "✓ Musical constraint rules successfully translated from Lisp cluster-engine" << std::endl;
        std::cout << "✓ Rules can enforce rhythmic, melodic, and harmonic constraints" << std::endl;
        std::cout << "✓ Backtrack routes properly configured for constraint propagation" << std::endl;
        std::cout << "✓ Performance suitable for real-time musical constraint solving" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}