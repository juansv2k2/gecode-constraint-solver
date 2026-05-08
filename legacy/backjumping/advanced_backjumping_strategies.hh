/**
 * @file advanced_backjumping_strategies.hh
 * @brief Advanced Backjumping Strategies from Cluster Engine v4.05
 * 
 * This implements the sophisticated backjumping system from cluster-engine v4.05
 * with multiple backjumping modes and intelligent rule analysis.
 */

#ifndef ADVANCED_BACKJUMPING_STRATEGIES_HH
#define ADVANCED_BACKJUMPING_STRATEGIES_HH

#include "enhanced_rule_architecture.hh"
#include "dual_solution_storage.hh"
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <iostream>
#include <chrono>

namespace AdvancedBackjumping {

/**
 * @brief Backjumping modes from cluster-engine v4.05
 */
enum class BackjumpMode : int {
    NO_BACKJUMPING = 1,      ///< Simple backtracking, no intelligence
    INTELLIGENT_BACKJUMP = 2, ///< Rule-based intelligent backjumping  
    CONSENSUS_BACKJUMP = 3    ///< All rules must agree on backjump distance
};

/**
 * @brief Advanced backjump analysis result
 * 
 * Encapsulates the complete analysis from multiple rules with
 * sophisticated conflict detection and resolution.
 */
struct AdvancedBackjumpResult {
    bool has_backjump;                    ///< Whether backjumping is needed
    int minimum_backjump_distance;        ///< Minimum distance to backjump
    int maximum_backjump_distance;        ///< Maximum distance suggested
    int consensus_backjump_distance;      ///< Agreed-upon distance (mode 3)
    
    std::vector<int> conflicting_variables; ///< Variables causing conflicts
    std::vector<std::string> conflict_reasons; ///< Why each conflict occurred
    
    // Rule-specific analysis
    int rules_suggesting_backjump;        ///< Number of rules suggesting backjump
    int total_rules_tested;              ///< Total number of rules evaluated
    
    // Performance metrics
    std::chrono::nanoseconds analysis_time; ///< Time taken for analysis
    
    AdvancedBackjumpResult() 
        : has_backjump(false), minimum_backjump_distance(0), 
          maximum_backjump_distance(0), consensus_backjump_distance(0),
          rules_suggesting_backjump(0), total_rules_tested(0) {}
};

/**
 * @brief Advanced backjump analyzer implementing cluster-engine v4.05 strategies
 * 
 * This class implements the sophisticated backjumping algorithms from
 * cluster-engine v4.05, providing multiple analysis modes and intelligent
 * conflict resolution for musical constraint satisfaction.
 */
class AdvancedBackjumpAnalyzer {
private:
    BackjumpMode current_mode_;
    bool debug_mode_;
    
    // Performance tracking
    size_t total_analyses_;
    size_t successful_backjumps_;
    double average_analysis_time_;
    
public:
    /**
     * @brief Construct backjump analyzer with specified mode
     */
    explicit AdvancedBackjumpAnalyzer(BackjumpMode mode = BackjumpMode::INTELLIGENT_BACKJUMP) 
        : current_mode_(mode), debug_mode_(false), total_analyses_(0), 
          successful_backjumps_(0), average_analysis_time_(0.0) {}
    
    /**
     * @brief Set backjumping mode
     */
    void set_mode(BackjumpMode mode) { current_mode_ = mode; }
    
    /**
     * @brief Enable/disable debug output
     */
    void set_debug_mode(bool debug) { debug_mode_ = debug; }
    
    /**
     * @brief Analyze backjump from test result (cluster-engine v4.05 mode 2)
     * 
     * Implements analyze-backjump-from-test-result2 from cluster-engine v4.05:
     * Collects minimum backjump distance from failed rules.
     */
    AdvancedBackjumpResult analyze_backjump_mode2(
        const std::vector<MusicalConstraints::RuleResult>& rule_results,
        int current_variable_index) const;
    
    /**
     * @brief Consensus backjump analysis (cluster-engine v4.05 mode 3)
     * 
     * Implements analyze-backjump-from-test-result3 from cluster-engine v4.05:
     * All rules must agree on backjump distance for consensus.
     */
    AdvancedBackjumpResult analyze_backjump_mode3(
        const std::vector<MusicalConstraints::RuleResult>& rule_results,
        int current_variable_index) const;
    
    /**
     * @brief Complete backjump analysis using current mode
     * 
     * Main entry point that routes to appropriate analysis method
     * based on current backjump mode.
     */
    AdvancedBackjumpResult analyze_backjump(
        const std::vector<MusicalConstraints::RuleResult>& rule_results,
        int current_variable_index);
    
    /**
     * @brief Get backjump mode name for debugging
     */
    std::string get_mode_name() const;
    
    /**
     * @brief Get performance statistics
     */
    struct PerformanceStats {
        size_t total_analyses;
        size_t successful_backjumps;
        double success_rate;
        double average_analysis_time_ms;
        
        PerformanceStats() : total_analyses(0), successful_backjumps(0), 
                           success_rate(0.0), average_analysis_time_ms(0.0) {}
    };
    
    PerformanceStats get_performance_stats() const;
    
    /**
     * @brief Reset performance counters
     */
    void reset_performance_stats();
};

/**
 * @brief Advanced backjump strategy coordinator
 * 
 * Coordinates multiple backjumping strategies and provides high-level
 * interface for integration with search algorithms.
 */
class BackjumpStrategyCoordinator {
private:
    std::unique_ptr<AdvancedBackjumpAnalyzer> analyzer_;
    BackjumpMode fallback_mode_;
    
    // Strategy selection heuristics
    bool adaptive_mode_selection_;
    std::vector<double> mode_performance_history_;
    
public:
    /**
     * @brief Construct coordinator with default strategy
     */
    explicit BackjumpStrategyCoordinator(
        BackjumpMode initial_mode = BackjumpMode::INTELLIGENT_BACKJUMP);
    
    /**
     * @brief Enable adaptive mode selection based on performance
     */
    void enable_adaptive_mode_selection(bool enable = true);
    
    /**
     * @brief Perform comprehensive backjump analysis
     * 
     * This is the main interface for search algorithms to get
     * sophisticated backjump recommendations.
     */
    AdvancedBackjumpResult perform_backjump_analysis(
        const std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>& rules,
        const MusicalConstraints::DualSolutionStorage& current_solution,
        int current_variable_index);
    
    /**
     * @brief Switch to fallback mode if current mode fails
     */
    void switch_to_fallback_mode();
    
    /**
     * @brief Get current strategy information
     */
    struct StrategyInfo {
        BackjumpMode current_mode;
        BackjumpMode fallback_mode;
        bool adaptive_enabled;
        AdvancedBackjumpAnalyzer::PerformanceStats performance;
    };
    
    StrategyInfo get_strategy_info() const;
    
    /**
     * @brief Print comprehensive analysis report
     */
    void print_analysis_report(const AdvancedBackjumpResult& result) const;
};

/**
 * @brief Musical backjump heuristics for enhanced musical intelligence
 * 
 * Specialized heuristics for musical constraint satisfaction that
 * take into account musical structure and patterns.
 */
class MusicalBackjumpHeuristics {
public:
    /**
     * @brief Analyze musical conflict patterns
     * 
     * Identifies common musical conflict patterns (voice crossings,
     * dissonances, scale violations) and suggests targeted backjumps.
     */
    static AdvancedBackjumpResult analyze_musical_conflicts(
        const MusicalConstraints::DualSolutionStorage& solution,
        int current_index,
        const std::vector<MusicalConstraints::RuleResult>& rule_results);
    
    /**
     * @brief Calculate voice-leading based backjump
     * 
     * Uses voice-leading analysis to determine optimal backjump points
     * for multi-voice musical constraints.
     */
    static int calculate_voice_leading_backjump(
        const MusicalConstraints::DualSolutionStorage& solution,
        int current_index,
        int num_voices);
    
    /**
     * @brief Determine harmonic rhythm backjump
     * 
     * Analyzes harmonic rhythm patterns to suggest backjump points
     * that align with musical phrase structure.
     */
    static int calculate_harmonic_rhythm_backjump(
        const MusicalConstraints::DualSolutionStorage& solution,
        int current_index);
    
    /**
     * @brief Musical phrase structure analysis
     * 
     * Identifies musical phrase boundaries and suggests backjumps
     * that respect musical structure.
     */
    static std::vector<int> identify_phrase_boundaries(
        const MusicalConstraints::DualSolutionStorage& solution,
        int current_index);
};

/**
 * @brief Integration helper for Gecode Space integration
 * 
 * Provides seamless integration between advanced backjumping strategies
 * and Gecode's constraint propagation system.
 */
class GecodeBackjumpIntegration {
public:
    /**
     * @brief Convert backjump result to Gecode branching heuristics
     * 
     * Translates advanced backjump analysis into Gecode-compatible
     * branching strategies and variable ordering.
     */
    static void apply_backjump_to_gecode_search(
        const AdvancedBackjumpResult& backjump_result,
        void* gecode_space_ptr);  // Gecode::Space* but avoiding header dependency
    
    /**
     * @brief Generate Gecode constraint posting order
     * 
     * Uses backjump analysis to determine optimal constraint posting
     * order for improved propagation efficiency.
     */
    static std::vector<int> generate_optimal_posting_order(
        const AdvancedBackjumpResult& backjump_result,
        int total_variables);
    
    /**
     * @brief Create Gecode variable ordering from backjump analysis
     */
    static std::vector<int> create_variable_ordering(
        const AdvancedBackjumpResult& backjump_result,
        int total_variables);
};

} // namespace AdvancedBackjumping

#endif // ADVANCED_BACKJUMPING_STRATEGIES_HH