/**
 * @file cluster_engine_backjump.hh
 * @brief Musical Backjumper - Intelligent Constraint-Guided Backtracking
 * 
 * Implements the sophisticated backjumping system from Cluster Engine v4.05
 * with musical intelligence and multiple backjump strategies.
 */

#ifndef CLUSTER_ENGINE_BACKJUMP_HH
#define CLUSTER_ENGINE_BACKJUMP_HH

#include "cluster_engine_core.hh"
#include <vector>
#include <map>
#include <string>

namespace ClusterEngine {

/**
 * @brief Backjump modes from Cluster Engine v4.05
 */
enum class BackjumpMode {
    NO_BACKJUMP = 1,      // Always step back by 1, never jump farther
    MIN_BACKJUMP = 2,     // Take shortest backjump proposed by any rule (default)
    CONSENSUS_BACKJUMP = 3 // Only backjump if ALL rules agree on same distance
};

/**
 * @brief Rule test result with backjump suggestion
 */
struct RuleTestResult {
    bool passed;                    // Did the rule pass?
    int suggested_backjump_engine;  // Engine to backjump to (-1 if no suggestion)
    int suggested_backjump_index;   // Index to backjump to (-1 if no suggestion)
    std::string rule_name;          // Rule identifier for debugging
    std::string failure_reason;     // Why the rule failed
    
    RuleTestResult(bool pass = true) 
        : passed(pass), suggested_backjump_engine(-1), 
          suggested_backjump_index(-1), rule_name("unknown") {}
    
    bool has_backjump_suggestion() const {
        return suggested_backjump_engine >= 0 && suggested_backjump_index >= 0;
    }
};

/**
 * @brief Backjump preferences for different musical rule types
 * 
 * Based on Cluster Engine's musical rule backtrack preferences:
 * - rhythm-pitch rules (rp1v): backtrack to self (engine that failed)
 * - rhythm-rhythm rules (rr2v): backtrack to other engine  
 * - pitch-pitch rules (ppNv): backtrack to self
 * - meter-duration rules (dm1v): backtrack to rhythm engine
 */
class BackjumpPreferences {
private:
    std::map<std::string, int> rule_backtrack_flags_;
    
public:
    BackjumpPreferences() {
        // Initialize default musical rule preferences
        rule_backtrack_flags_["rhythm-pitch-1v"] = 1;    // self
        rule_backtrack_flags_["rhythm-rhythm-2v"] = 2;   // other  
        rule_backtrack_flags_["pitch-pitch-Nv"] = 1;     // self
        rule_backtrack_flags_["meter-duration-1v"] = 3;  // rhythm engine
        rule_backtrack_flags_["duration-meter-1v"] = 4;  // meter engine
        rule_backtrack_flags_["meter-note-1v"] = 1;      // self
        rule_backtrack_flags_["list-all-events-Nv"] = 2; // other
    }
    
    int get_backtrack_preference(const std::string& rule_type) const {
        auto it = rule_backtrack_flags_.find(rule_type);
        return (it != rule_backtrack_flags_.end()) ? it->second : 1; // default to self
    }
    
    void set_backtrack_preference(const std::string& rule_type, int preference) {
        rule_backtrack_flags_[rule_type] = preference;
    }
};

/**
 * @brief Linear solution for backjump calculations
 * 
 * Provides fast access to solution data needed for backjump analysis:
 * - Count values (cumulative note counts)
 * - Onset times (timepoints for rhythm engines)  
 * - Pitch sequences (for pitch engines)
 * - Metric positions (for metric engine)
 */
struct LinearSolution {
    std::vector<std::vector<int>> count_values;    // [engine][index] = cumulative count
    std::vector<std::vector<double>> onset_times;  // [engine][index] = timepoint
    std::vector<std::vector<int>> pitch_values;    // [engine][index] = MIDI pitch
    std::vector<std::vector<std::pair<int,int>>> metric_values; // [engine][index] = (numerator, denominator)
    
    void update_for_engine(int engine_id, const MusicalEngine& engine);
    void clear() {
        count_values.clear();
        onset_times.clear();
        pitch_values.clear();
        metric_values.clear();
    }
};

/**
 * @brief Musical Backjumper - Core backjumping logic
 */
class MusicalBackjumper {
private:
    ClusterEngineCore* core_;
    BackjumpMode mode_;
    BackjumpPreferences preferences_;
    LinearSolution linear_solution_;
    
    // Backjump analysis methods
    int analyze_backjump_mode1(const std::vector<RuleTestResult>& results) const;
    int analyze_backjump_mode2(const std::vector<RuleTestResult>& results) const;
    int analyze_backjump_mode3(const std::vector<RuleTestResult>& results) const;
    
    // Index calculation helpers
    int find_index_for_count_value(int count_value, int engine_id) const;
    int find_index_for_timepoint(double timepoint, int engine_id) const;
    int find_index_for_position_in_duration_sequence(int position, int engine_id) const;
    
    // Safety checks
    bool is_valid_backjump_target(int target_engine, int target_index) const;
    bool would_skip_variables(int current_engine, int target_engine, int target_index) const;
    
public:
    MusicalBackjumper(ClusterEngineCore* core, BackjumpMode mode = BackjumpMode::MIN_BACKJUMP);
    
    /**
     * @brief Analyze rule test results and determine backjump target
     * 
     * @param results Vector of rule test results with backjump suggestions
     * @return Target engine index to backjump to (-1 for simple backstep)
     */
    int analyze_backjump(const std::vector<RuleTestResult>& results);
    
    /**
     * @brief Execute backjump to target engine and index
     */
    bool execute_backjump(int target_engine, int target_index);
    
    /**
     * @brief Execute simple backstep (move current engine back by 1)
     */
    bool execute_backstep(int current_engine);
    
    /**
     * @brief Update linear solution data for backjump calculations
     */
    void update_linear_solution();
    
    /**
     * @brief Set backjump mode
     */
    void set_mode(BackjumpMode mode) { mode_ = mode; }
    BackjumpMode get_mode() const { return mode_; }
    
    /**
     * @brief Access backjump preferences
     */
    BackjumpPreferences& get_preferences() { return preferences_; }
    const BackjumpPreferences& get_preferences() const { return preferences_; }
    
    // Debug and analysis
    void print_backjump_analysis(const std::vector<RuleTestResult>& results, int target) const;
    void print_linear_solution_summary() const;
    
    // Getters for analysis
    const LinearSolution& get_linear_solution() const { return linear_solution_; }
};

/**
 * @brief Backjump suggestion helpers for rule implementations
 */
struct BackjumpSuggestions {
    /**
     * @brief Suggest backjump for rhythm-pitch constraint failure
     */
    static RuleTestResult suggest_rhythm_pitch_backjump(
        int current_engine, int current_index, const std::string& reason);
    
    /**
     * @brief Suggest backjump for rhythm-rhythm constraint failure  
     */
    static RuleTestResult suggest_rhythm_rhythm_backjump(
        int engine1, int engine2, int current_index, const std::string& reason);
    
    /**
     * @brief Suggest backjump for pitch-pitch constraint failure
     */
    static RuleTestResult suggest_pitch_pitch_backjump(
        const std::vector<int>& engines, int current_index, const std::string& reason);
    
    /**
     * @brief Suggest backjump for metric-duration constraint failure
     */
    static RuleTestResult suggest_metric_duration_backjump(
        int metric_engine, int rhythm_engine, int current_index, const std::string& reason);
};

/**
 * @brief Advanced Linear Solution Converter
 * 
 * Based on convert-vsolution->linear-and-backjump from 07.backjumping.lisp
 * Provides comprehensive solution state conversion for optimal backjumping.
 */
class AdvancedLinearConverter {
public:
    struct BackjumpSolutionState {
        std::vector<std::vector<int>> engine_count_values;      // Count values for backjump analysis
        std::vector<std::vector<double>> engine_onset_values;   // Onset timepoints for backjump analysis  
        std::vector<std::vector<int>> changed_engines;          // Engines that changed in this step
        std::vector<bool> engine_locked;                        // Which engines are locked/solved
        
        BackjumpSolutionState(int num_engines) {
            engine_count_values.resize(num_engines);
            engine_onset_values.resize(num_engines);
            changed_engines.resize(num_engines);
            engine_locked.resize(num_engines, false);
        }
    };
    
    AdvancedLinearConverter() = default;
    
    // Convert complete solution to linear format with backjump data
    BackjumpSolutionState convert_solution_for_backjump(const ClusterEngineCore& core, 
                                                        const std::vector<int>& changed_engines);
    
    // Convert single engine for incremental updates (based on convert-ONE-vsolution->linear-and-backjump)
    void convert_one_engine_for_backjump(const ClusterEngineCore& core, int engine_id,
                                         BackjumpSolutionState& solution_state);
    
    // Extract count values for specific engine (get-one-engine-index-first-counts)
    std::vector<int> get_engine_index_first_counts(const MusicalEngine& engine);
    
    // Extract onset values for specific engine (get-one-engine-index-first-onsets)  
    std::vector<double> get_engine_index_first_onsets(const MusicalEngine& engine);
    
    // Check if engine is measure layer (last engine)
    bool is_measure_layer(int engine_id, int total_engines) const {
        return engine_id == (total_engines - 1);
    }
    
    // Check if engine is rhythm layer (even numbered)
    bool is_rhythm_layer(int engine_id) const {
        return (engine_id % 2) == 0;
    }
};

/**
 * @brief Advanced Backjump Index Calculator
 * 
 * Based on sophisticated index calculation functions from 07.backjumping.lisp
 * Provides precise backjump target calculation with musical intelligence.
 */
class AdvancedBackjumpIndexCalculator {
public:
    AdvancedBackjumpIndexCalculator() = default;
    
    // Find index for count value failure (find-index-for-countvalue)
    int find_index_for_count_value(int count_value, 
                                   const std::vector<int>& count_sequence) const;
    
    // Find index for timepoint failure (find-index-for-timepoint) 
    int find_index_for_timepoint(double timepoint,
                                 const std::vector<double>& timepoint_sequence) const;
    
    // Find index before timepoint (find-index-before-timepoint)
    int find_index_before_timepoint(double timepoint,
                                    const std::vector<double>& timepoint_sequence) const;
    
    // Find index for position in duration sequence (find-index-for-position-in-durationseq)
    int find_index_for_position_in_duration_sequence(int position, 
                                                     const LinearSolution& linear_solution,
                                                     const AdvancedLinearConverter::BackjumpSolutionState& backjump_state,
                                                     int engine_id) const;
    
    // Reset backjump indexes for new search (reset-vbackjump-indexes)
    void reset_backjump_indexes(std::vector<std::vector<int>>& backjump_indexes) const;
    
private:
    // Helper for safe index calculation
    int safe_index_calculation(int candidate_index, int sequence_length) const;
};

/**
 * @brief Advanced Backjump Coordinator  
 * 
 * Based on sophisticated backjump coordination from 07.backjumping.lisp
 * Handles multi-voice and cross-engine backjump coordination.
 */
class AdvancedBackjumpCoordinator {
public:
    struct BackjumpIndexState {
        std::vector<std::vector<int>> engine_backjump_indexes;  // Backjump targets per engine
        std::vector<bool> engine_has_backjump_target;           // Which engines have targets
        int primary_failed_engine;                              // Main engine that caused failure
        int secondary_failed_engine;                            // Secondary engine affected
        
        BackjumpIndexState(int num_engines) {
            engine_backjump_indexes.resize(num_engines);
            engine_has_backjump_target.resize(num_engines, false);
            primary_failed_engine = -1;
            secondary_failed_engine = -1;
        }
    };
    
    AdvancedBackjumpCoordinator(int num_engines) 
        : num_engines_(num_engines), calculator_(), converter_(), 
          current_backjump_state_(num_engines) {}
    
    // Set backjump indexes from pitch-duration failure (set-vbackjump-indexes-from-failed-count-pitch-duration)
    void set_backjump_from_pitch_duration_failure(int failed_count_value, int failed_engine,
                                                  int rhythm_engine, int pitch_engine,
                                                  const AdvancedLinearConverter::BackjumpSolutionState& backjump_state);
    
    // Set backjump indexes from multi-voice failure (set-vbackjump-indexes-from-failed-notecount-duration-pitch-in-voices)  
    void set_backjump_from_multi_voice_failure(const std::vector<int>& failed_note_counts,
                                               const std::vector<int>& voice_numbers,
                                               const AdvancedLinearConverter::BackjumpSolutionState& backjump_state,
                                               const LinearSolution& linear_solution);
    
    // Get optimal backjump target for current situation
    std::pair<int, int> get_optimal_backjump_target() const;
    
    // Reset backjump coordination for new search
    void reset_coordination();
    
    // Check if any engine has backjump target
    bool has_backjump_targets() const;
    
    // Get engines with pending backjump targets
    std::vector<int> get_engines_with_targets() const;
    
    // Update backjump state after engine progress
    void update_after_engine_progress(int engine_id, int new_index);
    
    // Get backjump state (const and non-const)
    const BackjumpIndexState& get_backjump_state() const { return current_backjump_state_; }
    BackjumpIndexState& get_backjump_state() { return current_backjump_state_; }
    
private:
    int num_engines_;
    AdvancedBackjumpIndexCalculator calculator_;
    AdvancedLinearConverter converter_;
    BackjumpIndexState current_backjump_state_;
};

/**
 * @brief Comprehensive Advanced Backjump Manager
 * 
 * Integrates all advanced backjumping capabilities into a unified interface
 * for the main search process. Provides the intelligence from the original
 * Cluster Engine backjumping system.
 */
class AdvancedBackjumpManager {
public:
    AdvancedBackjumpManager(ClusterEngineCore* core)
        : core_(core), coordinator_(core ? core->get_num_engines() : 0),
          converter_(), calculator_(), backjump_enabled_(true),
          intelligent_analysis_(true), current_linear_solution_() {}
    
    // Main backjump processing interface
    bool process_constraint_failure(int failed_engine, int failed_index, 
                                   const std::string& constraint_type,
                                   const std::vector<int>& affected_engines = {});
    
    // Process heuristic exhaustion
    bool process_heuristic_exhaustion(int current_engine, int current_index);
    
    // Get next backjump target
    std::pair<int, int> get_next_backjump_target();
    
    // Execute backjump operation  
    bool execute_advanced_backjump(int target_engine, int target_index);
    
    // Update solution state for backjump analysis
    void update_solution_state(const std::vector<int>& changed_engines);
    
    // Configuration
    void set_backjump_enabled(bool enabled) { backjump_enabled_ = enabled; }
    void set_intelligent_analysis(bool enabled) { intelligent_analysis_ = enabled; }
    
    bool is_backjump_enabled() const { return backjump_enabled_; }
    bool is_intelligent_analysis_enabled() const { return intelligent_analysis_; }
    
    // Reset for new search
    void reset_for_new_search();
    
    // Statistics and debugging
    void print_advanced_backjump_analysis() const;
    void print_solution_state_summary() const;
    
    // Access to components for integration
    AdvancedBackjumpCoordinator& get_coordinator() { return coordinator_; }
    const AdvancedBackjumpCoordinator& get_coordinator() const { return coordinator_; }
    
private:
    ClusterEngineCore* core_;
    AdvancedBackjumpCoordinator coordinator_;
    AdvancedLinearConverter converter_;
    AdvancedBackjumpIndexCalculator calculator_;
    bool backjump_enabled_;
    bool intelligent_analysis_;
    
    // Current solution state cache
    std::unique_ptr<AdvancedLinearConverter::BackjumpSolutionState> current_solution_state_;
    std::unique_ptr<LinearSolution> current_linear_solution_;
    
    // Update solution state cache
    void update_solution_cache();
    
    // Analyze failure pattern for optimal backjump strategy
    void analyze_failure_pattern(int failed_engine, const std::string& constraint_type,
                                const std::vector<int>& affected_engines);
};

} // namespace ClusterEngine

#endif // CLUSTER_ENGINE_BACKJUMP_HH