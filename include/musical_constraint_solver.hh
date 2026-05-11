/**
 * @file musical_constraint_solver.hh
 * @brief Main Interface for Production Musical Constraint Solver
 * 
 * Unified interface that brings together all cluster-engine functionality
 * with Gecode constraint programming for real-world musical generation.
 */

#ifndef MUSICAL_CONSTRAINT_SOLVER_HH
#define MUSICAL_CONSTRAINT_SOLVER_HH

#include "enhanced_rule_architecture.hh"
#include "dual_solution_storage.hh"
#include "gecode_cluster_integration.hh"
#include "dynamic_rule_compiler.hh"
#include "rule_expression_parser.hh"
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <limits>
#include <cstdint>

namespace MusicalConstraintSolver {

/**
 * @brief Configuration options for musical constraint solving
 */
struct SolverConfig {
    struct MetricDomainEntry {
        int numerator = 4;
        int denominator = 4;
        std::vector<int> tuplets;
        std::vector<int> beat_divisions;
    };

    // Basic sequence parameters
    int sequence_length = 8;
    int num_voices = 1;

    // Search configuration aligned to Gecode search concepts.
    enum class SearchEngine {
        DFS
    } search_engine = SearchEngine::DFS;

    GecodeClusterIntegration::VariableBranchingStrategy variable_branching =
        GecodeClusterIntegration::VariableBranchingStrategy::FIRST_FAIL;

    GecodeClusterIntegration::ValueSelectionStrategy value_selection =
        GecodeClusterIntegration::ValueSelectionStrategy::MIN;

    enum class RestartPolicy {
        NONE
    } restart_policy = RestartPolicy::NONE;

    bool enable_musical_intelligence = true;
    int max_search_iterations = 10000;
    double timeout_seconds = 30.0;
    
    // Musical style parameters
    enum MusicalStyle {
        CLASSICAL,
        JAZZ,
        CONTEMPORARY,
        MINIMAL,
        CUSTOM
    } style = CLASSICAL;
    
    // Output format
    bool verbose_output = true;
    bool show_rule_statistics = false;
    
    // Per-voice pitch domains. Required — voice_domains[v] defines allowed MIDI values for voice v.
    std::vector<std::vector<int>> voice_domains;

    // Per-voice rhythm domains. voice_rhythm_domains[v] = allowed duration values
    // for voice v, expressed as multiples of one tick (tick = 1/rhythm_base of a whole note).
    // Required — no fallback. Throws at solve time if empty.
    std::vector<std::vector<int>> voice_rhythm_domains;

    // Base (LCM of all duration denominators) used for rhythm_vars encoding.
    // e.g. if all durations are simple fractions (1/4, 1/8) rhythm_base = 8 and
    // a quarter note = 2, an eighth note = 1.
    // Updated by getVoiceRhythmDomains(); used by display logic.
    int rhythm_base = 1;

    // Optional total score span expressed in ticks using rhythm_base as the whole-note unit.
    // -1 means unspecified and preserves current event-count-driven behavior.
    int score_length_ticks = -1;

    // If true, solved voice durations must exactly fill score_length_ticks.
    // If false, they may end earlier but may never overflow the score span.
    bool require_exact_score_length = false;

    // Metric domain candidates used by the metric engine.
    // Phase-0/1 scaffold: this data is intentionally passive until
    // metric-engine activation is enabled and posting logic is wired.
    std::vector<MetricDomainEntry> metric_domain;

    // Guardrail flag for incremental rollout of metric as a first-class engine.
    bool enable_metric_engine = false;

    // Random search seed semantics:
    // - std::numeric_limits<unsigned int>::max(): deterministic search order
    // - 0: generate a fresh random seed for each solve
    // - any other value: reproducible randomized search using that seed
    unsigned int random_seed = std::numeric_limits<unsigned int>::max();

    // Domain constraints
    int max_interval_size = 12;  // Octave
    bool allow_repetitions = false;
    bool prefer_stepwise_motion = true;
    
    // Performance tuning
    bool enable_performance_optimization = true;
    bool cache_rule_results = true;
    int rule_cache_size = 1000;

    // Heuristic selector tuning (optional)
    // heuristic_top_k:
    // - 0: evaluate full domain (exact)
    // - >0: evaluate only first K candidate values from domain iterator (approximate, faster)
    int heuristic_top_k = 0;
    bool heuristic_trace = false;
    
    // Solution enumeration
    // max_solutions:
    // - 1: find one solution (default)
    // - -1 or any negative: find all solutions (exhaustive)
    // - N > 1: find up to N solutions
    int max_solutions = 1;
};

/**
 * @brief Musical solution representation
 */
struct MusicalSolution {
    struct MetricSegment {
        int start_tick = 0;
        int end_tick = 0;
        int start_index = 0;
        int end_index = 0;
        int numerator = 4;
        int denominator = 4;
    };

    struct ScoreEvent {
        int voice = 0;
        int index = 0;
        int pitch = 60;
        int rhythm = 0;
        int onset_ticks = 0;
        int duration_ticks = 0;
        bool is_rest = false;
    };

    struct ScoreMeasure {
        int measure_index = 0;
        int start_ticks = 0;
        int end_ticks = 0;
        int numerator = 4;
        int denominator = 4;
        std::vector<ScoreEvent> events;
    };

    struct SolvedScore {
        int rhythm_base = 1;
        std::vector<MetricSegment> metric_timeline;
        std::vector<ScoreMeasure> measures;
    };

    std::vector<int> absolute_notes;  // Legacy compatibility - first voice
    std::vector<int> intervals;       // Legacy compatibility - calculated from first voice
    std::vector<std::string> note_names;  // Legacy compatibility - first voice note names
    
    // Multi-voice solution data
    std::vector<std::vector<int>> voice_solutions;  // All voices - each vector is one voice
                                                    // -1 (REST_PITCH_SENTINEL) means rest at that position
    std::vector<std::vector<int>> voice_rhythms;    // Rhythm data for each voice; negative = rest duration
    std::vector<int> metric_signature;              // Metric/time signature data

    // Canonical score-level solved representation (phase-1 scaffold).
    // When available, this is the single source of truth and legacy arrays are projections.
    bool has_canonical_score = false;
    SolvedScore canonical_score;
    
    // Analysis data
    int total_rules_checked = 0;
    int backjumps_performed = 0;
    double solve_time_ms = 0.0;
    bool found_solution = false;
    std::string failure_reason;
    
    // Musical analysis
    double average_interval_size = 0.0;
    int melodic_direction_changes = 0;
    std::vector<std::string> applied_rules;
    
    void print_solution(std::ostream& os = std::cout) const;
    void export_to_midi(const std::string& filename) const;
    void export_to_xml(const std::string& filename) const;
    void export_to_png(const std::string& filename) const;
    std::string to_json() const;
    std::string to_xml() const;
    std::string to_musicxml() const;
};

// ===============================
// Rule Factory System
// ===============================

/**
 * @brief Factory for creating common musical constraint rule sets
 */
class MusicalRuleFactory {
public:
    /**
     * @brief Create basic musical rules
     */
    static std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> 
    create_basic_rules();
    
    /**
     * @brief Create jazz-oriented rules
     */
    static std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>
    create_jazz_rules();
    
    /**
     * @brief Create classical voice-leading rules
     */
    static std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>
    create_voice_leading_rules();
    
    /**
     * @brief Create contemporary/experimental rules
     */
    static std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>
    create_contemporary_rules();
    
    /**
     * @brief Create custom rule set from configuration
     */
    static std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>
    create_custom_rules(const SolverConfig& config);
    
    /**
     * @brief Get recommended rules for a musical style
     */
    static std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>
    get_style_rules(SolverConfig::MusicalStyle style);
};

// ===============================
// Main Solver Interface
// ===============================

/**
 * @brief Primary interface for musical constraint solving
 * 
 * Integrates all cluster-engine functionality with a clean, easy-to-use API
 * for real-world musical generation applications.
 */
class Solver {
private:
    // Engine rule configuration for cluster-engine targeting
    struct EngineRuleConfig {
        std::string rule_type;
        std::string function;
        std::vector<int> indices;
        std::vector<std::string> timepoints;
        int target_engine;
        std::vector<int> target_engines;  // For cross-engine rules
        std::string engine_type;
        std::string description;
        std::vector<double> parameters;
        std::vector<std::string> parameter_strings;
    };
    
    SolverConfig config_;
    std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> rules_;
    std::vector<EngineRuleConfig> engine_rule_configs_;  // Engine-targeted rules
    std::unique_ptr<MusicalConstraints::DualSolutionStorage> solution_storage_;
    
    // Performance tracking
    mutable std::map<std::string, double> performance_stats_;
    mutable int total_solutions_found_ = 0;
    mutable int total_solve_attempts_ = 0;
    
    // Retrograde Inversion tracking
    bool retrograde_inversion_enabled_ = false;
    int retrograde_inversion_center_ = 65;
    
    // Advanced constraint flags 
    bool perfect_fifth_intervals_enabled_ = false;
    bool twelve_tone_row_enabled_ = false;
    bool palindrome_voices_enabled_ = false;
    
    // Dynamic rule system (NEW)
    std::unique_ptr<DynamicRules::CompiledRuleSet> compiled_rules_;
    std::vector<nlohmann::json> dynamic_rule_configs_;

public:
    /**
     * @brief Constructor with default configuration
     */
    Solver();
    
    /**
     * @brief Constructor with configuration
     */
    explicit Solver(const SolverConfig& config);
    
    /**
     * @brief Copy constructor (disabled for performance)
     */
    Solver(const Solver&) = delete;
    Solver& operator=(const Solver&) = delete;
    
    /**
     * @brief Destructor
     */
    ~Solver() = default;
    
    // ===============================
    // Configuration Management
    // ===============================
    
    /**
     * @brief Update solver configuration
     */
    void configure(const SolverConfig& config);
    
    /**
     * @brief Get current configuration
     */
    const SolverConfig& get_config() const { return config_; }
    
    /**
     * @brief Quick setup for common scenarios
     */
    void setup_for_style(SolverConfig::MusicalStyle style);
    void setup_for_jazz_improvisation();
    void setup_for_classical_melody();
    void setup_for_experimental_music();
    
    // ===============================
    // Rule Management
    // ===============================
    
    /**
     * @brief Add musical rule to solver
     */
    void add_rule(std::shared_ptr<MusicalConstraints::MusicalRule> rule);
    
    /**
     * @brief Add rule with engine targeting information
     */
    void add_rule_config(const std::string& rule_type, const std::string& function, 
                        const std::vector<int>& indices, int target_engine, 
                        const std::vector<int>& target_engines,
                        const std::string& engine_type, const std::string& description,
                        const std::vector<double>& parameters = {},
                        const std::vector<std::string>& parameter_strings = {},
                        const std::vector<std::string>& timepoints = {});
    
    /**
     * @brief Add multiple rules
     */
    void add_rules(const std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>& rules);
    
    /**
     * @brief Clear all rules
     */
    void clear_rules();
    
    /**
     * @brief Get current rules count
     */
    size_t get_rules_count() const { return rules_.size(); }
    
    /**
     * @brief Auto-configure rules based on style
     */
    void auto_configure_rules();
    
    // ===============================
    // Dynamic Rule Management (NEW)
    // ===============================
    
    /**
     * @brief Add dynamic rule from JSON configuration
     */
    void add_dynamic_rule(const nlohmann::json& rule_json);
    
    /**
     * @brief Add dynamic rule from JSON string
     */
    void add_dynamic_rule(const std::string& rule_json_string);
    
    /**
     * @brief Load dynamic rules from JSON configuration array
     */
    void load_dynamic_rules(const std::vector<nlohmann::json>& rules_array);
    
    /**
     * @brief Clear all dynamic rules
     */
    void clear_dynamic_rules();
    
    /**
     * @brief Apply compiled wildcard constraint directly to solver
     * @param compiled_constraint The pre-compiled wildcard constraint to apply
     */
    void apply_compiled_constraint(std::unique_ptr<DynamicRules::CompiledConstraint> compiled_constraint);
    
    /**
     * @brief Get count of compiled dynamic rules
     */
    size_t get_dynamic_rules_count() const;

    /**
     * @brief Get count of dynamic constraints posted successfully in latest space build
     */
    int get_dynamic_rule_post_success_count() const;

    /**
     * @brief Get count of dynamic constraints that failed posting in latest space build
     */
    int get_dynamic_rule_post_failed_count() const;
    
    // ===============================
    // Solving Interface
    // ===============================
    
    /**
     * @brief Solve for single musical sequence
     */
    MusicalSolution solve();
    
    /**
     * @brief Solve for multiple solutions.
     * @param max_solutions Number of solutions to find, or -1 for all.
     * @param timeout_ms    Stop after this many milliseconds (0 = no limit).
     * @param on_solution   Optional callback invoked immediately when each solution is found.
     */
    std::vector<MusicalSolution> solve_multiple(
        int max_solutions = 1,
        int timeout_ms = 0,
        std::function<void(const MusicalSolution&, int)> on_solution = nullptr);
    
    /**
     * @brief Solve with custom starting note
     */
    MusicalSolution solve_with_starting_note(int starting_note);
    
    /**
     * @brief Solve with constraint on specific positions
     */
    MusicalSolution solve_with_constraints(const std::map<int, int>& fixed_notes);
    
    /**
     * @brief Interactive solving with user feedback
     */
    MusicalSolution solve_interactive(std::function<bool(const MusicalSolution&)> user_feedback);
    
    // ===============================
    // Analysis and Statistics
    // ===============================
    
    /**
     * @brief Get performance statistics
     */
    std::map<std::string, double> get_performance_stats() const;
    
    /**
     * @brief Get rule application statistics
     */
    std::map<std::string, int> get_rule_statistics() const;
    
    /**
     * @brief Reset performance counters
     */
    void reset_statistics();
    
    // ===============================
    // Utility Functions
    // ===============================
    
    /**
     * @brief Validate current configuration
     */
    bool validate_configuration(std::string& error_message) const;
    
    /**
     * @brief Test rules against sample sequence
     */
    bool test_rules(std::vector<int> test_sequence, std::string& report) const;
    
    /**
     * @brief Get recommended configuration for problem size
     */
    static SolverConfig get_recommended_config(int sequence_length, int complexity_level = 1);
    
    /**
     * @brief Convert MIDI note to note name
     */
    static std::string midi_to_note_name(int midi_note);
    
    /**
     * @brief Calculate interval name
     */
    static std::string interval_to_name(int semitones);
    
    /**
     * @brief Export solution directly to XML file
     */
    bool export_solution_to_xml(const MusicalSolution& solution, const std::string& filename) const;
    
    /**
     * @brief Export solution to PNG image file
     */
    bool export_solution_to_png(const MusicalSolution& solution, const std::string& filename) const;

    /**
     * @brief Solve and immediately export to XML
     */
    bool solve_and_export_xml(const std::string& filename);
    
    /**
     * @brief Solve and immediately export to PNG
     */
    bool solve_and_export_png(const std::string& filename);

private:
    void initialize_solver();
    
    /**
     * @brief Perform actual constraint solving (single solution)
     */
    MusicalSolution solve_internal();

    /**
     * @brief Build a fully-configured Gecode space with all constraints posted.
     * Caller (or the DFS engine) takes ownership of the returned pointer.
     */
    GecodeClusterIntegration::IntegratedMusicalSpace* build_configured_space_();

    /**
     * @brief Extract a MusicalSolution from an already-solved Gecode space.
     * Takes ownership of solved_space and deletes it. Pass nullptr to signal
     * that the search engine found no solution.
     */
    MusicalSolution extract_solution_from_space_(
        GecodeClusterIntegration::IntegratedMusicalSpace* solved_space);

    /**
     * @brief Validate solved candidates for onset-based r-metric-hierarchy modes.
     */
    bool validate_metric_hierarchy_solution_(const MusicalSolution& solution,
                                             std::string& failure_reason) const;
    
    /**
     * @brief Update performance statistics
     */
    void update_stats(const std::string& operation, double time_ms);
    
    /**
     * @brief Create solution from storage
     */
    MusicalSolution create_solution_from_storage() const;
};

// ===============================
// Convenience Functions
// ===============================

/**
 * @brief Quick solve with default settings
 */
MusicalSolution quick_solve(int length = 8, SolverConfig::MusicalStyle style = SolverConfig::CLASSICAL);

/**
 * @brief Solve jazz improvisation
 */
MusicalSolution solve_jazz_improvisation(int length = 16);

/**
 * @brief Solve classical melody
 */
MusicalSolution solve_classical_melody(int length = 8);

/**
 * @brief Batch solve multiple sequences
 */
std::vector<MusicalSolution> batch_solve(int num_sequences, const SolverConfig& config);

} // namespace MusicalConstraintSolver

#endif // MUSICAL_CONSTRAINT_SOLVER_HH