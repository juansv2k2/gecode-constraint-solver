/**
 * @file cluster_engine_core.hh
 * @brief True Cluster Engine Architecture - Multi-Engine Coordination System
 * 
 * Implements the authentic Cluster Engine approach with:
 * - Multi-engine coordination (rhythm/pitch alternation)
 * - Heuristic rule system for musical intelligence
 * - Musical domain generation and management
 * - Forward rule engine coordination
 * - Sophisticated musical backjumping
 */

#ifndef CLUSTER_ENGINE_CORE_HH
#define CLUSTER_ENGINE_CORE_HH

#include <vector>
#include <array>
#include <map>
#include <functional>
#include <memory>
#include <string>
#include <iostream>
#include <algorithm>
#include <numeric>



namespace ClusterEngine {

// Forward declarations
class MusicalEngine;
class HeuristicRule;
class ForwardRuleManager;
class MusicalBackjumper;
class LinearSolutionManager;

/**
 * @brief Engine types in Cluster Engine system
 */
enum class EngineType : int {
    RHYTHM_ENGINE = 0,    // Even numbers: 0, 2, 4, 6... (duration sequences)
    PITCH_ENGINE = 1,     // Odd numbers: 1, 3, 5, 7... (pitch sequences) 
    METRIC_ENGINE = -1    // Last engine: time signatures and metric structure
};

/**
 * @brief Musical candidate with dual representation (v4.05 innovation)
 */
struct MusicalCandidate {
    int absolute_value;    // MIDI pitch, timepoint (milliseconds), or time signature
    int interval_value;    // Interval (semitones), duration (milliseconds), or beat length
    bool absolute_primary; // Which value was the primary input
    
    MusicalCandidate() : absolute_value(0), interval_value(0), absolute_primary(true) {}
    
    MusicalCandidate(int abs_val, int int_val, bool abs_primary = true)
        : absolute_value(abs_val), interval_value(int_val), absolute_primary(abs_primary) {}
    
    // Automatic conversion helpers
    void calculate_missing_absolute(int reference_point = 0) {
        if (!absolute_primary) {
            absolute_value = reference_point + interval_value;
        }
    }
    
    void calculate_missing_interval(int reference_point = 0) {
        if (absolute_primary) {
            interval_value = absolute_value - reference_point;
        }
    }
};

/**
 * @brief Heuristic rule for musical intelligence
 */
class HeuristicRule {
private:
    std::vector<int> target_engines_;
    std::function<double(const std::vector<MusicalCandidate>&, int)> weight_function_;
    std::string description_;
    bool compiled_;
    
public:
    HeuristicRule(const std::vector<int>& engines, 
                  std::function<double(const std::vector<MusicalCandidate>&, int)> func,
                  const std::string& desc)
        : target_engines_(engines), weight_function_(func), description_(desc), compiled_(false) {}
    
    /**
     * @brief Calculate heuristic weight for a candidate
     */
    double calculate_weight(const std::vector<MusicalCandidate>& context, int candidate_index) const;
    
    bool applies_to_engine(int engine) const {
        return std::find(target_engines_.begin(), target_engines_.end(), engine) != target_engines_.end();
    }
    
    const std::vector<int>& get_engines() const { return target_engines_; }
    const std::string& get_description() const { return description_; }
};

/**
 * @brief Musical domain for engine-specific search spaces
 */
class MusicalDomain {
private:
    EngineType type_;
    std::vector<MusicalCandidate> candidates_;
    std::vector<double> heuristic_weights_;
    bool weights_calculated_;
    
public:
    MusicalDomain(EngineType type) : type_(type), weights_calculated_(false) {}
    
    void add_candidate(const MusicalCandidate& candidate) {
        candidates_.push_back(candidate);
        heuristic_weights_.push_back(0.0); // Default weight
        weights_calculated_ = false;
    }
    
    // Get candidates (const and non-const versions)
    const std::vector<MusicalCandidate>& get_candidates() const {
        return candidates_;
    }
    
    std::vector<MusicalCandidate>& get_candidates() {
        return candidates_;
    }
    
    void sort_by_heuristic_weights();
    
    void set_heuristic_weight(size_t index, double weight) {
        if (index < heuristic_weights_.size()) {
            heuristic_weights_[index] = weight;
            weights_calculated_ = true;
        }
    }
    
    EngineType get_type() const { return type_; }
    size_t size() const { return candidates_.size(); }
};

/**
 * @brief Individual musical engine (rhythm, pitch, or metric)
 */
class MusicalEngine {
private:
    int engine_id_;
    EngineType type_;
    MusicalDomain domain_;
    std::vector<MusicalCandidate> current_solution_;
    int current_index_;
    bool locked_;
    
public:
    MusicalEngine(int id, EngineType type) 
        : engine_id_(id), type_(type), domain_(type), current_index_(-1), locked_(false) {}
    
    // Engine identification
    int get_id() const { return engine_id_; }
    EngineType get_type() const { return type_; }
    bool is_rhythm_engine() const;
    bool is_pitch_engine() const;
    bool is_metric_engine() const;
    
    // Voice relationships  
    int get_voice_id() const;
    int get_partner_engine_id() const;
    
    // Domain management
    MusicalDomain& get_domain() { return domain_; }
    const MusicalDomain& get_domain() const { return domain_; }
    
    void lock_engine() { locked_ = true; }
    bool is_locked() const { return locked_ || domain_.size() <= 1; }
    
    // Solution management
    void step_forward() { current_index_++; }
    void step_backward() { if (current_index_ >= 0) current_index_--; }
    void set_index(int index) { current_index_ = index; }
    int get_index() const { return current_index_; }
    
    bool has_solution() const { return current_index_ >= 0; }
    size_t solution_length() const { return current_index_ + 1; }
    
    void assign_candidate(const MusicalCandidate& candidate) {
        if (current_index_ >= 0 && current_index_ < static_cast<int>(current_solution_.size())) {
            current_solution_[current_index_] = candidate;
        } else if (current_index_ >= 0) {
            current_solution_.resize(current_index_ + 1);
            current_solution_[current_index_] = candidate;
        }
    }
    
    const std::vector<MusicalCandidate>& get_solution() const { return current_solution_; }
    
    MusicalCandidate get_current_candidate() const {
        if (current_index_ >= 0 && current_index_ < static_cast<int>(current_solution_.size())) {
            return current_solution_[current_index_];
        }
        return MusicalCandidate(0, 0); // Default empty candidate
    }
};

/**
 * @brief Core multi-engine coordination system
 */
class ClusterEngineCore {
private:
    std::vector<std::unique_ptr<MusicalEngine>> engines_;
    std::vector<HeuristicRule> heuristic_rules_;
    
    int num_voices_;
    int max_index_;
    int current_engine_;
    bool debug_mode_;
    
public:
    ClusterEngineCore(int num_voices, int max_index_per_engine, bool debug = false);
    
    // Engine management
    void initialize_engines();
    MusicalEngine& get_engine(int id) { return *engines_[id]; }
    const MusicalEngine& get_engine(int id) const { return *engines_[id]; }
    int get_num_engines() const { return engines_.size(); }
    
    // Heuristic rules
    void add_heuristic_rule(const HeuristicRule& rule) {
        heuristic_rules_.push_back(rule);
    }
    
    void apply_heuristic_rules(int engine_id);
    
    // Main search interface
    bool search_step();
    bool search_complete();
    std::vector<std::vector<MusicalCandidate>> get_complete_solution();
    
    // Engine coordination
    int select_next_engine();
    void process_forward_step();
    void process_backtrack_step();
    
    // Current engine tracking
    int get_current_engine() const { return current_engine_; }
    void set_current_engine(int engine_id) {
        if (engine_id >= 0 && engine_id < static_cast<int>(engines_.size())) {
            current_engine_ = engine_id;
        }
    }
    
    // Debug and monitoring
    void print_engine_status() const;
    void print_solution_summary() const;
};

} // namespace ClusterEngine

#endif // CLUSTER_ENGINE_CORE_HH