/**
 * @file cluster_engine_interface.hh
 * @brief Main Cluster Engine Interface - Complete Musical AI System
 * 
 * Provides the main interface to the authentic Cluster Engine implementation
 * with all components integrated: multi-engine coordination, heuristic rules,
 * musical domains, sophisticated backjumping, and musical intelligence.
 */

#ifndef CLUSTER_ENGINE_INTERFACE_HH
#define CLUSTER_ENGINE_INTERFACE_HH

#include "cluster_engine_core.hh"
#include "cluster_engine_forward_rules.hh"
#include "cluster_engine_backjump.hh"
#include "cluster_engine_musical_rules.hh"
#include "cluster_engine_domains.hh"  
#include "cluster_engine_stop_rules.hh"
#include <memory>
#include <vector>
#include <functional>
#include <string>

namespace ClusterEngine {

/**
 * @brief Search configuration parameters
 */
struct SearchConfig {
    int max_variables_per_engine = 100;
    int max_search_loops = 1000000;
    BackjumpMode backjump_mode = BackjumpMode::MIN_BACKJUMP;
    ForwardRuleType forward_rule_type = ForwardRuleType::FWD_RULE2;
    bool debug_output = false;
    bool verbose_output = true;
    unsigned int random_seed = 0;
    
    // Search termination criteria
    int max_solutions = 1;
    int max_runtime_seconds = 300; // 5 minutes default
    bool find_all_solutions = false;
    
    // Stop rule configuration
    bool enable_stop_rules = true;
    bool enable_time_stops = true;
    bool enable_index_stops = true;
    bool enable_note_count_stops = false;
    double max_composition_length_seconds = 60.0;
    int max_notes_per_voice = 32;
    int max_measures = 16;
    StopLogic stop_logic = StopLogic::OR;
};

/**
 * @brief Musical composition result
 */
struct MusicalComposition {
    std::vector<std::vector<MusicalCandidate>> voices;  // [voice][note] = (absolute, interval)
    std::vector<TimeSignature> time_signatures;         // Metric structure
    double total_duration;                              // In seconds or beats
    int total_notes;                                    // Across all voices
    
    // Analysis data
    std::vector<std::vector<int>> absolute_pitches;     // [voice][note] = MIDI pitch
    std::vector<std::vector<int>> intervals;            // [voice][note] = interval
    std::vector<std::vector<double>> durations;         // [voice][note] = duration  
    std::vector<std::vector<double>> onset_times;      // [voice][note] = start time
    
    MusicalComposition() : total_duration(0.0), total_notes(0) {}
    
    // Analysis methods
    void calculate_analysis_data();
    int get_num_voices() const { return voices.size(); }
    bool is_empty() const { return voices.empty(); }
    
    // Export helpers
    std::string to_json() const;
    void save_to_midi(const std::string& filename) const;
    void save_to_musicxml(const std::string& filename) const;
};

/**
 * @brief Search statistics and debugging information
 */
struct SearchStatistics {
    int total_search_steps = 0;
    int backjump_count = 0;
    int backstep_count = 0;
    int heuristic_applications = 0;
    int rule_tests_passed = 0;
    int rule_tests_failed = 0;
    double search_time_seconds = 0.0;
    
    // Per-engine statistics
    std::vector<int> engine_forward_steps;
    std::vector<int> engine_backtrack_steps;
    std::vector<double> engine_search_time;
    
    void clear();
    void print_summary() const;
    double get_success_rate() const;
    double get_backjump_efficiency() const;
};

/**
 * @brief Complete Cluster Engine implementation
 */
class ClusterEngineInterface {
private:
    std::unique_ptr<ClusterEngineCore> core_;
    std::unique_ptr<ForwardRuleManager> forward_manager_;
    std::unique_ptr<MusicalBackjumper> backjump_manager_;
    std::unique_ptr<SearchTerminationManager> termination_manager_;
    
    SearchConfig config_;
    SearchStatistics stats_;
    DomainConfiguration domain_config_;
    
    std::vector<std::unique_ptr<MusicalRule>> constraint_rules_;
    std::vector<HeuristicRule> heuristic_rules_;
    
    bool initialized_;
    bool search_running_;
    
    // Internal search methods
    bool execute_search_loop();
    bool test_all_rules_at_current_position();
    void apply_heuristic_sorting();
    void record_search_statistics();
    void setup_stop_rules();
    bool check_stop_conditions();
    
public:
    ClusterEngineInterface();
    ~ClusterEngineInterface();
    
    /**
     * @brief Initialize the engine with domain configuration
     */
    bool initialize(const DomainConfiguration& domain_config, 
                   const SearchConfig& search_config = SearchConfig());
    
    /**
     * @brief Add constraint rule to the system
     */
    void add_constraint_rule(std::unique_ptr<MusicalRule> rule);
    
    /**
     * @brief Add heuristic rule for musical intelligence
     */
    void add_heuristic_rule(const HeuristicRule& rule);
    
    /**
     * @brief Configure stop rules for search termination
     */
    void configure_stop_rules(const std::string& rule_type = "classical", 
                             int max_length = 8,
                             bool enable_advanced = true);
    
    /**
     * @brief Add custom stop rule
     */
    void add_stop_rule(const StopCondition& condition);
    void add_time_stop(const std::vector<int>& voices, double max_time);
    void add_index_stop(const std::vector<int>& voices, int max_index);
    void add_note_count_stop(const std::vector<int>& voices, int max_notes);
    
    /**
     * @brief Clear and reset stop rules
     */
    void clear_stop_rules();
    void reset_stop_state();
    
    /**
     * @brief Enable/disable stop rule evaluation
     */
    void set_stop_rules_enabled(bool enabled);
    bool are_stop_rules_enabled() const;
    
    /**
     * @brief Main search function - find musical compositions
     */
    std::vector<MusicalComposition> search();
    
    /**
     * @brief Search for single composition
     */
    MusicalComposition search_single();
    
    /**
     * @brief Continue search from current state (for iterative solving)
     */
    std::vector<MusicalComposition> continue_search(int additional_solutions = 1);
    
    // Configuration access
    SearchConfig& get_search_config() { return config_; }
    const SearchConfig& get_search_config() const { return config_; }
    
    DomainConfiguration& get_domain_config() { return domain_config_; }
    const DomainConfiguration& get_domain_config() const { return domain_config_; }
    
    // Statistics and monitoring
    const SearchStatistics& get_search_statistics() const { return stats_; }
    void clear_statistics() { stats_.clear(); }
    
    // State management
    bool is_initialized() const { return initialized_; }
    bool is_search_running() const { return search_running_; }
    void reset_search_state();
    
    // Debugging and analysis
    void print_engine_status() const;
    void print_domain_summary() const;
    void print_rule_summary() const;
    void export_search_state(const std::string& filename) const;
    bool import_search_state(const std::string& filename);
};

/**
 * @brief High-level factory for common musical scenarios
 */
class ClusterEngineFactory {
public:
    /**
     * @brief Create engine for classical species counterpoint
     */
    static std::unique_ptr<ClusterEngineInterface> create_counterpoint_engine(
        int num_voices = 2,
        int cantus_firmus_length = 8,
        const std::string& mode = "dorian");
    
    /**
     * @brief Create engine for jazz chord progression harmonization
     */
    static std::unique_ptr<ClusterEngineInterface> create_jazz_harmonization_engine(
        const std::vector<std::string>& chord_progression,
        int num_voices = 4,
        const std::string& style = "bebop");
    
    /**
     * @brief Create engine for contemporary algorithmic composition
     */
    static std::unique_ptr<ClusterEngineInterface> create_algorithmic_composition_engine(
        int num_voices,
        const std::vector<int>& pitch_set,
        const std::vector<TimeSignature>& metric_structure,
        const std::string& technique = "serialism");
    
    /**
     * @brief Create engine for educational music theory exercises
     */
    static std::unique_ptr<ClusterEngineInterface> create_educational_engine(
        const std::string& exercise_type = "four_part_harmony",
        int difficulty_level = 1);
    
    /**
     * @brief Create engine for real-time improvisation assistance
     */
    static std::unique_ptr<ClusterEngineInterface> create_improvisation_engine(
        const std::vector<int>& scale_pitches,
        const std::vector<TimeSignature>& time_signatures = {{4,4}},
        int response_time_ms = 100);
};

/**
 * @brief Factory for common musical constraint rules
 */
class MusicalRuleFactory {
public:
    // Common factory methods - delegates to MusicalRuleLibrary
    static std::unique_ptr<RhythmPitchRule> create_no_repetition_rule(int voice_id);
    static std::unique_ptr<PitchPitchRule> create_consonant_intervals_rule(const std::vector<int>& voices);
    static std::unique_ptr<MusicalRule> create_range_rule(int voice_id, int min_pitch, int max_pitch);
    static std::unique_ptr<MusicalRule> create_stepwise_motion_rule(int voice_id, double preference = 0.7);
};

/**
 * @brief Rule library for common musical constraints
 */
class MusicalRuleLibrary {
public:
    // Classical counterpoint rules
    static std::unique_ptr<MusicalRule> create_no_parallel_fifths_rule(const std::vector<int>& voices);
    static std::unique_ptr<MusicalRule> create_no_parallel_octaves_rule(const std::vector<int>& voices);
    static std::unique_ptr<MusicalRule> create_stepwise_motion_rule(int voice_id, double preference = 0.7);
    static std::unique_ptr<MusicalRule> create_consonant_intervals_rule(const std::vector<int>& voices);
    
    // Jazz harmony rules
    static std::unique_ptr<MusicalRule> create_jazz_voice_leading_rule(const std::vector<int>& voices);
    static std::unique_ptr<MusicalRule> create_chord_tone_rule(const std::vector<int>& voices, const std::vector<int>& chord_tones);
    static std::unique_ptr<MusicalRule> create_bebop_scale_rule(int voice_id, const std::vector<int>& scale);
    
    // Contemporary rules  
    static std::unique_ptr<MusicalRule> create_twelve_tone_rule(const std::vector<int>& voices, const std::vector<int>& row);
    static std::unique_ptr<MusicalRule> create_pitch_class_set_rule(const std::vector<int>& voices, const std::vector<int>& pc_set);
    static std::unique_ptr<MusicalRule> create_rhythmic_pattern_rule(int voice_id, const std::vector<int>& pattern);
    
    // Educational rules
    static std::unique_ptr<MusicalRule> create_range_rule(int voice_id, int min_pitch, int max_pitch);
    static std::unique_ptr<MusicalRule> create_cadence_rule(const std::vector<int>& voices, const std::string& cadence_type);
    static std::unique_ptr<MusicalRule> create_phrase_structure_rule(const std::vector<int>& voices, int phrase_length);
};

/**
 * @brief Heuristic rule library for musical intelligence
 */
class MusicalHeuristicLibrary {
public:
    // Melodic heuristics
    static HeuristicRule create_melodic_contour_preference(int voice_id, const std::string& contour_type = "arch");
    static HeuristicRule create_stepwise_motion_preference(int voice_id, double step_weight = 2.0);
    static HeuristicRule create_consonance_preference(const std::vector<int>& voices, double consonance_weight = 1.5);
    
    // Rhythmic heuristics
    static HeuristicRule create_strong_beat_preference(int voice_id, double strong_beat_weight = 1.3);
    static HeuristicRule create_rhythmic_variety_preference(int voice_id, double variety_weight = 1.2);
    
    // Harmonic heuristics  
    static HeuristicRule create_voice_independence_preference(const std::vector<int>& voices, double independence_weight = 1.4);
    static HeuristicRule create_harmonic_rhythm_preference(const std::vector<int>& voices, int preferred_change_rate = 2);
    
    // Style-specific heuristics
    static HeuristicRule create_jazz_improvisation_preference(int voice_id, const std::vector<int>& chord_tones);
    static HeuristicRule create_classical_voice_leading_preference(const std::vector<int>& voices);
    static HeuristicRule create_contemporary_dissonance_preference(const std::vector<int>& voices, double dissonance_weight = 0.8);
};

} // namespace ClusterEngine

#endif // CLUSTER_ENGINE_INTERFACE_HH