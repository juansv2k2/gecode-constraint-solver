/**
 * @file cluster_engine_rules.hh
 * @brief Core Rules Interface System for ClusterEngine
 * 
 * This file contains the complete rules interface system that translates the
 * musical constraint logic from the original Cluster Engine Lisp implementation.
 * This is the heart of the constraint solver that enforces musical relationships.
 */

#ifndef CLUSTER_ENGINE_RULES_HH
#define CLUSTER_ENGINE_RULES_HH

#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <string>
#include <array>

namespace ClusterEngine {

// Forward declarations
class ClusterEngineCore;
class RuleCompilationContext;

/**
 * @brief Base types for rule system
 */
using MusicalValue = double;
using MusicalSequence = std::vector<MusicalValue>;
using BacktrackRoute = std::vector<int>;
using EngineList = std::vector<int>;

/**
 * @brief Rule evaluation result
 */
enum class RuleResult {
    PASS = 0,           // Rule constraint satisfied
    FAIL = 1,           // Rule constraint violated
    INSUFFICIENT_DATA = 2  // Not enough data to evaluate rule
};

/**
 * @brief Musical rule execution context
 * 
 * Provides access to the current solution state for rule evaluation
 */
struct RuleExecutionContext {
    const ClusterEngineCore* engine_core;
    const std::vector<std::vector<std::vector<MusicalValue>>>* vsolution;
    const std::vector<std::vector<MusicalSequence>>* vlinear_solution; 
    const std::vector<int>* vindex;
    const std::vector<std::vector<double>>* vsolution_for_backjump;
    const std::vector<std::vector<int>>* vbackjump_indexes;
    int current_engine;
    
    RuleExecutionContext(const ClusterEngineCore* core,
                        const std::vector<std::vector<std::vector<MusicalValue>>>* sol,
                        const std::vector<std::vector<MusicalSequence>>* linear_sol,
                        const std::vector<int>* index,
                        const std::vector<std::vector<double>>* sol_backjump,
                        const std::vector<std::vector<int>>* backjump_idx,
                        int engine)
        : engine_core(core), vsolution(sol), vlinear_solution(linear_sol),
          vindex(index), vsolution_for_backjump(sol_backjump), 
          vbackjump_indexes(backjump_idx), current_engine(engine) {}
};

/**
 * @class MusicalRule
 * @brief Base class for all musical constraint rules
 * 
 * Represents a single musical constraint that can be evaluated against
 * the current solution state. Rules can examine patterns within single
 * engines or relationships between multiple engines.
 */
class MusicalRule {
public:
    /**
     * @brief Rule type enumeration
     */
    enum class RuleType {
        SINGLE_ENGINE_RHYTHM,      // Rhythmic patterns within one voice
        SINGLE_ENGINE_PITCH,       // Pitch patterns within one voice
        DUAL_ENGINE_RHYTHM_PITCH,  // Rhythm-pitch coordination
        MULTI_ENGINE_HARMONIC,     // Multi-voice harmonic constraints
        METRIC_CONSTRAINT,         // Metric/time signature constraints
        VOICE_LEADING,            // Voice leading between multiple voices
        CUSTOM                    // User-defined custom rules
    };

protected:
    std::string name_;
    RuleType type_;
    EngineList target_engines_;
    BacktrackRoute backtrack_route_;
    int priority_;
    bool enabled_;

public:
    MusicalRule(const std::string& name, RuleType type, 
                const EngineList& engines, const BacktrackRoute& backtrack_route,
                int priority = 1)
        : name_(name), type_(type), target_engines_(engines), 
          backtrack_route_(backtrack_route), priority_(priority), enabled_(true) {}

    virtual ~MusicalRule() = default;

    /**
     * @brief Evaluate rule against current solution state
     * @param context Current execution context with solution data
     * @return Rule evaluation result
     */
    virtual RuleResult evaluate(const RuleExecutionContext& context) = 0;

    /**
     * @brief Get engines that this rule affects
     */
    const EngineList& get_target_engines() const { return target_engines_; }

    /**
     * @brief Get backtrack route for when this rule fails
     */
    const BacktrackRoute& get_backtrack_route() const { return backtrack_route_; }

    /**
     * @brief Get rule name for debugging and logging
     */
    const std::string& get_name() const { return name_; }

    /**
     * @brief Get rule type
     */
    RuleType get_type() const { return type_; }

    /**
     * @brief Get rule priority (higher = more important)
     */
    int get_priority() const { return priority_; }

    /**
     * @brief Enable/disable rule
     */
    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }

    /**
     * @brief Check if rule can be evaluated with current solution state
     */
    virtual bool can_evaluate(const RuleExecutionContext& context) = 0;

    /**
     * @brief Get description of what this rule checks
     */
    virtual std::string get_description() const = 0;
};

/**
 * @class SingleEngineRule
 * @brief Base for rules that examine patterns within a single engine
 */
class SingleEngineRule : public MusicalRule {
protected:
    int engine_id_;
    int window_size_;  // Number of cells to examine
    
public:
    SingleEngineRule(const std::string& name, RuleType type, int engine_id,
                     const BacktrackRoute& backtrack_route, int window_size,
                     int priority = 1)
        : MusicalRule(name, type, {engine_id}, backtrack_route, priority),
          engine_id_(engine_id), window_size_(window_size) {}

    int get_engine_id() const { return engine_id_; }
    int get_window_size() const { return window_size_; }

    bool can_evaluate(const RuleExecutionContext& context) override {
        return (*context.vindex)[engine_id_] >= (window_size_ - 1);
    }

protected:
    /**
     * @brief Get sequence of cells from current index backward
     */
    std::vector<MusicalSequence> get_cell_sequence(const RuleExecutionContext& context) const;

    /**
     * @brief Get current cell being evaluated
     */
    const MusicalSequence& get_current_cell(const RuleExecutionContext& context) const;

    /**
     * @brief Get total duration/pitch count for engine
     */
    int get_total_count(const RuleExecutionContext& context) const;
};

/**
 * @class DualEngineRule  
 * @brief Base for rules that examine relationships between two engines
 */
class DualEngineRule : public MusicalRule {
protected:
    int engine1_id_;
    int engine2_id_;
    int window_size_;

public:
    DualEngineRule(const std::string& name, RuleType type, 
                   int engine1_id, int engine2_id,
                   const BacktrackRoute& backtrack_route, int window_size,
                   int priority = 1)
        : MusicalRule(name, type, {engine1_id, engine2_id}, backtrack_route, priority),
          engine1_id_(engine1_id), engine2_id_(engine2_id), window_size_(window_size) {}

    bool can_evaluate(const RuleExecutionContext& context) override {
        return (*context.vindex)[engine1_id_] >= 0 && 
               (*context.vindex)[engine2_id_] >= 0 &&
               get_synchronized_length(context) >= window_size_;
    }

protected:
    /**
     * @brief Get synchronized pairs between two engines
     */
    std::vector<std::pair<MusicalValue, MusicalValue>> 
    get_synchronized_pairs(const RuleExecutionContext& context) const;

    /**
     * @brief Get synchronized length between engines
     */
    int get_synchronized_length(const RuleExecutionContext& context) const;
};

/**
 * @class MultiEngineRule
 * @brief Base for rules that examine relationships between multiple engines
 */
class MultiEngineRule : public MusicalRule {
protected:
    std::vector<int> engine_ids_;
    int window_size_;

public:
    MultiEngineRule(const std::string& name, RuleType type,
                    const std::vector<int>& engine_ids,
                    const BacktrackRoute& backtrack_route, int window_size,
                    int priority = 1)
        : MusicalRule(name, type, engine_ids, backtrack_route, priority),
          engine_ids_(engine_ids), window_size_(window_size) {}

    bool can_evaluate(const RuleExecutionContext& context) override {
        for (int engine_id : engine_ids_) {
            if ((*context.vindex)[engine_id] < 0) return false;
        }
        return get_minimum_synchronized_length(context) >= window_size_;
    }

protected:
    /**
     * @brief Get harmonic slices across multiple engines
     */
    std::vector<std::vector<MusicalValue>> 
    get_harmonic_slices(const RuleExecutionContext& context) const;

    /**
     * @brief Get minimum synchronized length across all engines
     */
    int get_minimum_synchronized_length(const RuleExecutionContext& context) const;
};

/**
 * @class RuleCompiler
 * @brief Compiles simple rule functions into executable musical rules
 * 
 * Translates user-defined constraint functions into optimized MusicalRule objects
 * that can efficiently access the engine solution state.
 */
class RuleCompiler {
public:
    /**
     * @brief Simple rule function type for single engine rules
     */
    using SimpleRuleFunction1 = std::function<bool(const MusicalSequence&)>;
    using SimpleRuleFunction2 = std::function<bool(const MusicalSequence&, const MusicalSequence&)>;
    using SimpleRuleFunction3 = std::function<bool(const MusicalSequence&, const MusicalSequence&, const MusicalSequence&)>;

    /**
     * @brief Simple rule function type for dual engine rules
     */
    using SimpleRuleFunctionDual = std::function<bool(const std::vector<std::pair<MusicalValue, MusicalValue>>&)>;

    /**
     * @brief Simple rule function type for multi engine harmonies
     */
    using SimpleRuleFunctionMulti = std::function<bool(const std::vector<std::vector<MusicalValue>>&)>;

    /**
     * @brief Compile single engine cell rule
     */
    static std::unique_ptr<MusicalRule> compile_single_engine_cells_rule(
        const std::string& name, int engine_id, 
        SimpleRuleFunction1 rule_function,
        const BacktrackRoute& backtrack_route = {});

    static std::unique_ptr<MusicalRule> compile_single_engine_cells_rule(
        const std::string& name, int engine_id,
        SimpleRuleFunction2 rule_function,
        const BacktrackRoute& backtrack_route = {});

    static std::unique_ptr<MusicalRule> compile_single_engine_cells_rule(
        const std::string& name, int engine_id,
        SimpleRuleFunction3 rule_function,
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Compile dual engine rhythm-pitch rule
     */
    static std::unique_ptr<MusicalRule> compile_dual_engine_rhythm_pitch_rule(
        const std::string& name, int rhythm_engine, int pitch_engine,
        SimpleRuleFunctionDual rule_function,
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Compile multi engine harmonic rule
     */
    static std::unique_ptr<MusicalRule> compile_multi_engine_harmonic_rule(
        const std::string& name, const std::vector<int>& engine_ids,
        SimpleRuleFunctionMulti rule_function,
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Create index-based rule for specific positions
     */
    static std::unique_ptr<MusicalRule> compile_index_rule(
        const std::string& name, int engine_id,
        const std::vector<int>& indexes,
        std::function<bool(const std::vector<MusicalSequence>&)> rule_function,
        const BacktrackRoute& backtrack_route = {});
};

/**
 * @class RuleManager
 * @brief Manages collection of musical rules and their execution
 */
class RuleManager {
private:
    // Rules organized by engine for efficient lookup
    std::vector<std::vector<std::unique_ptr<MusicalRule>>> engine_rules_;
    std::vector<std::unique_ptr<MusicalRule>> global_rules_;
    
    // Rule execution statistics
    struct RuleStats {
        int evaluations = 0;
        int passes = 0;
        int failures = 0;
    };
    std::map<std::string, RuleStats> rule_statistics_;
    
    // Locked engines (cannot be backtracked)
    std::vector<int> locked_engines_;

public:
    RuleManager(int num_engines);
    ~RuleManager() = default;

    /**
     * @brief Add a rule to the system
     */
    void add_rule(std::unique_ptr<MusicalRule> rule);

    /**
     * @brief Remove rule by name
     */
    bool remove_rule(const std::string& name);

    /**
     * @brief Test all rules for a specific engine
     * @return Backtrack route if any rule fails, empty if all pass
     */
    BacktrackRoute test_rules(int engine, const RuleExecutionContext& context);

    /**
     * @brief Test all global rules (not engine-specific)
     */
    BacktrackRoute test_global_rules(const RuleExecutionContext& context);

    /**
     * @brief Get all rules for an engine
     */
    const std::vector<std::unique_ptr<MusicalRule>>& get_rules_for_engine(int engine) const;

    /**
     * @brief Set locked engines that cannot be backtracked
     */
    void set_locked_engines(const std::vector<int>& locked_engines);

    /**
     * @brief Get rule statistics
     */
    const std::map<std::string, RuleStats>& get_statistics() const { return rule_statistics_; }

    /**
     * @brief Enable/disable specific rule
     */
    bool set_rule_enabled(const std::string& name, bool enabled);

    /**
     * @brief Filter backtrack route to exclude locked engines
     */
    BacktrackRoute filter_backtrack_route(const BacktrackRoute& route) const;

    /**
     * @brief Get total number of rules
     */
    size_t get_total_rule_count() const;

    /**
     * @brief Clear all rules
     */
    void clear_all_rules();

private:
    void update_rule_statistics(const std::string& rule_name, RuleResult result);
};

} // namespace ClusterEngine

#endif // CLUSTER_ENGINE_RULES_HH