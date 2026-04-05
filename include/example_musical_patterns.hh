// ===================================================================
// Example Musical Patterns - Hardcoded Rule Examples
// ===================================================================
//
// This file contains example musical pattern rules that can be used
// as templates or loaded into the dynamic pattern-based rule system.
// These are NOT hardcoded into the main system - they serve as examples
// of how to create musical pattern rules that can be passed as arguments.
//
// Usage: These examples can be instantiated and passed to the main
// constraint solver as dynamic rule arguments, following the JBS-Constraints
// approach where rules are external parameters rather than hardcoded logic.
//
// ===================================================================

#ifndef EXAMPLE_MUSICAL_PATTERNS_HH
#define EXAMPLE_MUSICAL_PATTERNS_HH

#include "pattern_based_rules.hh"
#include <vector>
#include <memory>

namespace MusicalConstraints {

/**
 * @brief Collection of example musical pattern rules
 * 
 * These serve as templates that can be instantiated and passed
 * as arguments to the constraint solver. They demonstrate different
 * types of musical intelligence that can be dynamically configured.
 */
class ExampleMusicalPatterns {
public:
    /**
     * @brief Create a simple ascending scale motif rule
     */
    static std::unique_ptr<PatternBasedRule> create_scale_motif_rule() {
        std::vector<int> scale_motif = {60, 62, 64, 65}; // C-D-E-F
        return std::unique_ptr<PatternBasedRule>(
            new MotifRule(scale_motif, 4, true, 0.75)
        );
    }
    
    /**
     * @brief Create a triad arpeggio pattern rule
     */
    static std::unique_ptr<PatternBasedRule> create_triad_arpeggio_rule() {
        std::vector<int> triad_arpeggio = {60, 64, 67}; // C-E-G
        return std::unique_ptr<PatternBasedRule>(
            new MotifRule(triad_arpeggio, 6, true, 0.8)
        );
    }
    
    /**
     * @brief Create a melodic sequence pattern rule
     */
    static std::unique_ptr<PatternBasedRule> create_sequence_rule() {
        std::vector<int> sequence_motif = {60, 62, 64}; // C-D-E
        return std::unique_ptr<PatternBasedRule>(
            new SequenceRule(sequence_motif, 2, 3, 0.7)
        );
    }
    
    /**
     * @brief Create a stepwise motion preference rule
     */
    static std::unique_ptr<PatternBasedRule> create_stepwise_motion_rule() {
        PatternTemplate stepwise_template(PatternType::CONTOUR_PATTERN,
                                         "Stepwise melodic motion", 0.1, false, true);
        stepwise_template.create_from_sequence({60, 61, 62, 63}); // Half steps
        return std::unique_ptr<PatternBasedRule>(
            new PatternBasedRule(stepwise_template, 0.6, false, "Prefer stepwise motion")
        );
    }
    
    /**
     * @brief Create jazz-style chromatic approach rule
     */
    static std::unique_ptr<PatternBasedRule> create_chromatic_approach_rule() {
        PatternTemplate chromatic_template(PatternType::TRANSPOSED_REPETITION,
                                          "Jazz chromatic approach", 0.2, true, true);
        // Example: approach note pattern (half-step below target)
        chromatic_template.create_from_sequence({59, 60}); // B->C approach
        return std::unique_ptr<PatternBasedRule>(
            new PatternBasedRule(chromatic_template, 0.7, false, "Jazz chromatic approach")
        );
    }
    
    /**
     * @brief Create common chord progression pattern rule
     */
    static std::unique_ptr<PatternBasedRule> create_chord_progression_rule() {
        PatternTemplate progression_template(PatternType::HARMONIC_PROGRESSION,
                                           "ii-V-I progression", 0.15, false, false);
        // Example: Dm-G-C progression in C major
        progression_template.create_from_sequence({62, 67, 60}); // D-G-C roots
        return std::unique_ptr<PatternBasedRule>(
            new PatternBasedRule(progression_template, 0.8, true, "Common jazz progression")
        );
    }
    
    /**
     * @brief Get a collection of common jazz pattern rules
     * 
     * These can be passed as arguments to the constraint solver
     * for jazz-style musical generation.
     */
    static std::vector<std::unique_ptr<PatternBasedRule>> get_jazz_pattern_rules() {
        std::vector<std::unique_ptr<PatternBasedRule>> jazz_rules;
        jazz_rules.push_back(create_triad_arpeggio_rule());
        jazz_rules.push_back(create_chromatic_approach_rule());
        jazz_rules.push_back(create_chord_progression_rule());
        return jazz_rules;
    }
    
    /**
     * @brief Get a collection of classical pattern rules
     */
    static std::vector<std::unique_ptr<PatternBasedRule>> get_classical_pattern_rules() {
        std::vector<std::unique_ptr<PatternBasedRule>> classical_rules;
        classical_rules.push_back(create_scale_motif_rule());
        classical_rules.push_back(create_sequence_rule());
        classical_rules.push_back(create_stepwise_motion_rule());
        return classical_rules;
    }
    
    /**
     * @brief Get all example pattern rules
     */
    static std::vector<std::unique_ptr<PatternBasedRule>> get_all_example_rules() {
        std::vector<std::unique_ptr<PatternBasedRule>> all_rules;
        all_rules.push_back(create_scale_motif_rule());
        all_rules.push_back(create_triad_arpeggio_rule());
        all_rules.push_back(create_sequence_rule());
        all_rules.push_back(create_stepwise_motion_rule());
        all_rules.push_back(create_chromatic_approach_rule());
        all_rules.push_back(create_chord_progression_rule());
        return all_rules;
    }
};

} // namespace MusicalConstraints

#endif // EXAMPLE_MUSICAL_PATTERNS_HH