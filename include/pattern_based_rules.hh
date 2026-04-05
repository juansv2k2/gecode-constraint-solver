// ===================================================================
// Pattern-Based Rules - Dynamic Musical Intelligence (JBS-Style)
// ===================================================================
// 
// DYNAMIC PATTERN ARCHITECTURE: Following JBS-Constraints approach where
// musical pattern rules are passed as external arguments rather than hardcoded.
// This enables flexible configuration based on user requirements.
//
// Dynamic Features:  
//   - External rule loading (no hardcoded patterns)
//   - Argument-driven musical intelligence configuration
//   - Musical motif recognition and repetition rules
//   - Melodic sequence pattern detection  
//   - Rhythmic pattern enforcement and variation
//   - Harmonic progression pattern matching
//
// Pattern Types (configurable via arguments):
//   - Exact Pattern Matching: Note-for-note repetition
//   - Transposed Pattern Matching: Same intervals, different starting pitch
//   - Rhythmic Pattern Matching: Same durations, different pitches
//   - Contour Pattern Matching: Same melodic shape, different intervals
//   - Harmonic Pattern Matching: Same harmonic function, different voicing
//
// Usage: Load rules from external sources (see example_musical_patterns.hh)
//
// ===================================================================

#ifndef PATTERN_BASED_RULES_HH
#define PATTERN_BASED_RULES_HH

#include <iostream>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iomanip>
#include "dual_solution_storage.hh"
#include "enhanced_rule_architecture.hh"

namespace MusicalConstraints {

// ===================================================================
// PatternType: Different kinds of musical pattern matching
// ===================================================================

enum class PatternType {
    EXACT_REPETITION,       // Exact note-for-note repetition
    TRANSPOSED_REPETITION,  // Same intervals, different starting pitch
    RHYTHMIC_PATTERN,       // Same rhythm, different pitches
    CONTOUR_PATTERN,        // Same melodic direction, different sizes
    SEQUENCE_PATTERN,       // Melodic sequence (pattern at different pitch levels)
    HARMONIC_PROGRESSION    // Harmonic function pattern
};

// ===================================================================
// PatternTemplate: Defines a musical pattern to match or enforce
// ===================================================================

struct PatternTemplate {
    std::vector<int> pitch_pattern;     // MIDI pitches (absolute) 
    std::vector<int> interval_pattern;  // Melodic intervals
    std::vector<int> rhythm_pattern;    // Durations in milliseconds
    std::vector<int> contour_pattern;   // Melodic directions: -1(down), 0(same), +1(up)
    
    PatternType pattern_type;
    std::string description;
    double tolerance;                   // Matching tolerance (0.0-1.0)
    bool allow_transposition;          // Whether transpositions are allowed
    bool allow_rhythmic_variation;     // Whether rhythm can vary
    
    PatternTemplate(PatternType type = PatternType::EXACT_REPETITION,
                   const std::string& desc = "Musical pattern",
                   double tol = 0.1, bool transpose = false, bool rhythm_var = false)
        : pattern_type(type), description(desc), tolerance(tol),
          allow_transposition(transpose), allow_rhythmic_variation(rhythm_var) {}
    
    /**
     * @brief Create pattern template from musical sequence
     * @param pitches MIDI pitch sequence
     * @param rhythms Duration sequence (optional)
     */
    void create_from_sequence(const std::vector<int>& pitches, 
                             const std::vector<int>& rhythms = {}) {
        pitch_pattern = pitches;
        rhythm_pattern = rhythms;
        
        // Calculate interval pattern
        interval_pattern.clear();
        for (size_t i = 1; i < pitches.size(); i++) {
            interval_pattern.push_back(pitches[i] - pitches[i-1]);
        }
        
        // Calculate contour pattern
        contour_pattern.clear();
        for (int interval : interval_pattern) {
            if (interval > 0) contour_pattern.push_back(1);      // Up
            else if (interval < 0) contour_pattern.push_back(-1); // Down
            else contour_pattern.push_back(0);                   // Same
        }
    }
    
    /**
     * @brief Get pattern length (number of notes)
     */
    size_t length() const {
        return pitch_pattern.size();
    }
    
    /**
     * @brief Check if pattern is valid
     */
    bool is_valid() const {
        return !pitch_pattern.empty() && 
               (interval_pattern.size() == pitch_pattern.size() - 1);
    }
};

// ===================================================================
// PatternMatcher: Sophisticated pattern recognition engine
// ===================================================================

class PatternMatcher {
public:
    /**
     * @brief Check if current sequence matches the pattern template
     * @param current_pitches Current pitch sequence to match
     * @param current_rhythms Current rhythm sequence (optional)
     * @param pattern Pattern template to match against
     * @return Matching score (0.0 = no match, 1.0 = perfect match)
     */
    static double calculate_pattern_match(const std::vector<int>& current_pitches,
                                         const std::vector<int>& current_rhythms,
                                         const PatternTemplate& pattern) {
        if (current_pitches.size() != pattern.length()) {
            return 0.0; // Size mismatch
        }
        
        switch (pattern.pattern_type) {
            case PatternType::EXACT_REPETITION:
                return match_exact_repetition(current_pitches, pattern);
                
            case PatternType::TRANSPOSED_REPETITION:
                return match_transposed_repetition(current_pitches, pattern);
                
            case PatternType::RHYTHMIC_PATTERN:
                return match_rhythmic_pattern(current_rhythms, pattern);
                
            case PatternType::CONTOUR_PATTERN:
                return match_contour_pattern(current_pitches, pattern);
                
            case PatternType::SEQUENCE_PATTERN:
                return match_sequence_pattern(current_pitches, pattern);
                
            case PatternType::HARMONIC_PROGRESSION:
                return match_harmonic_progression(current_pitches, pattern);
        }
        
        return 0.0;
    }
    
private:
    // Match exact note-for-note repetition
    static double match_exact_repetition(const std::vector<int>& current, 
                                        const PatternTemplate& pattern) {
        if (current.size() != pattern.pitch_pattern.size()) return 0.0;
        
        int matches = 0;
        for (size_t i = 0; i < current.size(); i++) {
            if (current[i] == pattern.pitch_pattern[i]) {
                matches++;
            }
        }
        
        return (double)matches / current.size();
    }
    
    // Match transposed repetition (same intervals, different starting pitch)
    static double match_transposed_repetition(const std::vector<int>& current,
                                             const PatternTemplate& pattern) {
        if (current.size() != pattern.pitch_pattern.size()) return 0.0;
        if (current.size() < 2) return 1.0; // Single note always matches
        
        // Calculate current intervals
        std::vector<int> current_intervals;
        for (size_t i = 1; i < current.size(); i++) {
            current_intervals.push_back(current[i] - current[i-1]);
        }
        
        // Compare with pattern intervals
        int matches = 0;
        for (size_t i = 0; i < current_intervals.size(); i++) {
            if (std::abs(current_intervals[i] - pattern.interval_pattern[i]) <= 1) {
                matches++; // Allow 1 semitone tolerance
            }
        }
        
        return (double)matches / current_intervals.size();
    }
    
    // Match rhythmic pattern
    static double match_rhythmic_pattern(const std::vector<int>& current_rhythms,
                                        const PatternTemplate& pattern) {
        if (current_rhythms.size() != pattern.rhythm_pattern.size()) return 0.0;
        if (pattern.rhythm_pattern.empty()) return 1.0; // No rhythm constraint
        
        int matches = 0;
        for (size_t i = 0; i < current_rhythms.size(); i++) {
            int tolerance_ms = (int)(pattern.rhythm_pattern[i] * pattern.tolerance);
            if (std::abs(current_rhythms[i] - pattern.rhythm_pattern[i]) <= tolerance_ms) {
                matches++;
            }
        }
        
        return (double)matches / current_rhythms.size();
    }
    
    // Match contour pattern (melodic direction)
    static double match_contour_pattern(const std::vector<int>& current,
                                       const PatternTemplate& pattern) {
        if (current.size() != pattern.pitch_pattern.size()) return 0.0;
        if (current.size() < 2) return 1.0;
        
        // Calculate current contour
        std::vector<int> current_contour;
        for (size_t i = 1; i < current.size(); i++) {
            int interval = current[i] - current[i-1];
            if (interval > 0) current_contour.push_back(1);      // Up
            else if (interval < 0) current_contour.push_back(-1); // Down  
            else current_contour.push_back(0);                   // Same
        }
        
        // Compare contours
        int matches = 0;
        for (size_t i = 0; i < current_contour.size(); i++) {
            if (current_contour[i] == pattern.contour_pattern[i]) {
                matches++;
            }
        }
        
        return (double)matches / current_contour.size();
    }
    
    // Match sequence pattern (same intervals at different pitch levels)
    static double match_sequence_pattern(const std::vector<int>& current,
                                        const PatternTemplate& pattern) {
        // Sequence patterns are transposed repetitions with specific interval relationships
        return match_transposed_repetition(current, pattern);
    }
    
    // Match harmonic progression pattern
    static double match_harmonic_progression(const std::vector<int>& current,
                                           const PatternTemplate& pattern) {
        if (current.size() != pattern.pitch_pattern.size()) return 0.0;
        
        // Simplified harmonic matching: check interval classes (mod 12)
        int matches = 0;
        for (size_t i = 0; i < current.size(); i++) {
            int current_class = current[i] % 12;
            int pattern_class = pattern.pitch_pattern[i] % 12;
            if (current_class == pattern_class) {
                matches++;
            }
        }
        
        return (double)matches / current.size();
    }
};

// ===================================================================
// PatternBasedRule: Advanced wildcard rule using pattern templates
// ===================================================================

class PatternBasedRule : public MusicalRule {
private:
    PatternTemplate pattern_template_;
    double required_match_threshold_;      // Minimum match score to pass (0.0-1.0)
    bool enforce_pattern_;                // Whether to enforce or just prefer pattern
    std::string rule_description_;
    
public:
    /**
     * @brief Create pattern-based musical rule
     * @param pattern Pattern template to match/enforce
     * @param threshold Minimum match score required (0.0-1.0)
     * @param enforce Whether to enforce (hard constraint) or prefer (soft constraint)
     * @param description Human-readable description
     */
    PatternBasedRule(const PatternTemplate& pattern, double threshold = 0.8,
                    bool enforce = false, const std::string& description = "Pattern rule")
        : pattern_template_(pattern), required_match_threshold_(threshold),
          enforce_pattern_(enforce), rule_description_(description) {}
    
    RuleResult check_rule(const DualSolutionStorage& storage, int current_index) const override {
        int pattern_length = (int)pattern_template_.length();
        if (current_index < pattern_length - 1) {
            return RuleResult::Success(); // Not enough notes yet
        }
        
        // Extract current sequence for pattern matching
        std::vector<int> current_pitches, current_rhythms;
        for (int i = 0; i < pattern_length; i++) {
            int check_pos = current_index - pattern_length + 1 + i;
            if (check_pos >= 0) {
                current_pitches.push_back(storage.absolute(check_pos));
                current_rhythms.push_back(250); // Mock rhythm (would integrate with rhythm engine)
            }
        }
        
        // Calculate pattern match score
        double match_score = PatternMatcher::calculate_pattern_match(
            current_pitches, current_rhythms, pattern_template_);
        
        // Check if pattern requirement is met
        if (match_score < required_match_threshold_) {
            if (enforce_pattern_) {
                return RuleResult::Failure(pattern_length, 
                    "Pattern requirement not met: " + rule_description_ + 
                    " (score: " + std::to_string(match_score) + ")");
            } else {
                // Soft constraint: allow but note preference
                return RuleResult::Success();
            }
        }
        
        return RuleResult::Success();
    }
    
    std::string description() const override { 
        return rule_description_ + " [Pattern: " + pattern_template_.description + "]"; 
    }
    
    std::vector<int> get_dependent_variables(int current_index) const override {
        std::vector<int> dependencies;
        int pattern_length = (int)pattern_template_.length();
        
        for (int i = 1; i < pattern_length; i++) {
            int dep_index = current_index - i;
            if (dep_index >= 0) {
                dependencies.push_back(dep_index);
            }
        }
        return dependencies;
    }
    
    std::string rule_type() const override { return "PatternBasedRule"; }
    
    // Getters
    const PatternTemplate& get_pattern_template() const { return pattern_template_; }
    double get_match_threshold() const { return required_match_threshold_; }
    bool is_enforcing() const { return enforce_pattern_; }
};

// ===================================================================
// SequenceRule: Specialized rule for melodic sequences
// ===================================================================

class SequenceRule : public PatternBasedRule {
private:
    int sequence_interval_;             // Interval between sequence repetitions
    int min_repetitions_;              // Minimum number of sequence repetitions
    
public:
    /**
     * @brief Create melodic sequence rule
     * @param motif Basic motif to sequence
     * @param sequence_interval Interval between sequence repetitions (semitones)
     * @param min_repetitions Minimum number of repetitions required
     * @param threshold Pattern matching threshold
     */
    SequenceRule(const std::vector<int>& motif, int sequence_interval = 2,
                int min_repetitions = 3, double threshold = 0.75)
        : PatternBasedRule(create_sequence_template(motif, sequence_interval, min_repetitions),
                          threshold, false, "Melodic sequence pattern"),
          sequence_interval_(sequence_interval), min_repetitions_(min_repetitions) {}
    
private:
    static PatternTemplate create_sequence_template(const std::vector<int>& motif,
                                                   int interval, int repetitions) {
        PatternTemplate seq_template(PatternType::SEQUENCE_PATTERN,
                               "Melodic sequence with " + std::to_string(repetitions) + " repetitions",
                               0.2, true, false);
        
        // Create sequence by repeating motif at different pitch levels
        std::vector<int> sequence_pitches;
        for (int rep = 0; rep < repetitions; rep++) {
            for (size_t i = 0; i < motif.size(); i++) {
                sequence_pitches.push_back(motif[i] + rep * interval);
            }
        }
        
        seq_template.create_from_sequence(sequence_pitches);
        return seq_template;
    }
};

// ===================================================================
// MotifRule: Rule for musical motif recognition and repetition
// ===================================================================

class MotifRule : public PatternBasedRule {
private:
    int motif_separation_;              // Distance between motif occurrences
    bool allow_variation_;              // Whether slight variations are allowed
    
public:
    /**
     * @brief Create musical motif rule  
     * @param motif_pitches MIDI pitches of the motif
     * @param separation Distance between motif occurrences
     * @param allow_var Whether to allow slight variations
     * @param threshold Pattern matching threshold
     */
    MotifRule(const std::vector<int>& motif_pitches, int separation = 4,
             bool allow_var = true, double threshold = 0.85)
        : PatternBasedRule(create_motif_template(motif_pitches, allow_var),
                          threshold, false, "Musical motif repetition"),
          motif_separation_(separation), allow_variation_(allow_var) {}
    
private:
    static PatternTemplate create_motif_template(const std::vector<int>& motif,
                                                bool allow_variation) {
        PatternType type = allow_variation ? PatternType::TRANSPOSED_REPETITION 
                                          : PatternType::EXACT_REPETITION;
        PatternTemplate motif_template(type, "Musical motif", 0.15, true, allow_variation);
        motif_template.create_from_sequence(motif);
        return motif_template;
    }
};

// ===================================================================
// PatternBasedRuleEngine: Manages pattern-based musical rules
// ===================================================================

class PatternBasedRuleEngine {
private:
    std::vector<std::unique_ptr<PatternBasedRule>> pattern_rules_;
    std::map<PatternType, int> pattern_type_counts_;
    int total_pattern_checks_;
    int total_pattern_matches_;
    int total_pattern_violations_;
    
public:
    PatternBasedRuleEngine() : total_pattern_checks_(0), total_pattern_matches_(0), 
                              total_pattern_violations_(0) {}
    
    /**
     * @brief Add pattern-based rule
     * @param rule Pattern rule to add (ownership transferred)
     */
    void add_pattern_rule(std::unique_ptr<PatternBasedRule> rule) {
        PatternType type = rule->get_pattern_template().pattern_type;
        pattern_type_counts_[type]++;
        pattern_rules_.push_back(std::move(rule));
    }
    
    /**
     * @brief Add multiple pattern rules from external source
     * 
     * Accepts a collection of rules passed as arguments to the system.
     * This enables dynamic rule configuration. Example rules can be found
     * in example_musical_patterns.hh for reference.
     */
    void add_pattern_rules(std::vector<std::unique_ptr<PatternBasedRule>> rules) {
        for (auto& rule : rules) {
            add_pattern_rule(std::move(rule));
        }
    }
    
    /**
     * @brief Check all pattern-based rules
     * @param storage Dual solution storage
     * @param current_index Current variable position  
     * @return Combined rule result with pattern intelligence
     */
    RuleResult check_all_patterns(const DualSolutionStorage& storage, int current_index) {
        std::vector<RuleResult> failures;
        total_pattern_checks_++;
        
        for (const auto& rule : pattern_rules_) {
            RuleResult result = rule->check_rule(storage, current_index);
            if (!result.success) {
                failures.push_back(result);
                total_pattern_violations_++;
            } else {
                total_pattern_matches_++;
            }
        }
        
        if (failures.empty()) {
            return RuleResult::Success();
        }
        
        // Combine pattern failures
        int min_backjump = failures[0].backjump_distance;
        std::string combined_reason = "Pattern-based constraints failed: ";
        
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
     * @brief Print pattern-based rule statistics
     */
    void print_statistics(std::ostream& os = std::cout) const {
        os << "\n🎵 Pattern-Based Rule Engine Statistics:\n";
        os << "  Total pattern rules: " << pattern_rules_.size() << "\n";
        
        for (const auto& count_pair : pattern_type_counts_) {
            os << "  " << pattern_type_name(count_pair.first)
               << ": " << count_pair.second << " rules\n";
        }
        
        os << "  Total pattern checks: " << total_pattern_checks_ << "\n";
        os << "  Pattern matches: " << total_pattern_matches_ << "\n";
        os << "  Pattern violations: " << total_pattern_violations_ << "\n";
        
        if (total_pattern_checks_ > 0) {
            double match_rate = (double)total_pattern_matches_ / total_pattern_checks_ * 100.0;
            os << "  Pattern match rate: " << std::fixed << std::setprecision(1)
               << match_rate << "%\n";
        }
    }
    
    // Getters
    size_t rule_count() const { return pattern_rules_.size(); }
    int get_match_count() const { return total_pattern_matches_; }
    int get_violation_count() const { return total_pattern_violations_; }
    
private:
    std::string pattern_type_name(PatternType type) const {
        switch (type) {
            case PatternType::EXACT_REPETITION: return "Exact Repetition";
            case PatternType::TRANSPOSED_REPETITION: return "Transposed Repetition";
            case PatternType::RHYTHMIC_PATTERN: return "Rhythmic Pattern";
            case PatternType::CONTOUR_PATTERN: return "Contour Pattern";
            case PatternType::SEQUENCE_PATTERN: return "Sequence Pattern"; 
            case PatternType::HARMONIC_PROGRESSION: return "Harmonic Progression";
        }
        return "Unknown Pattern";
    }
};

} // namespace MusicalConstraints

#endif // PATTERN_BASED_RULES_HH