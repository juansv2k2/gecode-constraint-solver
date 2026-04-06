/**
 * @file cluster_engine_heuristic.hh
 * @brief Enhanced Heuristic Rules System for Musical Constraint Solving
 * 
 * This system implements the sophisticated heuristic rule engine from the original 
 * Cluster Engine 06.heuristic-rules-interface.lisp, providing intelligent candidate 
 * ordering and domain reduction for massive search performance improvements.
 */

#ifndef CLUSTER_ENGINE_HEURISTIC_HH
#define CLUSTER_ENGINE_HEURISTIC_HH

#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <algorithm>
#include <string>
#include <cmath>

namespace ClusterEngine {

// Forward declarations
class ClusterEngineCore;
class MusicalEngine;
class MusicalCandidate;

/**
 * @brief Heuristic Rule Types
 */
enum class HeuristicRuleType {
    SINGLE_ENGINE_PITCH,      // Analyze patterns within one pitch engine
    SINGLE_ENGINE_RHYTHM,     // Analyze patterns within one rhythm engine
    DUAL_ENGINE_PITCH_RHYTHM, // Analyze rhythm-pitch coordination
    MULTI_ENGINE_HARMONIC,    // Analyze harmonic relationships across engines
    SWITCH_RULE,              // Binary rules (return weight or 0)
    WEIGHTED_RULE             // Continuous weight rules
};

/**
 * @brief Musical Context for Heuristic Analysis
 * 
 * Provides access to musical sequences, counts, and patterns needed for
 * sophisticated heuristic rule evaluation.
 */
class MusicalAnalysisContext {
private:
    ClusterEngineCore* core_;
    
    // Cached musical sequences for analysis
    std::vector<std::vector<int>> pitch_sequences_;
    std::vector<std::vector<int>> rhythm_sequences_;
    std::vector<std::vector<double>> onset_sequences_;
    std::vector<std::vector<int>> metric_sequences_;
    
    // Musical counts and statistics
    std::vector<int> total_pitch_counts_;
    std::vector<int> total_note_counts_;
    std::vector<int> total_duration_counts_;
    
    bool sequences_cached_;
    int last_update_checksum_;

public:
    MusicalAnalysisContext(ClusterEngineCore* core);
    
    // Core analysis functions
    void update_musical_sequences();
    void clear_cache();
    bool is_cache_valid() const;
    
    // Single-engine analysis
    std::vector<int> get_pitch_sequence(int engine) const;
    std::vector<int> get_rhythm_sequence(int engine) const;
    std::vector<double> get_onset_sequence(int engine) const;
    
    // Multi-engine pattern analysis
    std::vector<std::pair<int, int>> get_rhythm_pitch_pairs(int rhythm_engine, int pitch_engine) const;
    std::vector<std::vector<int>> get_harmonic_progression(const std::vector<int>& pitch_engines) const;
    
    // Musical statistics
    int get_total_pitch_count(int engine) const;
    int get_total_note_count(int engine) const;
    int get_total_duration_count(int engine) const;
    
    // Specific musical queries
    int get_pitch_at_count(int engine, int pitch_count) const;
    int get_duration_at_count(int engine, int note_count) const;
    double get_onset_at_count(int engine, int onset_count) const;
    
    // Current candidate analysis
    int count_notes_in_current_candidate(int engine, int candidate_index) const;
    double get_current_candidate_duration(int engine, int candidate_index) const;
    std::vector<int> get_current_candidate_pitches(int engine, int candidate_index) const;
};

/**
 * @brief Base class for all heuristic rules
 * 
 * Heuristic rules analyze the current musical state and return weights
 * for domain candidates, enabling intelligent search prioritization.
 */
class HeuristicRule {
protected:
    std::vector<int> target_engines_;
    HeuristicRuleType type_;
    std::string rule_name_;
    double base_weight_;
    bool enabled_;

public:
    HeuristicRule(const std::vector<int>& engines, HeuristicRuleType type, 
                  const std::string& name, double weight = 1.0);
    virtual ~HeuristicRule() = default;
    
    // Core rule evaluation
    virtual double evaluate_candidate(const MusicalAnalysisContext& context, 
                                      int engine, int candidate_index) const = 0;
    
    // Rule configuration
    void set_weight(double weight) { base_weight_ = weight; }
    double get_weight() const { return base_weight_; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    bool is_enabled() const { return enabled_; }
    
    // Rule information
    const std::vector<int>& get_target_engines() const { return target_engines_; }
    HeuristicRuleType get_type() const { return type_; }
    const std::string& get_name() const { return rule_name_; }
    
    // Utility functions for rule implementations
    double calculate_average_weight(const std::vector<double>& weights) const;
    double normalize_weight(double raw_weight, double min_val, double max_val) const;
};

/**
 * @brief Single-Engine Pitch Pattern Heuristic Rule
 * 
 * Analyzes melodic patterns within a single pitch engine to guide
 * candidate selection based on musical coherence.
 */
class SingleEnginePitchRule : public HeuristicRule {
private:
    std::function<double(const std::vector<int>&)> pitch_pattern_function_;
    int analysis_window_size_;
    int target_engine_;

public:
    SingleEnginePitchRule(int engine, 
                          std::function<double(const std::vector<int>&)> pattern_func,
                          const std::string& name,
                          int window_size = 3,
                          double weight = 1.0);
    
    double evaluate_candidate(const MusicalAnalysisContext& context, 
                              int engine, int candidate_index) const override;
    
    // Predefined pitch pattern functions
    static std::function<double(const std::vector<int>&)> create_stepwise_motion_preference();
    static std::function<double(const std::vector<int>&)> create_consonant_intervals_preference();
    static std::function<double(const std::vector<int>&)> create_melodic_contour_preference();
    static std::function<double(const std::vector<int>&)> create_pitch_stability_preference();
};

/**
 * @brief Single-Engine Rhythm Pattern Heuristic Rule
 * 
 * Analyzes rhythmic patterns within a single rhythm engine to promote
 * rhythmic coherence and musical flow.
 */
class SingleEngineRhythmRule : public HeuristicRule {
private:
    std::function<double(const std::vector<int>&)> rhythm_pattern_function_;
    int analysis_window_size_;
    int target_engine_;

public:
    SingleEngineRhythmRule(int engine,
                           std::function<double(const std::vector<int>&)> pattern_func,
                           const std::string& name,
                           int window_size = 4,
                           double weight = 1.0);
    
    double evaluate_candidate(const MusicalAnalysisContext& context, 
                              int engine, int candidate_index) const override;
    
    // Predefined rhythm pattern functions
    static std::function<double(const std::vector<int>&)> create_rhythmic_regularity_preference();
    static std::function<double(const std::vector<int>&)> create_syncopation_avoidance();
    static std::function<double(const std::vector<int>&)> create_rhythmic_variety_preference();
    static std::function<double(const std::vector<int>&)> create_metric_alignment_preference();
};

/**
 * @brief Dual-Engine Rhythm-Pitch Coordination Rule
 * 
 * Analyzes coordination between rhythm and pitch engines to ensure
 * musically coherent rhythm-pitch relationships.
 */
class DualEngineRhythmPitchRule : public HeuristicRule {
private:
    std::function<double(const std::vector<std::pair<int, int>>&)> coordination_function_;
    int rhythm_engine_;
    int pitch_engine_;
    int analysis_window_size_;

public:
    DualEngineRhythmPitchRule(int rhythm_engine, int pitch_engine,
                              std::function<double(const std::vector<std::pair<int, int>>&)> coord_func,
                              const std::string& name,
                              int window_size = 3,
                              double weight = 1.0);
    
    double evaluate_candidate(const MusicalAnalysisContext& context, 
                              int engine, int candidate_index) const override;
    
    // Predefined coordination functions
    static std::function<double(const std::vector<std::pair<int, int>>&)> create_strong_beat_emphasis();
    static std::function<double(const std::vector<std::pair<int, int>>&)> create_rhythmic_pitch_correlation();
    static std::function<double(const std::vector<std::pair<int, int>>&)> create_cadential_pattern_preference();
};

/**
 * @brief Multi-Engine Harmonic Analysis Rule
 * 
 * Analyzes harmonic relationships across multiple pitch engines to
 * promote consonant harmonies and smooth voice leading.
 */
class MultiEngineHarmonicRule : public HeuristicRule {
private:
    std::function<double(const std::vector<std::vector<int>>&)> harmonic_function_;
    std::vector<int> pitch_engines_;
    int analysis_window_size_;

public:
    MultiEngineHarmonicRule(const std::vector<int>& pitch_engines,
                            std::function<double(const std::vector<std::vector<int>>&)> harm_func,
                            const std::string& name,
                            int window_size = 2,
                            double weight = 1.0);
    
    double evaluate_candidate(const MusicalAnalysisContext& context, 
                              int engine, int candidate_index) const override;
    
    // Predefined harmonic functions
    static std::function<double(const std::vector<std::vector<int>>&)> create_consonance_preference();
    static std::function<double(const std::vector<std::vector<int>>&)> create_voice_leading_smoothness();
    static std::function<double(const std::vector<std::vector<int>>&)> create_chord_progression_logic();
    static std::function<double(const std::vector<std::vector<int>>&)> create_parallel_motion_avoidance();
};

/**
 * @brief Switch-based Heuristic Rule
 * 
 * Binary heuristic rules that return either the specified weight or zero
 * based on logical musical conditions.
 */
class SwitchHeuristicRule : public HeuristicRule {
private:
    std::function<bool(const MusicalAnalysisContext&, int, int)> condition_function_;
    double switch_weight_;

public:
    SwitchHeuristicRule(const std::vector<int>& engines,
                        std::function<bool(const MusicalAnalysisContext&, int, int)> condition,
                        double weight,
                        const std::string& name);
    
    double evaluate_candidate(const MusicalAnalysisContext& context, 
                              int engine, int candidate_index) const override;
    
    // Predefined switch conditions
    static std::function<bool(const MusicalAnalysisContext&, int, int)> create_avoid_repetition_condition();
    static std::function<bool(const MusicalAnalysisContext&, int, int)> create_range_enforcement_condition(int min_pitch, int max_pitch);
    static std::function<bool(const MusicalAnalysisContext&, int, int)> create_metric_position_condition();
};

/**
 * @brief Heuristic Rule Manager
 * 
 * Manages collection of heuristic rules, applies them to candidates,
 * and provides intelligent candidate ordering for search optimization.
 */
class HeuristicRuleManager {
private:
    ClusterEngineCore* core_;
    std::unique_ptr<MusicalAnalysisContext> analysis_context_;
    
    // Rule organization
    std::vector<std::vector<std::unique_ptr<HeuristicRule>>> rules_by_engine_;
    std::vector<std::unique_ptr<HeuristicRule>> global_rules_;
    
    // Performance optimization
    std::vector<std::vector<double>> cached_weights_;
    bool weights_cache_valid_;
    int last_weights_checksum_;
    
    // Statistics and monitoring
    int rule_evaluations_count_;
    double total_evaluation_time_;
    std::map<std::string, int> rule_usage_stats_;

public:
    HeuristicRuleManager(ClusterEngineCore* core);
    ~HeuristicRuleManager();
    
    // Rule management
    void add_rule(std::unique_ptr<HeuristicRule> rule);
    void remove_rule(const std::string& rule_name);
    void enable_rule(const std::string& rule_name);
    void disable_rule(const std::string& rule_name);
    
    // Core heuristic functionality
    void apply_heuristics_to_domain(int engine);
    std::vector<double> calculate_candidate_weights(int engine);
    void sort_domain_by_heuristics(int engine);
    
    // Cache management
    void invalidate_weights_cache();
    void update_analysis_context();
    bool is_cache_valid() const { return weights_cache_valid_; }
    
    // Rule creation helpers
    void add_default_pitch_rules(int engine);
    void add_default_rhythm_rules(int engine);
    void add_default_harmonic_rules(const std::vector<int>& pitch_engines);
    void add_default_coordination_rules(int rhythm_engine, int pitch_engine);
    
    // Performance monitoring
    double get_average_evaluation_time() const;
    int get_total_rule_evaluations() const { return rule_evaluations_count_; }
    void print_rule_statistics() const;
    void reset_statistics();
    
    // Advanced features
    void balance_rule_weights_automatically();
    void optimize_rule_parameters();
    std::vector<std::string> get_most_effective_rules() const;
    void export_rule_configuration(const std::string& filename) const;
    void import_rule_configuration(const std::string& filename);
};

/**
 * @brief Heuristic Rule Factory
 * 
 * Factory class for creating common and specialized heuristic rules
 * with predefined musical intelligence.
 */
class HeuristicRuleFactory {
public:
    // Single-engine rules
    static std::unique_ptr<HeuristicRule> create_stepwise_motion_rule(int engine, double weight = 2.0);
    static std::unique_ptr<HeuristicRule> create_consonant_intervals_rule(int engine, double weight = 1.5);
    static std::unique_ptr<HeuristicRule> create_rhythmic_regularity_rule(int engine, double weight = 1.2);
    static std::unique_ptr<HeuristicRule> create_pitch_stability_rule(int engine, double weight = 1.0);
    
    // Multi-engine rules
    static std::unique_ptr<HeuristicRule> create_rhythm_pitch_coordination_rule(
        int rhythm_engine, int pitch_engine, double weight = 1.8);
    static std::unique_ptr<HeuristicRule> create_harmonic_consonance_rule(
        const std::vector<int>& pitch_engines, double weight = 2.5);
    static std::unique_ptr<HeuristicRule> create_voice_leading_rule(
        const std::vector<int>& pitch_engines, double weight = 2.0);
    
    // Switch rules
    static std::unique_ptr<HeuristicRule> create_no_repetition_switch(
        int engine, double penalty_weight = -5.0);
    static std::unique_ptr<HeuristicRule> create_range_enforcement_switch(
        int engine, int min_pitch, int max_pitch, double penalty_weight = -10.0);
    
    // Advanced composite rules
    static std::unique_ptr<HeuristicRule> create_classical_style_rule(
        int rhythm_engine, int pitch_engine, double weight = 1.5);
    static std::unique_ptr<HeuristicRule> create_jazz_style_rule(
        int rhythm_engine, int pitch_engine, double weight = 1.3);
    static std::unique_ptr<HeuristicRule> create_minimalist_style_rule(
        int engine, double weight = 1.8);
    
    // Complete rule sets for common scenarios
    static void add_classical_music_rules(HeuristicRuleManager& manager, 
                                          const std::vector<int>& rhythm_engines,
                                          const std::vector<int>& pitch_engines);
    static void add_jazz_music_rules(HeuristicRuleManager& manager,
                                     const std::vector<int>& rhythm_engines,
                                     const std::vector<int>& pitch_engines);
    static void add_minimal_rules(HeuristicRuleManager& manager, int num_engines);
};

} // namespace ClusterEngine

#endif // CLUSTER_ENGINE_HEURISTIC_HH