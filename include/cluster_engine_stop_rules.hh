/**
 * @file cluster_engine_stop_rules.hh
 * @brief Search termination rules for Cluster Engine
 * 
 * Implements intelligent search stopping conditions based on time, index,
 * and musical structure analysis. Ported from 05i.rules-stop-search.lisp
 */

#ifndef CLUSTER_ENGINE_STOP_RULES_HH
#define CLUSTER_ENGINE_STOP_RULES_HH

#include <vector>
#include <functional>
#include <memory>
#include <string>

namespace ClusterEngine {

// Forward declarations
class ClusterEngineCore;
class MusicalEngine;
struct MusicalCandidate;

/**
 * @brief Types of stop conditions
 */
enum class StopConditionType {
    TIME_BASED,           // Stop at specific musical time
    INDEX_BASED,          // Stop at specific engine index  
    NOTE_COUNT_BASED,     // Stop when number of notes reached
    MEASURE_BASED,        // Stop at measure boundary
    CUSTOM_FUNCTION       // User-defined stop logic
};

/**
 * @brief Logic operators for combining conditions
 */
enum class StopLogic {
    OR,     // Stop when ANY condition is met
    AND,    // Stop when ALL conditions are met
    XOR     // Stop when exactly one condition is met
};

/**
 * @brief Individual stop condition
 */
class StopCondition {
private:
    StopConditionType type_;
    std::vector<int> target_engines_;
    double time_threshold_;
    int index_threshold_;
    int note_count_threshold_;
    std::function<bool(const ClusterEngineCore&)> custom_function_;
    std::string description_;
    bool active_;

public:
    StopCondition(StopConditionType type, 
                  const std::vector<int>& engines = {},
                  const std::string& desc = "");
    
    // Time-based stop condition
    static StopCondition create_time_stop(const std::vector<int>& engines, 
                                         double stop_time,
                                         const std::string& description = "Time Stop");
    
    // Index-based stop condition  
    static StopCondition create_index_stop(const std::vector<int>& engines,
                                          int stop_index,
                                          const std::string& description = "Index Stop");
    
    // Note count stop condition
    static StopCondition create_note_count_stop(const std::vector<int>& engines,
                                               int note_count,
                                               const std::string& description = "Note Count Stop");
    
    // Custom function stop condition
    static StopCondition create_custom_stop(std::function<bool(const ClusterEngineCore&)> func,
                                           const std::string& description = "Custom Stop");
    
    // Evaluation
    bool evaluate(const ClusterEngineCore& core) const;
    bool evaluate_time_condition(const ClusterEngineCore& core) const;
    bool evaluate_index_condition(const ClusterEngineCore& core) const;
    bool evaluate_note_count_condition(const ClusterEngineCore& core) const;
    
    // Configuration
    void set_time_threshold(double time) { time_threshold_ = time; }
    void set_index_threshold(int index) { index_threshold_ = index; }
    void set_note_count_threshold(int count) { note_count_threshold_ = count; }
    void set_custom_function(std::function<bool(const ClusterEngineCore&)> func) { 
        custom_function_ = func; 
    }
    
    // State management
    void activate() { active_ = true; }
    void deactivate() { active_ = false; }
    bool is_active() const { return active_; }
    
    // Information
    StopConditionType get_type() const { return type_; }
    const std::vector<int>& get_target_engines() const { return target_engines_; }
    const std::string& get_description() const { return description_; }
    double get_time_threshold() const { return time_threshold_; }
    int get_index_threshold() const { return index_threshold_; }
    int get_note_count_threshold() const { return note_count_threshold_; }
};

/**
 * @brief Manages multiple stop conditions and logic
 */
class StopRuleManager {
private:
    std::vector<StopCondition> conditions_;
    StopLogic logic_;
    bool search_stopped_;
    std::string last_stop_reason_;
    int evaluation_count_;
    
    // Statistics
    int time_stops_triggered_;
    int index_stops_triggered_;
    int note_count_stops_triggered_;
    int custom_stops_triggered_;

public:
    StopRuleManager(StopLogic logic = StopLogic::OR);
    
    // Condition management
    void add_condition(const StopCondition& condition);
    void add_time_stop(const std::vector<int>& engines, double stop_time);
    void add_index_stop(const std::vector<int>& engines, int stop_index);
    void add_note_count_stop(const std::vector<int>& engines, int note_count);
    void add_custom_stop(std::function<bool(const ClusterEngineCore&)> func,
                        const std::string& description = "Custom Stop");
    
    void clear_conditions();
    void clear_time_conditions();
    void clear_index_conditions();
    
    // Main evaluation function
    bool should_stop_search(const ClusterEngineCore& core);
    
    // Logic configuration
    void set_logic(StopLogic logic) { logic_ = logic; }
    StopLogic get_logic() const { return logic_; }
    
    // State queries
    bool is_search_stopped() const { return search_stopped_; }
    const std::string& get_last_stop_reason() const { return last_stop_reason_; }
    void reset_stop_state();
    
    // Condition access
    const std::vector<StopCondition>& get_conditions() const { return conditions_; }
    size_t get_condition_count() const { return conditions_.size(); }
    bool has_time_conditions() const;
    bool has_index_conditions() const;
    bool has_note_count_conditions() const;
    
    // Statistics
    void print_stop_statistics() const;
    void reset_statistics();
    int get_evaluation_count() const { return evaluation_count_; }
    int get_total_stops_triggered() const;
    
    // Musical analysis helpers
    double get_current_musical_time(const ClusterEngineCore& core, int engine) const;
    int get_current_note_count(const ClusterEngineCore& core, int engine) const;
    int get_total_note_count(const ClusterEngineCore& core) const;
    
    // Advanced stop conditions
    void add_measure_boundary_stop(const std::vector<int>& engines, int measure_count);
    void add_harmonic_cadence_stop(const std::vector<int>& engines);
    void add_phrase_completion_stop(const std::vector<int>& engines, int phrase_length);
};

/**
 * @brief Factory for common stop rule patterns
 */
class StopRuleFactory {
public:
    // Classical composition patterns
    static std::unique_ptr<StopRuleManager> create_classical_stop_rules(
        const std::vector<int>& voices, 
        int measures = 8,
        bool include_cadence_detection = true);
    
    // Jazz improvisation patterns  
    static std::unique_ptr<StopRuleManager> create_jazz_stop_rules(
        const std::vector<int>& voices,
        int chorus_length = 32,
        bool allow_extended_solos = false);
    
    // Real-time interactive patterns
    static std::unique_ptr<StopRuleManager> create_realtime_stop_rules(
        const std::vector<int>& voices,
        double max_time_seconds = 10.0,
        int max_notes_per_voice = 16);
    
    // Educational exercise patterns
    static std::unique_ptr<StopRuleManager> create_exercise_stop_rules(
        const std::vector<int>& voices,
        const std::string& exercise_type = "species_counterpoint");
    
    // Custom patterns with user configuration
    static std::unique_ptr<StopRuleManager> create_custom_stop_rules(
        const std::vector<int>& voices,
        const std::vector<std::pair<StopConditionType, double>>& conditions,
        StopLogic logic = StopLogic::OR);
};

/**
 * @brief Stop rule integration with search process
 */
class SearchTerminationManager {
private:
    std::unique_ptr<StopRuleManager> stop_manager_;
    bool auto_evaluation_enabled_;
    int evaluation_frequency_;
    int evaluations_since_check_;
    
public:
    SearchTerminationManager();
    ~SearchTerminationManager();
    
    // Manager assignment
    void set_stop_manager(std::unique_ptr<StopRuleManager> manager);
    StopRuleManager* get_stop_manager() { return stop_manager_.get(); }
    const StopRuleManager* get_stop_manager() const { return stop_manager_.get(); }
    
    // Search integration
    bool check_termination(const ClusterEngineCore& core);
    void configure_auto_evaluation(bool enabled, int frequency = 10);
    
    // Callback for search loop integration
    std::function<void(const std::string&)> termination_callback;
    
    // Reset for new search
    void reset_for_new_search();
    
    // Statistics and monitoring
    void print_termination_analysis() const;
};

} // namespace ClusterEngine

#endif // CLUSTER_ENGINE_STOP_RULES_HH