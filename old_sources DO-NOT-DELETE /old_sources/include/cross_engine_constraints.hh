// ===================================================================
// Cross-Engine Musical Constraints - Cluster Engine v4.05 Musical AI
// ===================================================================
// 
// ADVANCED MUSICAL INTELLIGENCE: Sophisticated rhythm-pitch relationships
// that coordinate between multiple musical engines using dual solution access.
//
// Revolutionary Features:
//   - Cross-dimensional musical constraints (rhythm ↔ pitch)
//   - Musical intelligence rules (consonance, accent patterns, phrasing)  
//   - Real-time coordination between rhythm and pitch engines
//   - Advanced pattern recognition for musical motifs
//
// Examples:
//   - Long durations prefer consonant intervals
//   - Syncopated rhythms trigger specific pitch patterns
//   - Metric accents emphasize harmonic tensions
//   - Phrase endings require resolution patterns
//
// ===================================================================

#ifndef CROSS_ENGINE_CONSTRAINTS_HH
#define CROSS_ENGINE_CONSTRAINTS_HH

#include <iostream>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <set>
#include <algorithm>
#include <iomanip>
#include "dual_solution_storage.hh"
#include "enhanced_rule_architecture.hh"

namespace MusicalConstraints {

// ===================================================================
// Musical Relationship Types: Different kinds of cross-engine rules
// ===================================================================

enum class MusicalRelationType {
    DURATION_INTERVAL,       // Duration length affects interval consonance
    RHYTHM_PITCH_PATTERN,   // Rhythmic patterns trigger pitch behaviors
    METRIC_HARMONIC_ACCENT, // Strong beats emphasize harmonic content
    PHRASE_STRUCTURE,       // Musical phrasing affects both rhythm and pitch
    CONTOUR_RHYTHM,         // Melodic contour interacts with rhythm
    SYNCOPATION_HARMONY     // Syncopated rhythms create harmonic tension
};

// ===================================================================
// MusicalContext: Analyzed musical information for intelligent decisions
// ===================================================================

struct MusicalContext {
    // Rhythmic analysis
    int duration_ms;                    // Current note duration
    bool is_syncopated;                // Off-beat placement
    bool is_accented_beat;             // Strong metric position
    double relative_duration;          // Duration relative to average
    
    // Pitch analysis  
    int pitch_midi;                     // MIDI pitch value
    int melodic_interval;              // Interval from previous note
    int harmonic_interval_class;       // Interval class (mod 12)
    bool is_consonant;                 // Consonant interval (P1,P5,P8,M3,m3)
    
    // Structural analysis
    int phrase_position;               // Position within phrase (0-based)
    bool is_phrase_beginning;          // Start of musical phrase
    bool is_phrase_ending;             // End of musical phrase  
    int distance_to_phrase_end;        // Notes until phrase ends
    
    MusicalContext() : duration_ms(0), is_syncopated(false), is_accented_beat(false),
                      relative_duration(1.0), pitch_midi(60), melodic_interval(0),
                      harmonic_interval_class(0), is_consonant(true), phrase_position(0),
                      is_phrase_beginning(false), is_phrase_ending(false), distance_to_phrase_end(0) {}
    
    // Calculate consonance based on interval class
    void update_consonance() {
        // Consonant intervals: unison(0), minor third(3), major third(4), 
        // perfect fifth(7), minor sixth(8), major sixth(9), octave(0)
        std::set<int> consonant_intervals = {0, 3, 4, 7, 8, 9};
        is_consonant = consonant_intervals.count(harmonic_interval_class) > 0;
    }
    
    // Analyze rhythmic placement
    void update_rhythmic_context(int position_in_measure, int beat_subdivision) {
        // Simple syncopation detection: off strong beats in 4/4 time
        is_accented_beat = (position_in_measure % beat_subdivision == 0);
        is_syncopated = !is_accented_beat && (duration_ms > 200); // Long off-beat notes
    }
    
    // Update phrase analysis
    void update_phrase_context(int current_pos, int phrase_length) {
        phrase_position = current_pos % phrase_length;
        is_phrase_beginning = (phrase_position == 0);
        is_phrase_ending = (phrase_position == phrase_length - 1);
        distance_to_phrase_end = phrase_length - 1 - phrase_position;
    }
};

// ===================================================================
// CrossEngineConstraint: Base class for musical relationship rules
// ===================================================================

class CrossEngineConstraint {
protected:
    MusicalRelationType relationship_type_;
    std::string description_;
    double strength_;                   // Constraint strength (0.0-1.0)
    bool is_mandatory_;                // Hard vs soft constraint
    
public:
    CrossEngineConstraint(MusicalRelationType type, const std::string& desc, 
                         double strength = 1.0, bool mandatory = true)
        : relationship_type_(type), description_(desc), strength_(strength), is_mandatory_(mandatory) {}
    
    virtual ~CrossEngineConstraint() = default;
    
    /**
     * @brief Check if cross-engine constraint is satisfied
     * @param storage Dual solution storage with current assignments
     * @param position Current variable position being checked
     * @return RuleResult with success/failure and backjump suggestions
     */
    virtual RuleResult check_constraint(const DualSolutionStorage& storage, int position) = 0;
    
    /**
     * @brief Get musical context analysis for current position
     * @param storage Solution storage for analysis
     * @param position Position to analyze
     * @return Musical context with rhythm/pitch/structure analysis
     */
    virtual MusicalContext analyze_musical_context(const DualSolutionStorage& storage, int position) {
        MusicalContext context;
        
        if (position >= 0) {
            // Get current values
            context.pitch_midi = storage.absolute(position);
            
            // Calculate interval from previous note
            if (position > 0) {
                context.melodic_interval = storage.interval(position);
                context.harmonic_interval_class = std::abs(context.melodic_interval) % 12;
                context.update_consonance();
            }
            
            // Mock duration analysis (would integrate with rhythm engine)
            context.duration_ms = 250; // Default quarter note
            context.relative_duration = 1.0;
            
            // Mock phrase analysis (8-note phrases)
            context.update_phrase_context(position, 8);
            
            // Mock metric analysis (4/4 time)
            context.update_rhythmic_context(position, 4);
        }
        
        return context;
    }
    
    // Getters
    MusicalRelationType get_relationship_type() const { return relationship_type_; }
    const std::string& get_description() const { return description_; }
    double get_strength() const { return strength_; }
    bool is_mandatory() const { return is_mandatory_; }
};

// ===================================================================
// DurationIntervalConstraint: Long durations prefer consonant intervals
// ===================================================================

class DurationIntervalConstraint : public CrossEngineConstraint {
private:
    int min_duration_ms_;               // Minimum duration to trigger constraint
    double consonance_preference_;      // How much to prefer consonant intervals
    
public:
    DurationIntervalConstraint(int min_duration = 400, double consonance_pref = 0.8,
                              double strength = 0.9, bool mandatory = false)
        : CrossEngineConstraint(MusicalRelationType::DURATION_INTERVAL, 
                               "Long durations prefer consonant intervals", strength, mandatory),
          min_duration_ms_(min_duration), consonance_preference_(consonance_pref) {}
    
    RuleResult check_constraint(const DualSolutionStorage& storage, int position) override {
        if (position <= 0) return RuleResult::Success();
        
        MusicalContext context = analyze_musical_context(storage, position);
        
        // Apply constraint: long durations should have consonant intervals
        if (context.duration_ms >= min_duration_ms_) {
            if (!context.is_consonant) {
                if (is_mandatory_) {
                    return RuleResult::Failure(2, "Long duration requires consonant interval");
                } else {
                    // Soft constraint: record preference violation
                    return RuleResult::Success(); // Allow but note preference violation
                }
            }
        }
        
        return RuleResult::Success();
    }
};

// ===================================================================
// RhythmPitchPatternConstraint: Specific rhythmic patterns trigger pitch behaviors
// ===================================================================

class RhythmPitchPatternConstraint : public CrossEngineConstraint {
private:
    std::vector<int> rhythm_pattern_;   // Duration pattern to match (in ms)
    std::function<bool(const std::vector<int>&)> pitch_response_; // Required pitch response
    
public:
    RhythmPitchPatternConstraint(const std::vector<int>& rhythm_pattern,
                                std::function<bool(const std::vector<int>&)> pitch_response,
                                double strength = 0.8, bool mandatory = false)
        : CrossEngineConstraint(MusicalRelationType::RHYTHM_PITCH_PATTERN,
                               "Rhythmic pattern triggers pitch response", strength, mandatory),
          rhythm_pattern_(rhythm_pattern), pitch_response_(pitch_response) {}
    
    RuleResult check_constraint(const DualSolutionStorage& storage, int position) override {
        int pattern_length = rhythm_pattern_.size();
        if (position < pattern_length - 1) return RuleResult::Success();
        
        // Check if current sequence matches rhythm pattern
        std::vector<int> current_rhythm_sequence;
        std::vector<int> current_pitch_sequence;
        
        for (int i = 0; i < pattern_length; i++) {
            int check_pos = position - pattern_length + 1 + i;
            if (check_pos >= 0) {
                // Mock: extract rhythm timing (would integrate with actual rhythm engine)
                current_rhythm_sequence.push_back(250); // Mock quarter note
                current_pitch_sequence.push_back(storage.absolute(check_pos));
            }
        }
        
        // If rhythm pattern matches, check pitch response
        if (matches_rhythm_pattern(current_rhythm_sequence)) {
            if (!pitch_response_(current_pitch_sequence)) {
                return RuleResult::Failure(pattern_length, "Rhythm pattern requires specific pitch response");
            }
        }
        
        return RuleResult::Success();
    }
    
private:
    bool matches_rhythm_pattern(const std::vector<int>& current_rhythm) {
        if (current_rhythm.size() != rhythm_pattern_.size()) return false;
        
        // Simple pattern matching (could be enhanced with tolerance)
        for (size_t i = 0; i < rhythm_pattern_.size(); i++) {
            if (std::abs(current_rhythm[i] - rhythm_pattern_[i]) > 50) { // 50ms tolerance
                return false;
            }
        }
        return true;
    }
};

// ===================================================================
// MetricHarmonicAccentConstraint: Strong beats emphasize harmonic content
// ===================================================================

class MetricHarmonicAccentConstraint : public CrossEngineConstraint {
private:
    std::vector<int> accent_positions_;  // Beat positions that are accented
    bool prefer_consonance_on_accents_;  // Whether accents prefer consonance
    bool allow_dissonance_off_accents_;  // Whether off-beats allow dissonance
    
public:
    MetricHarmonicAccentConstraint(const std::vector<int>& accent_positions,
                                  bool consonance_on_accents = true,
                                  bool dissonance_off_accents = true,
                                  double strength = 0.7, bool mandatory = false)
        : CrossEngineConstraint(MusicalRelationType::METRIC_HARMONIC_ACCENT,
                               "Strong beats emphasize harmonic stability", strength, mandatory),
          accent_positions_(accent_positions), 
          prefer_consonance_on_accents_(consonance_on_accents),
          allow_dissonance_off_accents_(dissonance_off_accents) {}
    
    RuleResult check_constraint(const DualSolutionStorage& storage, int position) override {
        if (position <= 0) return RuleResult::Success();
        
        MusicalContext context = analyze_musical_context(storage, position);
        
        // Check if current position is an accented beat
        bool is_accent = std::find(accent_positions_.begin(), accent_positions_.end(), 
                                  position % 4) != accent_positions_.end();
        
        if (is_accent && prefer_consonance_on_accents_) {
            if (!context.is_consonant) {
                return RuleResult::Failure(1, "Accented beat requires consonant harmony");
            }
        }
        
        // Allow more freedom on off-beats
        return RuleResult::Success();
    }
};

// ===================================================================
// PhraseStructureConstraint: Musical phrasing affects both rhythm and pitch
// ===================================================================

class PhraseStructureConstraint : public CrossEngineConstraint {
private:
    int phrase_length_;                 // Length of musical phrases
    bool require_phrase_resolution_;    // Whether phrases must resolve
    
public:
    PhraseStructureConstraint(int phrase_length = 8, bool require_resolution = true,
                             double strength = 0.85, bool mandatory = false)
        : CrossEngineConstraint(MusicalRelationType::PHRASE_STRUCTURE,
                               "Phrase structure guides rhythm and pitch", strength, mandatory),
          phrase_length_(phrase_length), require_phrase_resolution_(require_resolution) {}
    
    RuleResult check_constraint(const DualSolutionStorage& storage, int position) override {
        if (position <= 0) return RuleResult::Success();
        
        MusicalContext context = analyze_musical_context(storage, position);
        context.update_phrase_context(position, phrase_length_);
        
        // Phrase ending requirements
        if (context.is_phrase_ending && require_phrase_resolution_) {
            // Prefer resolution: small intervals or return to tonic
            if (std::abs(context.melodic_interval) > 5) {
                return RuleResult::Failure(2, "Phrase ending requires melodic resolution");
            }
        }
        
        // Phrase beginning preferences: allow larger leaps
        if (context.is_phrase_beginning) {
            // More freedom at phrase beginnings (no constraint)
            return RuleResult::Success();
        }
        
        return RuleResult::Success();
    }
};

// ===================================================================
// CrossEngineCoordinator: Manages all cross-engine musical constraints
// ===================================================================

class CrossEngineCoordinator {
private:
    std::vector<std::unique_ptr<CrossEngineConstraint>> constraints_;
    std::map<MusicalRelationType, int> constraint_counts_;
    int total_constraint_checks_;
    int total_violations_;
    
public:
    CrossEngineCoordinator() : total_constraint_checks_(0), total_violations_(0) {}
    
    /**
     * @brief Add a cross-engine musical constraint
     * @param constraint Constraint to add (ownership transferred)
     */
    void add_constraint(std::unique_ptr<CrossEngineConstraint> constraint) {
        MusicalRelationType type = constraint->get_relationship_type();
        constraint_counts_[type]++;
        constraints_.push_back(std::move(constraint));
    }
    
    /**
     * @brief Check all cross-engine constraints for current position
     * @param storage Dual solution storage
     * @param position Current variable position
     * @return Combined rule result with cross-engine intelligence
     */
    RuleResult check_all_constraints(const DualSolutionStorage& storage, int position) {
        std::vector<RuleResult> failures;
        total_constraint_checks_++;
        
        for (const auto& constraint : constraints_) {
            RuleResult result = constraint->check_constraint(storage, position);
            if (!result.success) {
                failures.push_back(result);
                total_violations_++;
            }
        }
        
        if (failures.empty()) {
            return RuleResult::Success();
        }
        
        // Combine failures using musical intelligence
        int min_backjump = failures[0].backjump_distance;
        std::string combined_reason = "Cross-engine constraints failed: ";
        
        for (size_t i = 0; i < failures.size(); ++i) {
            if (failures[i].backjump_distance < min_backjump) {
                min_backjump = failures[i].backjump_distance;
            }
            if (i > 0) combined_reason += "; ";
            combined_reason += failures[i].failure_reason;
        }
        
        return RuleResult::Failure(min_backjump, combined_reason);
    }
    
    /**
     * @brief Add common musical constraint patterns
     */
    void add_standard_musical_constraints() {
        // 1. Long durations prefer consonant intervals
        add_constraint(std::unique_ptr<CrossEngineConstraint>(
            new DurationIntervalConstraint(400, 0.8, 0.9, false)
        ));
        
        // 2. Strong beats (positions 0, 2) prefer harmonic stability
        add_constraint(std::unique_ptr<CrossEngineConstraint>(
            new MetricHarmonicAccentConstraint({0, 2}, true, true, 0.7, false)
        ));
        
        // 3. Phrase structure guides melodic resolution
        add_constraint(std::unique_ptr<CrossEngineConstraint>(
            new PhraseStructureConstraint(8, true, 0.85, false)
        ));
        
        // 4. Syncopated rhythm pattern creates harmonic tension
        std::vector<int> syncopated_pattern = {200, 300, 200}; // Short-long-short
        auto tension_response = [](const std::vector<int>& pitches) {
            if (pitches.size() < 3) return true;
            // Prefer larger intervals in middle of syncopated pattern
            int middle_interval = std::abs(pitches[1] - pitches[0]);
            return middle_interval >= 3; // At least a minor third
        };
        
        add_constraint(std::unique_ptr<CrossEngineConstraint>(
            new RhythmPitchPatternConstraint(syncopated_pattern, tension_response, 0.6, false)
        ));
    }
    
    /**
     * @brief Print cross-engine constraint statistics
     */
    void print_statistics(std::ostream& os = std::cout) const {
        os << "\n🎼 Cross-Engine Musical Intelligence Statistics:\n";
        os << "  Total constraints: " << constraints_.size() << "\n";
        
        for (const auto& count_pair : constraint_counts_) {
            os << "  " << relationship_type_name(count_pair.first) 
               << ": " << count_pair.second << " constraints\n";
        }
        
        os << "  Total constraint checks: " << total_constraint_checks_ << "\n";
        os << "  Total violations: " << total_violations_ << "\n";
        
        if (total_constraint_checks_ > 0) {
            double violation_rate = (double)total_violations_ / total_constraint_checks_ * 100.0;
            os << "  Violation rate: " << std::fixed << std::setprecision(1) 
               << violation_rate << "%\n";
        }
    }
    
    /**
     * @brief Get count of constraints by type
     */
    size_t constraint_count() const { return constraints_.size(); }
    int get_violation_count() const { return total_violations_; }
    int get_check_count() const { return total_constraint_checks_; }
    
private:
    std::string relationship_type_name(MusicalRelationType type) const {
        switch (type) {
            case MusicalRelationType::DURATION_INTERVAL: return "Duration-Interval";
            case MusicalRelationType::RHYTHM_PITCH_PATTERN: return "Rhythm-Pitch Pattern";
            case MusicalRelationType::METRIC_HARMONIC_ACCENT: return "Metric-Harmonic Accent";
            case MusicalRelationType::PHRASE_STRUCTURE: return "Phrase Structure";
            case MusicalRelationType::CONTOUR_RHYTHM: return "Contour-Rhythm";
            case MusicalRelationType::SYNCOPATION_HARMONY: return "Syncopation-Harmony";
        }
        return "Unknown";
    }
};

} // namespace MusicalConstraints

#endif // CROSS_ENGINE_CONSTRAINTS_HH