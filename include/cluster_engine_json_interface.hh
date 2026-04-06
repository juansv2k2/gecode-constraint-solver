/**
 * @file cluster_engine_json_interface.hh
 * @brief JSON Configuration Interface for Cluster Engine
 * 
 * Provides a JSON-based interface that mirrors the original Lisp cluster engine,
 * allowing users to specify complex musical constraint problems through configuration files.
 */

#ifndef CLUSTER_ENGINE_JSON_INTERFACE_HH
#define CLUSTER_ENGINE_JSON_INTERFACE_HH

#include "enhanced_rule_architecture.hh"
#include "advanced_backjumping_strategies.hh"
#include "dual_solution_storage.hh"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace ClusterEngineJSON {

// ===============================
// JSON Configuration Structures
// ===============================

/**
 * @brief Represents a fraction value (for rhythmic durations)
 */
struct Fraction {
    int numerator;
    int denominator;
    
    double to_double() const { return static_cast<double>(numerator) / denominator; }
    std::string to_string() const { return std::to_string(numerator) + "/" + std::to_string(denominator); }
};

/**
 * @brief Constraint function specification from JSON
 */
struct ConstraintFunction {
    enum Type {
        BUILTIN,
        LAMBDA
    } type;
    
    // For builtin functions
    enum BuiltinFunction {
        NOT_EQUAL,
        EQUAL, 
        LESS_THAN,
        GREATER_THAN,
        INTERVAL_SIZE,
        CONSONANT_INTERVAL,
        STEPWISE,
        JUMP_RESOLUTION
    } builtin_function;
    
    std::vector<double> parameters;
    
    // For lambda functions
    std::vector<std::string> variables;
    std::string expression;
};

/**
 * @brief Musical rule specification from JSON
 */
struct RuleSpec {
    enum RuleType {
        R_PITCHES_ONE_VOICE,
        R_INDEX_PITCHES_ONE_VOICE,
        R_CHORDS,
        R_JBS_ONE_VOICE,
        R_METRIC_HIERARCHY,
        R_RHYTHM_HIERARCHY,
        R_PITCHES_TWO_VOICES,
        R_RHYTHMS_ONE_VOICE,
        R_CUSTOM
    } rule_type;
    
    ConstraintFunction constraint_function;
    std::vector<int> indices;
    int voice = -1;  // -1 means applies to all voices
    
    enum EngineType {
        RHYTHM,
        PITCH, 
        METRIC
    } engine_type;
    
    bool enabled = true;
    int priority = 5;
    std::string description;
};

/**
 * @brief Metric domain specification
 */
struct MetricDomain {
    std::vector<std::pair<int, int>> time_signatures;  // [(4,4), (3,4), etc.]
    std::vector<std::vector<int>> subdivisions;
    std::vector<std::vector<int>> beat_structures;
};

/**
 * @brief Voice domain specification (rhythm + pitch)
 */
struct VoiceDomain {
    int voice_index;
    std::vector<Fraction> rhythm_domain;
    std::vector<int> pitch_domain;
};

/**
 * @brief Domain specification for all engines
 */
struct DomainSpec {
    MetricDomain metric_domain;
    std::vector<VoiceDomain> voice_domains;
};

/**
 * @brief Search configuration
 */
struct SearchConfig {
    int max_iterations = 10000;
    double timeout_seconds = 30.0;
    bool enable_heuristics = true;
    bool debug_mode = false;
};

/**
 * @brief Complete cluster engine configuration from JSON
 */
struct ClusterConfig {
    int solution_length;
    int num_voices;
    
    enum BacktrackMethod {
        CHRONOLOGICAL,
        INTELLIGENT, 
        MUSICAL,
        MINIMAL
    } backtrack_method;
    
    std::vector<RuleSpec> rules;
    DomainSpec domains;
    SearchConfig search_config;
    
    std::map<std::string, std::string> metadata;
};

// ===============================
// Engine Mapping System
// ===============================

/**
 * @brief Maps voices to engines following cluster conventions
 * 
 * Engine mapping for N voices:
 * - Engine 0: Rhythm Voice 0
 * - Engine 1: Pitch Voice 0  
 * - Engine 2: Rhythm Voice 1
 * - Engine 3: Pitch Voice 1
 * - ...
 * - Engine 2*N: Global Metric Domain
 */
class EngineMapper {
public:
    explicit EngineMapper(int num_voices) : num_voices_(num_voices) {
        total_engines_ = 2 * num_voices + 1;
    }
    
    int get_rhythm_engine(int voice) const {
        return 2 * voice;
    }
    
    int get_pitch_engine(int voice) const {
        return 2 * voice + 1;
    }
    
    int get_metric_engine() const {
        return 2 * num_voices_;
    }
    
    int get_total_engines() const {
        return total_engines_;
    }
    
    bool is_rhythm_engine(int engine) const {
        return engine < 2 * num_voices_ && engine % 2 == 0;
    }
    
    bool is_pitch_engine(int engine) const {
        return engine < 2 * num_voices_ && engine % 2 == 1;
    }
    
    bool is_metric_engine(int engine) const {
        return engine == 2 * num_voices_;
    }
    
    int get_voice_for_engine(int engine) const {
        if (is_metric_engine(engine)) return -1;
        return engine / 2;
    }
    
private:
    int num_voices_;
    int total_engines_;
};

// ===============================
// Constraint Function Factory
// ===============================

/**
 * @brief Creates constraint functions from JSON specifications
 */
class ConstraintFunctionFactory {
public:
    using ConstraintFunc = std::function<bool(const std::vector<int>&)>;
    
    static ConstraintFunc create_constraint(const ConstraintFunction& spec);
    
private:
    static ConstraintFunc create_builtin_constraint(ConstraintFunction::BuiltinFunction func, 
                                                   const std::vector<double>& params);
    static ConstraintFunc create_lambda_constraint(const std::vector<std::string>& variables,
                                                  const std::string& expression);
};

// ===============================
// Main JSON Interface Class
// ===============================

/**
 * @brief Main interface for loading and executing JSON cluster configurations
 */
class ClusterEngineJSONInterface {
public:
    /**
     * @brief Load configuration from JSON file
     */
    bool load_configuration(const std::string& json_file_path);
    
    /**
     * @brief Load configuration from JSON string
     */
    bool load_configuration_string(const std::string& json_string);
    
    /**
     * @brief Execute loaded configuration  
     */
    bool execute();
    
    /**
     * @brief Get solution results
     */
    const std::vector<std::vector<int>>& get_solutions() const { return solutions_; }
    
    /**
     * @brief Get engine assignments
     */
    const EngineMapper& get_engine_mapper() const { return *engine_mapper_; }
    
    /**
     * @brief Execution statistics structure
     */
    struct ExecutionStats {
        int total_rules_checked = 0;
        int backjumps_performed = 0; 
        double solve_time_ms = 0.0;
        bool found_solution = false;
        std::string failure_reason;
        
        std::vector<int> rules_per_engine;
        std::vector<double> engine_solve_times;
    };
    
    /**
     * @brief Get execution statistics
     */
    ExecutionStats get_execution_stats() const { return execution_stats_; }
    
    /**
     * @brief Export results to various formats
     */
    std::string export_to_json() const;
    bool export_to_midi(const std::string& filename) const;
    bool export_to_xml(const std::string& filename = "", 
                       const std::string& output_dir = "tests/output") const;
    void print_solution_summary(std::ostream& os = std::cout) const;
    
    /**
     * @brief Configuration validation
     */
    bool validate_configuration() const;
    std::vector<std::string> get_validation_errors() const { return validation_errors_; }
    
private:
    ClusterConfig config_;
    std::unique_ptr<EngineMapper> engine_mapper_;
    
    // Solution storage
    std::vector<std::vector<int>> solutions_;  // Solutions per engine
    ExecutionStats execution_stats_;
    
    // Validation
    mutable std::vector<std::string> validation_errors_;
    
    // Internal methods
    bool parse_json(const std::string& json_content);
    bool setup_engines();
    bool setup_rules();
    bool setup_domains();
    bool run_solver();
    
    // JSON parsing helpers
    RuleSpec::RuleType parse_rule_type(const std::string& type_str) const;
    ConstraintFunction parse_constraint_function(const std::map<std::string, std::string>& func_obj) const;
    ClusterConfig::BacktrackMethod parse_backtrack_method(const std::string& method_str) const;
};

// ===============================
// Convenience Functions
// ===============================

/**
 * @brief Quick interface for simple cases
 */
namespace QuickInterface {
    
    /**
     * @brief Solve a musical constraint problem from JSON file
     */
    bool solve_from_file(const std::string& json_file, 
                        std::vector<std::vector<int>>& solutions,
                        ClusterEngineJSONInterface::ExecutionStats& stats);
    
    /**
     * @brief Create a simple configuration programmatically
     */
    std::string create_simple_config(int solution_length, 
                                   int num_voices,
                                   const std::vector<int>& pitch_range,
                                   const std::vector<std::string>& rule_types = {"no_repetitions", "stepwise"});
    
    /**
     * @brief Validate a JSON configuration file
     */
    bool validate_config_file(const std::string& json_file,
                             std::vector<std::string>& errors);
}

} // namespace ClusterEngineJSON

#endif // CLUSTER_ENGINE_JSON_INTERFACE_HH