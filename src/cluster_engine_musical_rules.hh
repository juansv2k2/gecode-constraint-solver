/**
 * @file cluster_engine_musical_rules.hh
 * @brief Musical Rule Factories and Common Constraint Functions
 * 
 * This file provides pre-built musical constraint functions that implement
 * common musical rules translated from the cluster-engine Lisp system.
 */

#ifndef CLUSTER_ENGINE_MUSICAL_RULES_HH
#define CLUSTER_ENGINE_MUSICAL_RULES_HH

#include "cluster_engine_rules.hh"
#include <memory>

namespace ClusterEngine {
namespace MusicalRules {

/**
 * @class MusicalRuleFactory
 * @brief Factory for creating common musical constraint rules
 */
class MusicalRuleFactory {
public:
    // =============================================================================
    // Single Engine Rhythm Rules
    // =============================================================================

    /**
     * @brief No repeated durations rule
     */
    static std::unique_ptr<MusicalRule> create_no_repeated_durations(
        int rhythm_engine, const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Rhythmic canon rule (repeated rhythmic patterns)
     */
    static std::unique_ptr<MusicalRule> create_rhythmic_canon(
        int rhythm_engine, const MusicalSequence& pattern, 
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Accelerando rule (gradually decreasing note values)
     */ 
    static std::unique_ptr<MusicalRule> create_accelerando(
        int rhythm_engine, const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Ritardando rule (gradually increasing note values)
     */
    static std::unique_ptr<MusicalRule> create_ritardando(
        int rhythm_engine, const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Metric accent rule (stronger beats get longer durations)
     */
    static std::unique_ptr<MusicalRule> create_metric_accent(
        int rhythm_engine, const BacktrackRoute& backtrack_route = {});

    // =============================================================================
    // Single Engine Pitch Rules  
    // =============================================================================

    /**
     * @brief No repeated pitches rule
     */
    static std::unique_ptr<MusicalRule> create_no_repeated_pitches(
        int pitch_engine, const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Stepwise motion rule (prefer semitone/tone steps)
     */
    static std::unique_ptr<MusicalRule> create_stepwise_motion(
        int pitch_engine, const BacktrackRoute& backtrack_route = {});

    /**
     * @brief No large leaps rule (avoid intervals > octave)
     */
    static std::unique_ptr<MusicalRule> create_no_large_leaps(
        int pitch_engine, int max_interval = 12, 
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Melodic contour rule (control melodic direction)
     */
    static std::unique_ptr<MusicalRule> create_melodic_contour(
        int pitch_engine, const std::vector<int>& contour_pattern,
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Pitch range rule (keep pitches within specified range)
     */
    static std::unique_ptr<MusicalRule> create_pitch_range(
        int pitch_engine, MusicalValue min_pitch, MusicalValue max_pitch,
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Scale membership rule (pitches must belong to specified scale)
     */
    static std::unique_ptr<MusicalRule> create_scale_membership(
        int pitch_engine, const std::vector<int>& scale_degrees,
        const BacktrackRoute& backtrack_route = {});

    // =============================================================================
    // Two Engine Rhythm-Pitch Rules
    // =============================================================================

    /**
     * @brief Strong beat emphasis (higher/longer pitches on strong beats)
     */
    static std::unique_ptr<MusicalRule> create_strong_beat_emphasis(
        int rhythm_engine, int pitch_engine,
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Syncopation avoidance (avoid syncopated rhythms)
     */
    static std::unique_ptr<MusicalRule> create_no_syncopation(
        int rhythm_engine, int pitch_engine,
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Cadential pattern (specific rhythm-pitch patterns at phrase ends)
     */
    static std::unique_ptr<MusicalRule> create_cadential_pattern(
        int rhythm_engine, int pitch_engine,
        const std::vector<std::pair<MusicalValue, MusicalValue>>& cadence_pattern,
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Rest placement rule (rests only at specific metric positions)
     */
    static std::unique_ptr<MusicalRule> create_rest_placement(
        int rhythm_engine, int pitch_engine,
        const std::vector<double>& allowed_rest_positions,
        const BacktrackRoute& backtrack_route = {});

    // =============================================================================
    // Multi Engine Harmonic Rules
    // =============================================================================

    /**
     * @brief No parallel fifths/octaves rule (avoid parallel perfect intervals)
     */
    static std::unique_ptr<MusicalRule> create_no_parallel_perfect_intervals(
        const std::vector<int>& pitch_engines,
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Consonant harmonies rule (prefer consonant intervals)
     */
    static std::unique_ptr<MusicalRule> create_consonant_harmonies(
        const std::vector<int>& pitch_engines,
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Voice leading rule (smooth voice leading between chords)
     */
    static std::unique_ptr<MusicalRule> create_voice_leading(
        const std::vector<int>& pitch_engines, int max_step_size = 2,
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief No voice crossing rule (maintain voice order)
     */
    static std::unique_ptr<MusicalRule> create_no_voice_crossing(
        const std::vector<int>& pitch_engines,
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Chord membership rule (notes must form valid chords)
     */
    static std::unique_ptr<MusicalRule> create_chord_membership(
        const std::vector<int>& pitch_engines,
        const std::vector<std::vector<int>>& allowed_chord_types,
        const BacktrackRoute& backtrack_route = {});

    // =============================================================================
    // Metric/Time Signature Rules
    // =============================================================================

    /**
     * @brief Time signature consistency (durations fit time signature)
     */
    static std::unique_ptr<MusicalRule> create_time_signature_consistency(
        int rhythm_engine, int meter_engine,
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Metric modulation rule (controlled changes in time signature)
     */
    static std::unique_ptr<MusicalRule> create_metric_modulation(
        int meter_engine, const std::vector<std::pair<int,int>>& allowed_transitions,
        const BacktrackRoute& backtrack_route = {});

    // =============================================================================
    // Higher-Level Musical Rules
    // =============================================================================

    /**
     * @brief Phrase structure rule (control phrase lengths and boundaries)
     */
    static std::unique_ptr<MusicalRule> create_phrase_structure(
        int rhythm_engine, int pitch_engine,
        int min_phrase_length, int max_phrase_length,
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Classical counterpoint rule (species counterpoint constraints)
     */
    static std::unique_ptr<MusicalRule> create_species_counterpoint(
        int cantus_firmus_engine, int counterpoint_engine, int species = 1,
        const BacktrackRoute& backtrack_route = {});

    /**
     * @brief Jazz chord progression rule (enforce jazz harmonic progressions)
     */
    static std::unique_ptr<MusicalRule> create_jazz_chord_progression(
        const std::vector<int>& pitch_engines,
        const std::vector<std::vector<int>>& chord_progression,
        const BacktrackRoute& backtrack_route = {});

    // =============================================================================
    // Utilities for Rule Creation
    // =============================================================================

    /**
     * @brief Create backtrack route that targets the same engine
     */
    static BacktrackRoute create_self_backtrack(int engine_id) {
        return {engine_id};
    }

    /**
     * @brief Create backtrack route that targets other engine in pair
     */
    static BacktrackRoute create_other_engine_backtrack(int engine_id) {
        return {engine_id % 2 == 0 ? engine_id + 1 : engine_id - 1};
    }

    /**
     * @brief Create backtrack route that targets multiple engines
     */
    static BacktrackRoute create_multi_engine_backtrack(const std::vector<int>& engine_ids) {
        return engine_ids;
    }

private:
    // Helper functions for common musical calculations
    static bool is_consonant_interval(double interval);
    static bool is_perfect_interval(double interval);
    static double calculate_interval(MusicalValue pitch1, MusicalValue pitch2);
    static int get_scale_degree(MusicalValue pitch, const std::vector<int>& scale);
    static bool is_strong_beat_position(double position, const std::pair<int,int>& time_signature);
};

/**
 * @class CommonMusicalPatterns
 * @brief Collection of pre-defined musical patterns for rules
 */
class CommonMusicalPatterns {
public:
    // Rhythmic patterns
    static const std::vector<MusicalValue> QUARTER_NOTE_PATTERN;
    static const std::vector<MusicalValue> EIGHTH_NOTE_PATTERN;
    static const std::vector<MusicalValue> DOTTED_RHYTHM_PATTERN;
    static const std::vector<MusicalValue> SYNCOPATED_PATTERN;
    
    // Scale patterns
    static const std::vector<int> MAJOR_SCALE;
    static const std::vector<int> MINOR_SCALE;
    static const std::vector<int> PENTATONIC_SCALE;
    static const std::vector<int> CHROMATIC_SCALE;
    
    // Chord types
    static const std::vector<std::vector<int>> MAJOR_TRIADS;
    static const std::vector<std::vector<int>> MINOR_TRIADS;
    static const std::vector<std::vector<int>> SEVENTH_CHORDS;
    
    // Jazz chord progressions
    static const std::vector<std::vector<int>> II_V_I_PROGRESSION;
    static const std::vector<std::vector<int>> CIRCLE_OF_FIFTHS;
    
    // Contour patterns
    static const std::vector<int> ASCENDING_CONTOUR;
    static const std::vector<int> DESCENDING_CONTOUR;
    static const std::vector<int> ARCH_CONTOUR;
    static const std::vector<int> V_CONTOUR;
};

/**
 * @class PresetRuleSets
 * @brief Pre-configured rule sets for different musical styles
 */  
class PresetRuleSets {
public:
    /**
     * @brief Create rule set for strict classical counterpoint
     */
    static void add_classical_counterpoint_rules(
        RuleManager& manager, 
        const std::vector<int>& voices,
        int cantus_firmus_voice = 0);

    /**
     * @brief Create rule set for jazz improvisation
     */
    static void add_jazz_improvisation_rules(
        RuleManager& manager,
        const std::vector<int>& rhythm_engines,
        const std::vector<int>& pitch_engines);

    /**
     * @brief Create rule set for minimalist composition
     */
    static void add_minimalist_rules(
        RuleManager& manager,
        const std::vector<int>& engines);

    /**
     * @brief Create rule set for atonal composition  
     */
    static void add_atonal_rules(
        RuleManager& manager,
        const std::vector<int>& pitch_engines);

    /**
     * @brief Create basic rule set (minimal constraints)
     */
    static void add_basic_rules(
        RuleManager& manager,
        const std::vector<int>& rhythm_engines,
        const std::vector<int>& pitch_engines);
};

} // namespace MusicalRules
} // namespace ClusterEngine

#endif // CLUSTER_ENGINE_MUSICAL_RULES_HH