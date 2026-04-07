/**
 * @file cluster_engine_forward_rules.hh
 * @brief Forward Rule Manager - Intelligent Engine Selection System
 * 
 * Implements the Cluster Engine forward rule system for intelligent
 * engine coordination based on musical priorities and constraints.
 */

#ifndef CLUSTER_ENGINE_FORWARD_RULES_HH
#define CLUSTER_ENGINE_FORWARD_RULES_HH

#include "cluster_engine_core.hh"
#include <vector>
#include <memory>

namespace ClusterEngine {

/**
 * @brief BackTrack history for forward rule decisions
 */
struct BacktrackHistory {
    std::vector<int> engines;         // Engine history
    std::vector<int> indices;         // Index history  
    std::vector<int> count_values;    // Count values
    std::vector<double> time_positions; // Time positions
    
    void clear() {
        engines.clear();
        indices.clear();
        count_values.clear();
        time_positions.clear();
    }
    
    bool has_backtrack_route() const {
        return !engines.empty();
    }
};

/**
 * @brief Forward Rule Manager - fwd-rule2 implementation
 * 
 * Implements the sophisticated engine selection algorithm:
 * 1. If backtrack history exists, forward to that engine
 * 2. Metric engine must be longest (extend if needed)
 * 3. Fill pitch gaps for existing durations (highest priority voice first)
 * 4. Search rhythm in voice that is most behind (default order breaks ties)
 */
class ForwardRuleManager {
private:
    ClusterEngineCore* core_;
    BacktrackHistory backtrack_history_;
    std::vector<int> default_engine_order_;
    std::vector<int> locked_engines_;
    
    // Engine analysis helpers
    double get_metric_engine_length() const;
    std::vector<double> get_all_voice_total_lengths() const;
    double get_voice_total_length(int voice_id) const;
    int find_pitch_engine_with_missing_pitches() const;
    int find_shortest_rhythm_engine() const;
    bool voice_has_missing_pitches(int voice_id) const;
    double get_engine_current_endtime(int engine_id) const;
    
public:
    ForwardRuleManager(ClusterEngineCore* core);
    
    /**
     * @brief Main forward rule decision (fwd-rule2 implementation)
     * 
     * Priority order:
     * 1. Backtrack route if exists
     * 2. Metric engine if it's not longest
     * 3. Pitch engine with missing pitches (priority order)
     * 4. Shortest rhythm engine (default order breaks ties)
     */
    int select_next_engine();
    
    /**
     * @brief Set backtrack route for next forward step
     */
    void set_backtrack_route(int engine, int index, int count_value, double time_pos) {
        backtrack_history_.engines.push_back(engine);
        backtrack_history_.indices.push_back(index);
        backtrack_history_.count_values.push_back(count_value);
        backtrack_history_.time_positions.push_back(time_pos);
    }
    
    /**
     * @brief Pop backtrack route (used when forwarding to backtrack engine)
     */
    void pop_backtrack_route() {
        if (backtrack_history_.has_backtrack_route()) {
            backtrack_history_.engines.pop_back();
            backtrack_history_.indices.pop_back();
            backtrack_history_.count_values.pop_back();
            backtrack_history_.time_positions.pop_back();
        }
    }
    
    /**
     * @brief Clear all backtrack history
     */
    void clear_backtrack_history() {
        backtrack_history_.clear();
    }
    
    /**
     * @brief Set default engine search order
     */
    void set_default_engine_order(const std::vector<int>& order) {
        default_engine_order_ = order;
    }
    
    /**
     * @brief Set locked engines (cannot be searched)
     */
    void set_locked_engines(const std::vector<int>& locked) {
        locked_engines_ = locked;
    }
    
    /**
     * @brief Check if engine is available for search
     */
    bool is_engine_available(int engine_id) const {
        return std::find(locked_engines_.begin(), locked_engines_.end(), engine_id) == locked_engines_.end();
    }
    
    // Debug and analysis
    void print_engine_analysis() const;
    void print_forward_decision(int selected_engine) const;
    
    // Getters for analysis
    const BacktrackHistory& get_backtrack_history() const { return backtrack_history_; }
    const std::vector<int>& get_default_order() const { return default_engine_order_; }
};

/**
 * @brief Forward Rule Types (different algorithms)
 */
enum class ForwardRuleType {
    FWD_RULE1,  // Simple shortest index next (very basic)
    FWD_RULE2,  // Sophisticated musical algorithm (default/recommended)
    FWD_RULE3   // Alternative algorithm variant
};

/**
 * @brief Forward Rule Factory
 */
class ForwardRuleFactory {
public:
    static std::unique_ptr<ForwardRuleManager> create_forward_rule_manager(
        ForwardRuleType type, ClusterEngineCore* core);
};

} // namespace ClusterEngine

#endif // CLUSTER_ENGINE_FORWARD_RULES_HH