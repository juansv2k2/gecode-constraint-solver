/**
 * @file cluster_engine_rules_implementation.cpp
 * @brief Implementation of the Core Rules Interface System
 * 
 * This file contains the complete implementation of the musical constraint
 * rules system translated from the original Cluster Engine Lisp code.
 */

#include "cluster_engine_rules.hh"
#include "cluster_engine_interface.hh"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <iostream>

namespace ClusterEngine {

// =============================================================================
// Helper Functions for Rule System
// =============================================================================

namespace {
    /**
     * @brief Count notes excluding rests (negative values)
     */
    int count_notes_excluding_rests(const MusicalSequence& sequence) {
        return std::count_if(sequence.begin(), sequence.end(), 
                           [](MusicalValue val) { return val >= 0; });
    }

    /**
     * @brief Count total events (notes + rests)
     */
    int count_total_events(const MusicalSequence& sequence) {
        return static_cast<int>(sequence.size());
    }

    /**
     * @brief Check if engine is a pitch engine (odd numbered)
     */
    bool is_pitch_engine(int engine_id) {
        return engine_id % 2 == 1;
    }

    /**
     * @brief Check if engine is a rhythm engine (even numbered)
     */
    bool is_rhythm_engine(int engine_id) {
        return engine_id % 2 == 0;
    }

    /**
     * @brief Get musical interval between two pitches
     */
    double get_interval(MusicalValue pitch1, MusicalValue pitch2) {
        return std::abs(pitch2 - pitch1);
    }

    /**
     * @brief Calculate melodic contour direction
     */
    int get_contour_direction(MusicalValue pitch1, MusicalValue pitch2) {
        if (pitch2 > pitch1) return 1;   // ascending
        if (pitch2 < pitch1) return -1;  // descending
        return 0;                        // same
    }

    /**
     * @brief Check if interval is consonant
     */
    bool is_consonant_interval(double interval) {
        // Perfect unison, octave, fifth, fourth, major/minor thirds and sixths
        double mod_interval = fmod(interval, 12.0);
        return (mod_interval == 0 || mod_interval == 3 || mod_interval == 4 ||
                mod_interval == 5 || mod_interval == 7 || mod_interval == 8 ||
                mod_interval == 9 || mod_interval == 12);
    }
}

// =============================================================================
// SingleEngineRule Implementation
// =============================================================================

std::vector<MusicalSequence> SingleEngineRule::get_cell_sequence(
    const RuleExecutionContext& context) const {
    
    std::vector<MusicalSequence> sequence;
    int current_index = (*context.vindex)[engine_id_];
    
    for (int i = std::max(0, current_index - window_size_ + 1); i <= current_index; ++i) {
        const auto& solution = (*context.vsolution)[engine_id_][i];
        if (!solution.empty()) {
            sequence.push_back(solution[0]); // Get first candidate
        }
    }
    
    return sequence;
}

const MusicalSequence& SingleEngineRule::get_current_cell(
    const RuleExecutionContext& context) const {
    
    int current_index = (*context.vindex)[engine_id_];
    const auto& solution = (*context.vsolution)[engine_id_][current_index];
    
    if (solution.empty()) {
        throw std::runtime_error("No candidates available at current index for engine " + 
                               std::to_string(engine_id_));
    }
    
    return solution[0]; // Return first candidate
}

int SingleEngineRule::get_total_count(const RuleExecutionContext& context) const {
    if (is_pitch_engine(engine_id_)) {
        // For pitch engines, count total pitches (excluding rests)
        int total = 0;
        for (const auto& seq : (*context.vlinear_solution)[engine_id_]) {
            total += count_notes_excluding_rests(seq);
        }
        return total;
    } else {
        // For rhythm engines, count total durations
        int total = 0;
        for (const auto& seq : (*context.vlinear_solution)[engine_id_]) {
            total += count_total_events(seq);
        }
        return total;
    }
}

// =============================================================================
// DualEngineRule Implementation
// =============================================================================

std::vector<std::pair<MusicalValue, MusicalValue>> 
DualEngineRule::get_synchronized_pairs(const RuleExecutionContext& context) const {
    
    std::vector<std::pair<MusicalValue, MusicalValue>> pairs;
    
    // Get linear solutions for both engines
    const auto& linear1 = (*context.vlinear_solution)[engine1_id_];
    const auto& linear2 = (*context.vlinear_solution)[engine2_id_];
    
    // Synchronize based on note positions (excluding rests for pitches)
    size_t pos1 = 0, pos2 = 0;
    
    for (size_t seq_idx = 0; seq_idx < std::min(linear1.size(), linear2.size()); ++seq_idx) {
        const auto& seq1 = linear1[seq_idx];
        const auto& seq2 = linear2[seq_idx];
        
        // Handle rhythm-pitch synchronization
        if (is_rhythm_engine(engine1_id_) && is_pitch_engine(engine2_id_)) {
            // Rhythm-pitch pairing
            for (size_t i = 0; i < std::min(seq1.size(), seq2.size()); ++i) {
                if (seq2[i] >= 0) { // Only include pitched notes
                    pairs.emplace_back(seq1[i], seq2[i]);
                }
            }
        } else if (is_pitch_engine(engine1_id_) && is_rhythm_engine(engine2_id_)) {
            // Pitch-rhythm pairing
            for (size_t i = 0; i < std::min(seq1.size(), seq2.size()); ++i) {
                if (seq1[i] >= 0) { // Only include pitched notes
                    pairs.emplace_back(seq1[i], seq2[i]);
                }
            }
        } else {
            // Same type pairing
            for (size_t i = 0; i < std::min(seq1.size(), seq2.size()); ++i) {
                pairs.emplace_back(seq1[i], seq2[i]);
            }
        }
    }
    
    return pairs;
}

int DualEngineRule::get_synchronized_length(const RuleExecutionContext& context) const {
    return static_cast<int>(get_synchronized_pairs(context).size());
}

// =============================================================================
// MultiEngineRule Implementation  
// =============================================================================

std::vector<std::vector<MusicalValue>> 
MultiEngineRule::get_harmonic_slices(const RuleExecutionContext& context) const {
    
    std::vector<std::vector<MusicalValue>> slices;
    
    // Find the minimum length across all engines
    int min_length = get_minimum_synchronized_length(context);
    
    for (int slice_idx = 0; slice_idx < min_length; ++slice_idx) {
        std::vector<MusicalValue> slice;
        
        for (int engine_id : engine_ids_) {
            const auto& linear = (*context.vlinear_solution)[engine_id];
            
            // Extract value at this synchronized position
            int pos = 0;
            bool found = false;
            
            for (const auto& seq : linear) {
                if (pos + seq.size() > slice_idx) {
                    // Value is in this sequence
                    size_t local_idx = slice_idx - pos;
                    if (local_idx < seq.size()) {
                        slice.push_back(seq[local_idx]);
                        found = true;
                        break;
                    }
                }
                pos += seq.size();
            }
            
            if (!found) {
                slice.push_back(-1); // Rest or no value
            }
        }
        
        slices.push_back(slice);
    }
    
    return slices;
}

int MultiEngineRule::get_minimum_synchronized_length(
    const RuleExecutionContext& context) const {
    
    int min_length = std::numeric_limits<int>::max();
    
    for (int engine_id : engine_ids_) {
        const auto& linear = (*context.vlinear_solution)[engine_id];
        int engine_length = 0;
        
        for (const auto& seq : linear) {
            if (is_pitch_engine(engine_id)) {
                engine_length += count_notes_excluding_rests(seq);
            } else {
                engine_length += count_total_events(seq);
            }
        }
        
        min_length = std::min(min_length, engine_length);
    }
    
    return min_length == std::numeric_limits<int>::max() ? 0 : min_length;
}

// =============================================================================
// Concrete Rule Implementations
// =============================================================================

/**
 * @brief Single engine cells rule implementation
 */
template<typename RuleFunctionType>
class SingleEngineCellsRule : public SingleEngineRule {
private:
    RuleFunctionType rule_function_;
    
public:
    SingleEngineCellsRule(const std::string& name, int engine_id,
                         RuleFunctionType rule_function,
                         const BacktrackRoute& backtrack_route, int window_size)
        : SingleEngineRule(name, 
                         is_pitch_engine(engine_id) ? RuleType::SINGLE_ENGINE_PITCH : RuleType::SINGLE_ENGINE_RHYTHM,
                         engine_id, backtrack_route, window_size),
          rule_function_(rule_function) {}

    RuleResult evaluate(const RuleExecutionContext& context) override {
        if (!can_evaluate(context)) {
            return RuleResult::INSUFFICIENT_DATA;
        }

        try {
            std::vector<MusicalSequence> sequence = get_cell_sequence(context);
            
            // Apply rule function based on window size
            bool result = false;
            if constexpr (std::is_invocable_v<RuleFunctionType, const MusicalSequence&>) {
                if (sequence.size() >= 1) {
                    result = rule_function_(sequence.back());
                }
            } else if constexpr (std::is_invocable_v<RuleFunctionType, const MusicalSequence&, const MusicalSequence&>) {
                if (sequence.size() >= 2) {
                    result = rule_function_(sequence[sequence.size()-2], sequence.back());
                }
            } else if constexpr (std::is_invocable_v<RuleFunctionType, const MusicalSequence&, const MusicalSequence&, const MusicalSequence&>) {
                if (sequence.size() >= 3) {
                    result = rule_function_(sequence[sequence.size()-3], sequence[sequence.size()-2], sequence.back());
                }
            }
            
            return result ? RuleResult::PASS : RuleResult::FAIL;
            
        } catch (const std::exception& e) {
            std::cerr << "Error evaluating rule " << get_name() << ": " << e.what() << std::endl;
            return RuleResult::FAIL;
        }
    }

    std::string get_description() const override {
        return "Single engine cells rule for engine " + std::to_string(engine_id_) + 
               " with window size " + std::to_string(window_size_);
    }
};

/**
 * @brief Dual engine rhythm-pitch rule implementation
 */
class DualEngineRhythmPitchRule : public DualEngineRule {
private:
    std::function<bool(const std::vector<std::pair<MusicalValue, MusicalValue>>&)> rule_function_;
    
public:
    DualEngineRhythmPitchRule(const std::string& name, int rhythm_engine, int pitch_engine,
                             std::function<bool(const std::vector<std::pair<MusicalValue, MusicalValue>>&)> rule_function,
                             const BacktrackRoute& backtrack_route, int window_size)
        : DualEngineRule(name, RuleType::DUAL_ENGINE_RHYTHM_PITCH,
                        rhythm_engine, pitch_engine, backtrack_route, window_size),
          rule_function_(rule_function) {}

    RuleResult evaluate(const RuleExecutionContext& context) override {
        if (!can_evaluate(context)) {
            return RuleResult::INSUFFICIENT_DATA;
        }

        try {
            auto pairs = get_synchronized_pairs(context);
            
            // Apply rule to the last 'window_size_' pairs
            if (pairs.size() >= window_size_) {
                std::vector<std::pair<MusicalValue, MusicalValue>> window_pairs(
                    pairs.end() - window_size_, pairs.end());
                
                bool result = rule_function_(window_pairs);
                return result ? RuleResult::PASS : RuleResult::FAIL;
            }
            
            return RuleResult::INSUFFICIENT_DATA;
            
        } catch (const std::exception& e) {
            std::cerr << "Error evaluating dual engine rule " << get_name() << ": " << e.what() << std::endl;
            return RuleResult::FAIL;
        }
    }

    std::string get_description() const override {
        return "Dual engine rhythm-pitch rule between engines " + 
               std::to_string(engine1_id_) + " and " + std::to_string(engine2_id_);
    }
};

/**
 * @brief Multi engine harmonic rule implementation
 */
class MultiEngineHarmonicRule : public MultiEngineRule {
private:
    std::function<bool(const std::vector<std::vector<MusicalValue>>&)> rule_function_;
    
public:
    MultiEngineHarmonicRule(const std::string& name, const std::vector<int>& engine_ids,
                           std::function<bool(const std::vector<std::vector<MusicalValue>>&)> rule_function,
                           const BacktrackRoute& backtrack_route, int window_size)
        : MultiEngineRule(name, RuleType::MULTI_ENGINE_HARMONIC,
                         engine_ids, backtrack_route, window_size),
          rule_function_(rule_function) {}

    RuleResult evaluate(const RuleExecutionContext& context) override {
        if (!can_evaluate(context)) {
            return RuleResult::INSUFFICIENT_DATA;
        }

        try {
            auto slices = get_harmonic_slices(context);
            
            // Apply rule to the last 'window_size_' slices
            if (slices.size() >= window_size_) {
                std::vector<std::vector<MusicalValue>> window_slices(
                    slices.end() - window_size_, slices.end());
                
                bool result = rule_function_(window_slices);
                return result ? RuleResult::PASS : RuleResult::FAIL;
            }
            
            return RuleResult::INSUFFICIENT_DATA;
            
        } catch (const std::exception& e) {
            std::cerr << "Error evaluating multi engine rule " << get_name() << ": " << e.what() << std::endl;
            return RuleResult::FAIL;
        }
    }

    std::string get_description() const override {
        std::string desc = "Multi engine harmonic rule for engines: ";
        for (size_t i = 0; i < engine_ids_.size(); ++i) {
            if (i > 0) desc += ", ";
            desc += std::to_string(engine_ids_[i]);
        }
        return desc;
    }
};

// =============================================================================
// RuleCompiler Implementation
// =============================================================================

std::unique_ptr<MusicalRule> RuleCompiler::compile_single_engine_cells_rule(
    const std::string& name, int engine_id,
    SimpleRuleFunction1 rule_function,
    const BacktrackRoute& backtrack_route) {
    
    return std::make_unique<SingleEngineCellsRule<SimpleRuleFunction1>>(
        name, engine_id, rule_function, backtrack_route, 1);
}

std::unique_ptr<MusicalRule> RuleCompiler::compile_single_engine_cells_rule(
    const std::string& name, int engine_id,
    SimpleRuleFunction2 rule_function,
    const BacktrackRoute& backtrack_route) {
    
    return std::make_unique<SingleEngineCellsRule<SimpleRuleFunction2>>(
        name, engine_id, rule_function, backtrack_route, 2);
}

std::unique_ptr<MusicalRule> RuleCompiler::compile_single_engine_cells_rule(
    const std::string& name, int engine_id,
    SimpleRuleFunction3 rule_function,
    const BacktrackRoute& backtrack_route) {
    
    return std::make_unique<SingleEngineCellsRule<SimpleRuleFunction3>>(
        name, engine_id, rule_function, backtrack_route, 3);
}

std::unique_ptr<MusicalRule> RuleCompiler::compile_dual_engine_rhythm_pitch_rule(
    const std::string& name, int rhythm_engine, int pitch_engine,
    SimpleRuleFunctionDual rule_function,
    const BacktrackRoute& backtrack_route) {
    
    return std::make_unique<DualEngineRhythmPitchRule>(
        name, rhythm_engine, pitch_engine, rule_function, backtrack_route, 1);
}

std::unique_ptr<MusicalRule> RuleCompiler::compile_multi_engine_harmonic_rule(
    const std::string& name, const std::vector<int>& engine_ids,
    SimpleRuleFunctionMulti rule_function,
    const BacktrackRoute& backtrack_route) {
    
    return std::make_unique<MultiEngineHarmonicRule>(
        name, engine_ids, rule_function, backtrack_route, 1);
}

std::unique_ptr<MusicalRule> RuleCompiler::compile_index_rule(
    const std::string& name, int engine_id,
    const std::vector<int>& indexes,
    std::function<bool(const std::vector<MusicalSequence>&)> rule_function,
    const BacktrackRoute& backtrack_route) {
    
    // Index rules require special implementation (simplified version)
    class IndexRule : public SingleEngineRule {
    private:
        std::vector<int> indexes_;
        std::function<bool(const std::vector<MusicalSequence>&)> rule_function_;
        
    public:
        IndexRule(const std::string& name, int engine_id,
                 const std::vector<int>& indexes,
                 std::function<bool(const std::vector<MusicalSequence>&)> rule_function,
                 const BacktrackRoute& backtrack_route)
            : SingleEngineRule(name, 
                             is_pitch_engine(engine_id) ? RuleType::SINGLE_ENGINE_PITCH : RuleType::SINGLE_ENGINE_RHYTHM,
                             engine_id, backtrack_route, *std::max_element(indexes.begin(), indexes.end()) + 1),
              indexes_(indexes), rule_function_(rule_function) {}

        bool can_evaluate(const RuleExecutionContext& context) override {
            int current_index = (*context.vindex)[engine_id_];
            return current_index >= *std::max_element(indexes_.begin(), indexes_.end());
        }

        RuleResult evaluate(const RuleExecutionContext& context) override {
            if (!can_evaluate(context)) {
                return RuleResult::INSUFFICIENT_DATA;
            }

            try {
                std::vector<MusicalSequence> indexed_cells;
                int current_index = (*context.vindex)[engine_id_];
                
                for (int idx : indexes_) {
                    int target_index = current_index - (*std::max_element(indexes_.begin(), indexes_.end()) - idx);
                    if (target_index >= 0) {
                        const auto& solution = (*context.vsolution)[engine_id_][target_index];
                        if (!solution.empty()) {
                            indexed_cells.push_back(solution[0]);
                        }
                    }
                }

                bool result = rule_function_(indexed_cells);
                return result ? RuleResult::PASS : RuleResult::FAIL;
                
            } catch (const std::exception& e) {
                std::cerr << "Error evaluating index rule " << get_name() << ": " << e.what() << std::endl;
                return RuleResult::FAIL;
            }
        }

        std::string get_description() const override {
            return "Index rule for engine " + std::to_string(engine_id_) + 
                   " at indexes: [implementation detail]";
        }
    };
    
    return std::make_unique<IndexRule>(name, engine_id, indexes, rule_function, backtrack_route);
}

// =============================================================================
// RuleManager Implementation
// =============================================================================

RuleManager::RuleManager(int num_engines) {
    engine_rules_.resize(num_engines);
}

void RuleManager::add_rule(std::unique_ptr<MusicalRule> rule) {
    if (!rule) return;
    
    const auto& target_engines = rule->get_target_engines();
    
    if (target_engines.size() == 1) {
        // Single engine rule
        int engine_id = target_engines[0];
        if (engine_id >= 0 && engine_id < static_cast<int>(engine_rules_.size())) {
            engine_rules_[engine_id].push_back(std::move(rule));
        }
    } else {
        // Multi-engine or global rule
        global_rules_.push_back(std::move(rule));
    }
}

bool RuleManager::remove_rule(const std::string& name) {
    // Search in engine rules
    for (auto& engine_rule_list : engine_rules_) {
        auto it = std::find_if(engine_rule_list.begin(), engine_rule_list.end(),
                              [&name](const std::unique_ptr<MusicalRule>& rule) {
                                  return rule->get_name() == name;
                              });
        if (it != engine_rule_list.end()) {
            engine_rule_list.erase(it);
            return true;
        }
    }
    
    // Search in global rules
    auto it = std::find_if(global_rules_.begin(), global_rules_.end(),
                          [&name](const std::unique_ptr<MusicalRule>& rule) {
                              return rule->get_name() == name;
                          });
    if (it != global_rules_.end()) {
        global_rules_.erase(it);
        return true;
    }
    
    return false;
}

BacktrackRoute RuleManager::test_rules(int engine, const RuleExecutionContext& context) {
    BacktrackRoute failed_routes;
    
    if (engine < 0 || engine >= static_cast<int>(engine_rules_.size())) {
        return failed_routes;
    }
    
    // Test all rules for this engine
    for (const auto& rule : engine_rules_[engine]) {
        if (!rule->is_enabled()) continue;
        
        RuleResult result = rule->evaluate(context);
        update_rule_statistics(rule->get_name(), result);
        
        if (result == RuleResult::FAIL) {
            // Rule failed - get backtrack route
            BacktrackRoute route = filter_backtrack_route(rule->get_backtrack_route());
            failed_routes.insert(failed_routes.end(), route.begin(), route.end());
        }
    }
    
    return failed_routes;
}

BacktrackRoute RuleManager::test_global_rules(const RuleExecutionContext& context) {
    BacktrackRoute failed_routes;
    
    for (const auto& rule : global_rules_) {
        if (!rule->is_enabled()) continue;
        
        RuleResult result = rule->evaluate(context);
        update_rule_statistics(rule->get_name(), result);
        
        if (result == RuleResult::FAIL) {
            BacktrackRoute route = filter_backtrack_route(rule->get_backtrack_route());
            failed_routes.insert(failed_routes.end(), route.begin(), route.end());
        }
    }
    
    return failed_routes;
}

const std::vector<std::unique_ptr<MusicalRule>>& 
RuleManager::get_rules_for_engine(int engine) const {
    static std::vector<std::unique_ptr<MusicalRule>> empty_vector;
    
    if (engine < 0 || engine >= static_cast<int>(engine_rules_.size())) {
        return empty_vector;
    }
    
    return engine_rules_[engine];
}

void RuleManager::set_locked_engines(const std::vector<int>& locked_engines) {
    locked_engines_ = locked_engines;
}

bool RuleManager::set_rule_enabled(const std::string& name, bool enabled) {
    // Search in all rules
    for (auto& engine_rule_list : engine_rules_) {
        for (auto& rule : engine_rule_list) {
            if (rule->get_name() == name) {
                rule->set_enabled(enabled);
                return true;
            }
        }
    }
    
    for (auto& rule : global_rules_) {
        if (rule->get_name() == name) {
            rule->set_enabled(enabled);
            return true;
        }
    }
    
    return false;
}

BacktrackRoute RuleManager::filter_backtrack_route(const BacktrackRoute& route) const {
    BacktrackRoute filtered_route;
    
    for (int engine : route) {
        if (std::find(locked_engines_.begin(), locked_engines_.end(), engine) == locked_engines_.end()) {
            filtered_route.push_back(engine);
        }
    }
    
    return filtered_route;
}

size_t RuleManager::get_total_rule_count() const {
    size_t count = global_rules_.size();
    
    for (const auto& engine_rule_list : engine_rules_) {
        count += engine_rule_list.size();
    }
    
    return count;
}

void RuleManager::clear_all_rules() {
    for (auto& engine_rule_list : engine_rules_) {
        engine_rule_list.clear();
    }
    global_rules_.clear();
    rule_statistics_.clear();
}

void RuleManager::update_rule_statistics(const std::string& rule_name, RuleResult result) {
    auto& stats = rule_statistics_[rule_name];
    stats.evaluations++;
    
    switch (result) {
        case RuleResult::PASS:
            stats.passes++;
            break;
        case RuleResult::FAIL:
            stats.failures++;
            break;
        case RuleResult::INSUFFICIENT_DATA:
            // Don't count as pass or fail
            break;
    }
}

} // namespace ClusterEngine