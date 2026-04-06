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

} // namespace ClusterEngine

#endif // CLUSTER_ENGINE_BACKJUMP_HH