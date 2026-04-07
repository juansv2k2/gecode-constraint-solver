/**
 * @file cluster_engine_musical_rules.hh
 * @brief Musical Rule Interface System - Domain-Specific Constraint Rules
 * 
 * Implements the authentic Cluster Engine musical rule types:
 * - rhythm-pitch rules (rp1v family)
 * - rhythm-rhythm rules (rr2v family) 
 * - pitch-pitch rules (ppNv family)
 * - meter-duration rules (dm1v family)
 * - And specialized musical constraint patterns
 */

#ifndef CLUSTER_ENGINE_MUSICAL_RULES_HH
#define CLUSTER_ENGINE_MUSICAL_RULES_HH

#include "cluster_engine_core.hh"
#include "cluster_engine_backjump.hh"
#include <vector>
#include <functional>
#include <string>
#include <memory>

namespace ClusterEngine {

/**
 * @brief Musical rule types from Cluster Engine
 */
enum class MusicalRuleType {
    RHYTHM_PITCH_1V,      // Single voice rhythm-pitch constraints (rp1v)
    RHYTHM_RHYTHM_2V,     // Two voice rhythm-rhythm constraints (rr2v)
    PITCH_PITCH_NV,       // Multi-voice pitch-pitch constraints (ppNv) 
    METER_DURATION_1V,    // Meter-duration constraints (dm1v)
    DURATION_METER_1V,    // Duration-meter constraints (md1v)
    METER_NOTE_1V,        // Meter-note constraints (mn1v)
    NOTE_METER_1V,        // Note-meter constraints (nm1v)
    LIST_ALL_EVENTS_NV,   // Cross-voice event constraints (leNv)
    RHYTHM_HIERARCHY_2V,  // Rhythmic hierarchy constraints (rh2v)
    RHYTHM_METRIC_HIER_2V // Rhythm-metric hierarchy (rmh2v)
};

/**
 * @brief Rule application pattern from Cluster Engine v4.05
 */
enum class RulePattern {
    INDEX_RULE,     // Apply at specific variable indexes  
    WILDCARD_RULE,  // Sliding window pattern across sequence
    RL_RULE         // Right-to-left from starting point to end
};

/**
 * @brief Musical rule context - data provided to rule functions
 */
struct MusicalRuleContext {
    int current_engine;
    int current_index;
    const std::vector<MusicalCandidate>& solution_sequence;
    const std::vector<MusicalEngine*>& all_engines;
    const LinearSolution& linear_solution;
    
    MusicalRuleContext(int engine, int index, 
                      const std::vector<MusicalCandidate>& seq,
                      const std::vector<MusicalEngine*>& engines,
                      const LinearSolution& linear)
        : current_engine(engine), current_index(index), solution_sequence(seq),
          all_engines(engines), linear_solution(linear) {}
};

/**
 * @brief Base musical rule interface
 */
class MusicalRule {
protected:
    MusicalRuleType type_;
    RulePattern pattern_;
    std::vector<int> target_engines_;
    std::vector<int> application_indexes_;
    std::string description_;
    bool compiled_;
    
    // Helper methods for rule implementations
    std::vector<MusicalCandidate> get_rhythm_motif_sequence(const MusicalRuleContext& ctx, int length) const;
    std::vector<MusicalCandidate> get_pitch_motif_sequence(const MusicalRuleContext& ctx, int length) const;
    std::vector<int> get_absolute_values(const std::vector<MusicalCandidate>& candidates) const;
    std::vector<int> get_interval_values(const std::vector<MusicalCandidate>& candidates) const;
    
public:
    MusicalRule(MusicalRuleType type, const std::vector<int>& engines, 
               const std::string& desc = "Musical Rule")
        : type_(type), pattern_(RulePattern::WILDCARD_RULE), target_engines_(engines), 
          description_(desc), compiled_(false) {}
    
    virtual ~MusicalRule() = default;
    
    /**
     * @brief Test if rule passes for current musical context
     */
    virtual RuleTestResult test_rule(const MusicalRuleContext& context) = 0;
    
    /**
     * @brief Check if rule applies to given engine at given index
     */
    virtual bool applies_to(int engine, int index) const;
    
    // Getters
    MusicalRuleType get_type() const { return type_; }
    RulePattern get_pattern() const { return pattern_; }
    const std::vector<int>& get_engines() const { return target_engines_; }
    const std::string& get_description() const { return description_; }
};

/**
 * @brief Rhythm-Pitch Rule (rp1v family)
 * 
 * Constraints between rhythm and pitch in a single voice.
 * Applied to both rhythm and pitch engines of the same voice.
 */
class RhythmPitchRule : public MusicalRule {
private:
    std::function<bool(const std::vector<int>&, const std::vector<int>&)> rule_function_;
    
public:
    RhythmPitchRule(int voice_id, 
                   std::function<bool(const std::vector<int>&, const std::vector<int>&)> func,
                   const std::string& desc = "Rhythm-Pitch Rule");
    
    RuleTestResult test_rule(const MusicalRuleContext& context) override;
};

/**
 * @brief Rhythm-Rhythm Rule (rr2v family)
 * 
 * Constraints between rhythm sequences of two voices.
 * Complex formatting options for duration relationships.
 */
class RhythmRhythmRule : public MusicalRule {
public:
    enum class FormatType {
        DUR_OFFSET_A,      // '(dur-v1 offset) - rests included in v2
        DUR_OFFSET_DUR_B,  // '(dur-v1 offset dur-v2) - rests included  
        BREAK_RESTS_V1_C,  // Break at rests in v1, rests included in v2
        BREAK_RESTS_V2_E,  // Break at rests in v2
        SEGMENT_LIST_G,    // '((dur-v1 offset) (dur-v1 offset) ...)
        BREAK_BOTH_K       // Break at rests in v1 or v2
    };
    
private:
    std::function<bool(const std::vector<std::vector<int>>&)> rule_function_;
    FormatType format_type_;
    int voice1_id_, voice2_id_;
    
    std::vector<std::vector<int>> format_rhythm_data(const MusicalRuleContext& context) const;
    
public:
    RhythmRhythmRule(int voice1, int voice2, FormatType format,
                    std::function<bool(const std::vector<std::vector<int>>&)> func,
                    const std::string& desc = "Rhythm-Rhythm Rule");
    
    RuleTestResult test_rule(const MusicalRuleContext& context) override;
};

/**
 * @brief Pitch-Pitch Rule (ppNv family)
 * 
 * Harmonic constraints between multiple pitch sequences.
 * Supports various simultaneous note analysis methods.
 */
class PitchPitchRule : public MusicalRule {
public:
    enum class HarmonicType {
        ALL_SLICES_A,        // All possible harmonic slices
        BEAT_SLICES_B,       // Harmonic slices on all beats
        FIRST_VOICE_C,       // Use first voice timing
        TIME_POINTS_D        // At specific time points
    };
    
private:
    std::function<bool(const std::vector<std::vector<int>>&)> rule_function_;
    HarmonicType harmonic_type_;
    std::vector<int> voice_ids_;
    std::vector<double> specific_timepoints_;
    
    std::vector<std::vector<int>> extract_harmonic_slices(const MusicalRuleContext& context) const;
    
public:
    PitchPitchRule(const std::vector<int>& voices, HarmonicType type,
                  std::function<bool(const std::vector<std::vector<int>>&)> func,
                  const std::string& desc = "Pitch-Pitch Rule");
    
    // Constructor for time-point specific analysis
    PitchPitchRule(const std::vector<int>& voices, const std::vector<double>& timepoints,
                  std::function<bool(const std::vector<std::vector<int>>&)> func,
                  const std::string& desc = "Pitch-Pitch Timepoint Rule");
    
    RuleTestResult test_rule(const MusicalRuleContext& context) override;
};

/**
 * @brief Meter-Duration Rule (dm1v family)
 * 
 * Constraints between metric structure and duration sequences.
 * Ensures proper alignment with time signatures and beat patterns.
 */
class MeterDurationRule : public MusicalRule {
private:
    std::function<bool(const std::vector<std::pair<int,int>>&, const std::vector<int>&)> rule_function_;
    int voice_id_;
    
public:
    MeterDurationRule(int voice, 
                     std::function<bool(const std::vector<std::pair<int,int>>&, const std::vector<int>&)> func,
                     const std::string& desc = "Meter-Duration Rule");
    
    RuleTestResult test_rule(const MusicalRuleContext& context) override;
};

/**
 * @brief List All Events Rule (leNv family)
 * 
 * Cross-voice event analysis across any number of voices.
 * Provides comprehensive event lists for global constraints.
 */
class ListAllEventsRule : public MusicalRule {
public:
    enum class EventType {
        PITCHES_A,      // All pitch events
        DURATIONS_B,    // All duration events  
        ALL_EVENTS      // All events combined
    };
    
private:
    std::function<bool(const std::vector<std::vector<int>>&)> rule_function_;
    EventType event_type_;
    std::vector<int> voice_ids_;
    
    std::vector<std::vector<int>> collect_all_events(const MusicalRuleContext& context) const;
    
public:
    ListAllEventsRule(const std::vector<int>& voices, EventType type,
                     std::function<bool(const std::vector<std::vector<int>>&)> func,
                     const std::string& desc = "List All Events Rule");
    
    RuleTestResult test_rule(const MusicalRuleContext& context) override;
};

} // namespace ClusterEngine

#endif // CLUSTER_ENGINE_MUSICAL_RULES_HH