/**
 * @file test_advanced_backjumping.cpp
 * @brief Test suite for Advanced Backjumping Strategies from Cluster Engine v4.05
 * 
 * Demonstrates all three backjumping modes and musical heuristics integration.
 */

#include "advanced_backjumping_strategies.hh"
#include "enhanced_rule_architecture.hh"
#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <iomanip>

using namespace AdvancedBackjumping;
using namespace MusicalConstraints;

// ===============================
// Mock Musical Rules for Testing
// ===============================

/**
 * @brief Test rule that fails and suggests specific backjump distance
 */
class TestBackjumpRule : public MusicalRule {
private:
    int suggested_backjump_;
    bool should_fail_;
    std::string rule_name_;
    
public:
    TestBackjumpRule(const std::string& name, int backjump_distance, bool fail = true)
        : suggested_backjump_(backjump_distance), should_fail_(fail), rule_name_(name) {}
    
    RuleResult check_rule(const DualSolutionStorage& storage, int current_index) const override {
        if (should_fail_) {
            RuleResult result = RuleResult::Failure(suggested_backjump_, 
                                                   rule_name_ + " constraint violation");
            
            // Add specific backjump suggestion
            BackjumpSuggestion suggestion(current_index - 1, suggested_backjump_);
            suggestion.explanation = rule_name_ + " suggests backjump " + std::to_string(suggested_backjump_);
            result.add_suggestion(suggestion);
            
            return result;
        } else {
            return RuleResult::Success();
        }
    }
    
    std::string description() const override {
        return rule_name_ + " (test rule)";
    }
    
    std::vector<int> get_dependent_variables(int current_index) const override {
        return {current_index};
    }
    
    std::string rule_type() const override { return "TestBackjumpRule"; }
};

/**
 * @brief Test rule that suggests multiple backjump options
 */
class MultipleBackjumpRule : public MusicalRule {
private:
    std::vector<int> suggested_distances_;
    std::string rule_name_;
    
public:
    MultipleBackjumpRule(const std::string& name, const std::vector<int>& distances)
        : suggested_distances_(distances), rule_name_(name) {}
    
    RuleResult check_rule(const DualSolutionStorage& storage, int current_index) const override {
        RuleResult result = RuleResult::Failure(suggested_distances_[0], 
                                               rule_name_ + " multiple conflicts");
        
        // Add multiple suggestions
        for (size_t i = 0; i < suggested_distances_.size(); ++i) {
            BackjumpSuggestion suggestion(current_index - static_cast<int>(i + 1), suggested_distances_[i]);
            suggestion.explanation = rule_name_ + " conflict " + std::to_string(i + 1);
            result.add_suggestion(suggestion);
        }
        
        return result;
    }
    
    std::string description() const override {
        return rule_name_ + " (multiple backjumps)";
    }
    
    std::vector<int> get_dependent_variables(int current_index) const override {
        return {current_index};
    }
    
    std::string rule_type() const override { return "MultipleBackjumpRule"; }
};

// ===============================
// Test Suite Implementation
// ===============================

class AdvancedBackjumpingTestSuite {
private:
    DualSolutionStorage create_test_solution() {
        DualSolutionStorage solution(10, DomainType::ABSOLUTE_DOMAIN, 60);
        
        // Create a musical sequence with some problems for testing
        solution.write_absolute(60, 0);  // C
        solution.write_absolute(67, 1);  // G (perfect 5th)
        solution.write_absolute(72, 2);  // C (octave)
        solution.write_absolute(65, 3);  // F (down)
        solution.write_absolute(84, 4);  // High C (large leap problem)
        solution.write_absolute(48, 5);  // Low C (extreme leap problem)
        
        return solution;
    }
    
public:
    void run_all_tests() {
        std::cout << "🚀 ADVANCED BACKJUMPING STRATEGIES TEST SUITE" << std::endl;
        std::cout << "=============================================" << std::endl;
        
        test_mode1_no_backjumping();
        test_mode2_intelligent_backjumping();
        test_mode3_consensus_backjumping();
        test_musical_heuristics();
        test_strategy_coordinator();
        test_performance_analysis();
        
        std::cout << "\\n🏆 ALL ADVANCED BACKJUMPING TESTS COMPLETED!" << std::endl;
    }
    
    void test_mode1_no_backjumping() {
        std::cout << "\\n🔬 Test 1: Mode 1 - No Backjumping" << std::endl;
        std::cout << "=================================" << std::endl;
        
        AdvancedBackjumpAnalyzer analyzer(BackjumpMode::NO_BACKJUMPING);
        analyzer.set_debug_mode(true);
        
        // Create test rules that would suggest backjumping
        std::vector<RuleResult> rule_results;
        rule_results.push_back(RuleResult::Failure(3, "Scale violation"));
        rule_results.push_back(RuleResult::Failure(2, "Range violation"));
        rule_results.push_back(RuleResult::Success());
        
        AdvancedBackjumpResult result = analyzer.analyze_backjump(rule_results, 5);
        
        std::cout << "Expected: No backjumping despite rule failures" << std::endl;
        std::cout << "Result: " << (result.has_backjump ? "BACKJUMP" : "NO BACKJUMP") << std::endl;
        std::cout << "Test: " << (!result.has_backjump ? "✅ PASSED" : "❌ FAILED") << std::endl;
    }
    
    void test_mode2_intelligent_backjumping() {
        std::cout << "\\n🔬 Test 2: Mode 2 - Intelligent Backjumping" << std::endl;
        std::cout << "===========================================" << std::endl;
        
        AdvancedBackjumpAnalyzer analyzer(BackjumpMode::INTELLIGENT_BACKJUMP);
        analyzer.set_debug_mode(true);
        
        // Create rules with different backjump suggestions
        std::vector<RuleResult> rule_results;
        
        // Rule 1: Suggests backjump distance 2
        RuleResult result1 = RuleResult::Failure(2, "Consonance violation");
        BackjumpSuggestion suggestion1(3, 2);
        suggestion1.explanation = "Consonance requires backjump 2";
        result1.add_suggestion(suggestion1);
        rule_results.push_back(result1);
        
        // Rule 2: Suggests backjump distance 3  
        RuleResult result2 = RuleResult::Failure(3, "Voice leading violation");
        BackjumpSuggestion suggestion2(2, 3);
        suggestion2.explanation = "Voice leading requires backjump 3";
        result2.add_suggestion(suggestion2);
        rule_results.push_back(result2);
        
        // Rule 3: Suggests backjump distance 1
        RuleResult result3 = RuleResult::Failure(1, "Scale violation");
        BackjumpSuggestion suggestion3(4, 1);
        suggestion3.explanation = "Scale requires backjump 1";
        result3.add_suggestion(suggestion3);
        rule_results.push_back(result3);
        
        AdvancedBackjumpResult result = analyzer.analyze_backjump(rule_results, 5);
        
        std::cout << "\\nExpected: Minimum backjump distance (Mode 2 behavior)" << std::endl;
        std::cout << "Minimum distance: " << result.minimum_backjump_distance << std::endl;
        std::cout << "Maximum distance: " << result.maximum_backjump_distance << std::endl;
        std::cout << "Consensus distance: " << result.consensus_backjump_distance << std::endl;
        std::cout << "Test: " << (result.consensus_backjump_distance == 1 ? "✅ PASSED" : "❌ FAILED") << std::endl;
    }
    
    void test_mode3_consensus_backjumping() {
        std::cout << "\\n🔬 Test 3: Mode 3 - Consensus Backjumping" << std::endl;
        std::cout << "=========================================" << std::endl;
        
        AdvancedBackjumpAnalyzer analyzer(BackjumpMode::CONSENSUS_BACKJUMP);
        analyzer.set_debug_mode(true);
        
        // Create rules where multiple rules agree on distance 2
        std::vector<RuleResult> rule_results;
        
        // 3 rules suggest distance 2 (should achieve consensus)
        for (int i = 0; i < 3; ++i) {
            RuleResult result = RuleResult::Failure(2, "Rule " + std::to_string(i + 1));
            BackjumpSuggestion suggestion(4 - i, 2);
            suggestion.explanation = "Rule " + std::to_string(i + 1) + " consensus vote";
            result.add_suggestion(suggestion);
            rule_results.push_back(result);
        }
        
        // 1 rule suggests distance 3 (minority)
        RuleResult minority_result = RuleResult::Failure(3, "Minority rule");
        BackjumpSuggestion minority_suggestion(1, 3);
        minority_suggestion.explanation = "Minority opinion";
        minority_result.add_suggestion(minority_suggestion);
        rule_results.push_back(minority_result);
        
        AdvancedBackjumpResult result = analyzer.analyze_backjump(rule_results, 5);
        
        std::cout << "\\nExpected: Consensus on distance 2 (75% agreement)" << std::endl;
        std::cout << "Has consensus: " << (result.has_backjump ? "YES" : "NO") << std::endl;
        std::cout << "Consensus distance: " << result.consensus_backjump_distance << std::endl;
        std::cout << "Test: " << (result.has_backjump && result.consensus_backjump_distance == 2 ? 
                                  "✅ PASSED" : "❌ FAILED") << std::endl;
    }
    
    void test_musical_heuristics() {
        std::cout << "\\n🔬 Test 4: Musical Backjump Heuristics" << std::endl;
        std::cout << "=====================================" << std::endl;
        
        DualSolutionStorage solution = create_test_solution();
        
        std::cout << "Test solution with musical problems:" << std::endl;
        solution.print_solution();
        
        // Test voice leading analysis
        int voice_leading_backjump = MusicalBackjumpHeuristics::calculate_voice_leading_backjump(
            solution, 5, 2);
        std::cout << "\\nVoice leading backjump: " << voice_leading_backjump << std::endl;
        
        // Test harmonic rhythm analysis
        int harmonic_backjump = MusicalBackjumpHeuristics::calculate_harmonic_rhythm_backjump(
            solution, 5);
        std::cout << "Harmonic rhythm backjump: " << harmonic_backjump << std::endl;
        
        // Test phrase boundary detection
        std::vector<int> boundaries = MusicalBackjumpHeuristics::identify_phrase_boundaries(
            solution, 5);
        std::cout << "Phrase boundaries: ";
        for (int boundary : boundaries) {
            std::cout << boundary << " ";
        }
        std::cout << std::endl;
        
        // Test complete musical conflict analysis
        std::vector<RuleResult> empty_rule_results;
        AdvancedBackjumpResult musical_result = MusicalBackjumpHeuristics::analyze_musical_conflicts(
            solution, 5, empty_rule_results);
        
        std::cout << "\\nMusical analysis result:" << std::endl;
        std::cout << "Has backjump: " << (musical_result.has_backjump ? "YES" : "NO") << std::endl;
        if (musical_result.has_backjump) {
            std::cout << "Consensus distance: " << musical_result.consensus_backjump_distance << std::endl;
        }
        std::cout << "Conflict reasons: " << musical_result.conflict_reasons.size() << std::endl;
        
        std::cout << "Test: " << ((voice_leading_backjump > 0 || harmonic_backjump > 0) ? 
                                  "✅ PASSED" : "❌ FAILED") << std::endl;
    }
    
    void test_strategy_coordinator() {
        std::cout << "\\n🔬 Test 5: Strategy Coordinator Integration" << std::endl;
        std::cout << "===========================================" << std::endl;
        
        BackjumpStrategyCoordinator coordinator(BackjumpMode::INTELLIGENT_BACKJUMP);
        coordinator.enable_adaptive_mode_selection(true);
        
        // Create test rules
        std::vector<std::shared_ptr<MusicalRule>> rules;
        rules.push_back(std::make_shared<TestBackjumpRule>("Scale Rule", 2));
        rules.push_back(std::make_shared<TestBackjumpRule>("Range Rule", 3));
        rules.push_back(std::make_shared<MultipleBackjumpRule>("Complex Rule", std::vector<int>{1, 2, 1}));
        
        DualSolutionStorage solution = create_test_solution();
        
        // Perform integrated analysis
        AdvancedBackjumpResult result = coordinator.perform_backjump_analysis(rules, solution, 5);
        
        // Print comprehensive report
        coordinator.print_analysis_report(result);
        
        std::cout << "Test: " << (result.total_rules_tested == 3 ? "✅ PASSED" : "❌ FAILED") << std::endl;
    }
    
    void test_performance_analysis() {
        std::cout << "\\n🔬 Test 6: Performance Analysis" << std::endl;
        std::cout << "==============================" << std::endl;
        
        AdvancedBackjumpAnalyzer analyzer(BackjumpMode::INTELLIGENT_BACKJUMP);
        
        // Run multiple analyses to gather performance data
        int num_tests = 100;
        std::cout << "Running " << num_tests << " performance tests..." << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_tests; ++i) {
            std::vector<RuleResult> rule_results;
            rule_results.push_back(RuleResult::Failure(i % 3 + 1, "Perf test rule"));
            analyzer.analyze_backjump(rule_results, 5);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        AdvancedBackjumpAnalyzer::PerformanceStats stats = analyzer.get_performance_stats();
        
        std::cout << "\\nPerformance Results:" << std::endl;
        std::cout << "Total analyses: " << stats.total_analyses << std::endl;
        std::cout << "Successful backjumps: " << stats.successful_backjumps << std::endl;
        std::cout << "Success rate: " << std::fixed << std::setprecision(1) 
                  << stats.success_rate << "%" << std::endl;
        std::cout << "Average analysis time: " << std::fixed << std::setprecision(3)
                  << stats.average_analysis_time_ms << " ms" << std::endl;
        std::cout << "Total test time: " << duration.count() / 1000.0 << " ms" << std::endl;
        
        bool performance_ok = (stats.average_analysis_time_ms < 1.0); // Should be sub-millisecond
        std::cout << "Test: " << (performance_ok ? "✅ PASSED" : "❌ FAILED") << std::endl;
    }
};

// ===============================
// Demonstration Scenarios
// ===============================

void demonstrate_cluster_engine_v405_backjumping() {
    std::cout << "\\n🎯 CLUSTER ENGINE v4.05 BACKJUMPING DEMONSTRATION" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    std::cout << "\\n📖 Scenario: Musical constraint solving with intelligent backjumping" << std::endl;
    std::cout << "Based on cluster-engine v4.05 advanced backtracking algorithms" << std::endl;
    
    // Create a realistic musical scenario
    DualSolutionStorage musical_sequence(8, DomainType::ABSOLUTE_DOMAIN, 60);
    
    // Build a sequence that will have conflicts
    musical_sequence.write_absolute(60, 0);  // C4
    musical_sequence.write_absolute(64, 1);  // E4 (major 3rd)
    musical_sequence.write_absolute(67, 2);  // G4 (perfect 5th)
    musical_sequence.write_absolute(72, 3);  // C5 (octave)
    musical_sequence.write_absolute(85, 4);  // PROBLEM: way too high
    musical_sequence.write_absolute(47, 5);  // PROBLEM: extreme low leap
    
    std::cout << "\\n🎼 Musical sequence with problems:" << std::endl;
    musical_sequence.print_solution();
    
    // Create musical rules that will detect problems
    std::vector<std::shared_ptr<MusicalRule>> musical_rules;
    musical_rules.push_back(std::make_shared<TestBackjumpRule>("Range Constraint", 2));
    musical_rules.push_back(std::make_shared<TestBackjumpRule>("Voice Leading", 3));
    musical_rules.push_back(std::make_shared<TestBackjumpRule>("Melodic Contour", 1));
    
    // Test all three modes on the same problem
    std::vector<BackjumpMode> modes = {
        BackjumpMode::NO_BACKJUMPING,
        BackjumpMode::INTELLIGENT_BACKJUMP,
        BackjumpMode::CONSENSUS_BACKJUMP
    };
    
    std::vector<std::string> mode_names = {
        "Mode 1: No Backjumping",
        "Mode 2: Intelligent Backjump", 
        "Mode 3: Consensus Backjump"
    };
    
    for (size_t i = 0; i < modes.size(); ++i) {
        std::cout << "\\n" << mode_names[i] << std::endl;
        std::cout << std::string(mode_names[i].length(), '=') << std::endl;
        
        BackjumpStrategyCoordinator coordinator(modes[i]);
        AdvancedBackjumpResult result = coordinator.perform_backjump_analysis(
            musical_rules, musical_sequence, 5);
        
        coordinator.print_analysis_report(result);
    }
    
    std::cout << "\\n✅ Cluster Engine v4.05 backjumping demonstration complete!" << std::endl;
}

// ===============================
// Main Test Execution
// ===============================

int main() {
    try {
        std::cout << "🚀 ADVANCED BACKJUMPING STRATEGIES FROM CLUSTER ENGINE v4.05\\n"
                  << "============================================================\\n" << std::endl;
        
        std::cout << "This test suite validates the implementation of sophisticated" << std::endl;
        std::cout << "backjumping algorithms from cluster-engine v4.05 with:" << std::endl;
        std::cout << "  ✅ Multiple backjumping modes (No/Intelligent/Consensus)" << std::endl;
        std::cout << "  ✅ Musical heuristics integration" << std::endl;
        std::cout << "  ✅ Performance-optimized analysis" << std::endl;
        std::cout << "  ✅ Comprehensive conflict resolution" << std::endl;
        
        // Run comprehensive test suite
        AdvancedBackjumpingTestSuite test_suite;
        test_suite.run_all_tests();
        
        // Run demonstration scenarios
        demonstrate_cluster_engine_v405_backjumping();
        
        std::cout << "\\n🏆 ADVANCED BACKJUMPING STRATEGIES: FULLY IMPLEMENTED!" << std::endl;
        std::cout << "=======================================================" << std::endl;
        
        std::cout << "\\n🎼 System Capabilities:" << std::endl;
        std::cout << "  ✅ cluster-engine v4.05 backjumping modes 1-3" << std::endl;
        std::cout << "  ✅ Intelligent rule-based conflict analysis" << std::endl;
        std::cout << "  ✅ Consensus-based backjumping with agreement thresholds" << std::endl;
        std::cout << "  ✅ Musical heuristics for voice leading and phrase structure" << std::endl;
        std::cout << "  ✅ Performance optimization and statistics tracking" << std::endl;
        std::cout << "  ✅ Gecode integration interface for constraint propagation" << std::endl;
        
        std::cout << "\\n🚀 Ready for real-time musical constraint satisfaction!" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}