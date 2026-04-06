// ===================================================================
// Phase 4: Compositional Memory Management - Musical Decision Intelligence
// ===================================================================
//
// INTELLIGENT MUSICAL MEMORY: Advanced memory system that learns from
// musical decisions and guides future compositional choices with musical
// intelligence and contextual awareness.
//
// Revolutionary Capabilities:
//   ✅ Musical decision pattern recognition and learning
//   ✅ Compositional style adaptation based on previous choices  
//   ✅ Musical preference evolution during composition
//   ✅ Context-aware musical suggestion generation
//   ✅ Musical motif and phrase memory with reuse intelligence
//
// Memory Types:
//   - Immediate Memory: Current musical phrase/segment decisions
//   - Short-term Memory: Recent musical pattern preferences  
//   - Long-term Memory: Compositional style and musical tendencies
//   - Structural Memory: Large-scale musical form and development
//   - Pattern Memory: Musical motifs, sequences, and repeated elements
//
// ===================================================================

#ifndef COMPOSITIONAL_MEMORY_HH
#define COMPOSITIONAL_MEMORY_HH

#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <deque>
#include <memory>
#include <algorithm>
#include <numeric>
#include "musical_state_persistence.hh"
#include "dual_solution_storage.hh"

namespace MusicalConstraints {

/**
 * @brief Musical decision with contextual information
 */
struct MusicalDecision {
    int decision_index;                    // Position in musical sequence
    int chosen_value;                      // The musical choice made (pitch, interval, etc.)
    std::vector<int> alternative_values;   // Other options considered
    std::string decision_reason;           // Why this choice was made
    CompositionContext context_at_decision;    // Musical context when decision was made
    double confidence_level;               // How confident the decision was (0.0-1.0)
    std::chrono::system_clock::time_point decision_time; // When decision was made
    
    MusicalDecision(int index, int value, const std::string& reason,
                   const CompositionContext& context, double confidence = 1.0)
        : decision_index(index), chosen_value(value), decision_reason(reason),
          context_at_decision(context), confidence_level(confidence),
          decision_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief Musical pattern memory for motif and sequence reuse
 */
class MusicalPatternMemory {
private:
    struct StoredPattern {
        std::vector<int> pattern_notes;
        std::vector<int> pattern_intervals;
        std::string pattern_name;
        int usage_count;
        double pattern_quality_score;
        CompositionContext original_context;
        
        StoredPattern(const std::vector<int>& notes, const std::string& name,
                     const CompositionContext& context, double quality = 1.0)
            : pattern_notes(notes), pattern_name(name), usage_count(1),
              pattern_quality_score(quality), original_context(context) {
            
            // Calculate intervals for pattern matching
            pattern_intervals.clear();
            for (size_t i = 1; i < notes.size(); ++i) {
                pattern_intervals.push_back(notes[i] - notes[i-1]);
            }
        }
    };
    
    std::vector<StoredPattern> stored_patterns_;
    std::map<std::string, int> pattern_categories_;  // e.g., "motif", "sequence", "phrase_ending"
    
public:
    /**
     * @brief Store a new musical pattern in memory
     */
    void store_pattern(const std::vector<int>& notes, const std::string& name,
                      const CompositionContext& context, const std::string& category = "motif") {
        stored_patterns_.emplace_back(notes, name, context);
        pattern_categories_[category]++;
        
        std::cout << "🧠 Pattern stored: " << name << " (" << category 
                  << "), " << notes.size() << " notes\n";
    }
    
    /**
     * @brief Find similar patterns for musical suggestion
     */
    std::vector<std::vector<int>> suggest_similar_patterns(const std::vector<int>& seed_pattern, 
                                                          const CompositionContext& current_context,
                                                          int max_suggestions = 3) {
        std::vector<std::pair<std::vector<int>, double>> pattern_scores;
        
        for (const auto& stored : stored_patterns_) {
            double similarity = calculate_pattern_similarity(seed_pattern, stored.pattern_notes);
            double context_compatibility = calculate_context_compatibility(
                current_context, stored.original_context);
            
            double overall_score = (similarity * 0.7) + (context_compatibility * 0.3);
            pattern_scores.emplace_back(stored.pattern_notes, overall_score);
        }
        
        // Sort by score and return best suggestions
        std::sort(pattern_scores.begin(), pattern_scores.end(),
                 [](const std::pair<std::vector<int>, double>& a, const std::pair<std::vector<int>, double>& b) { return a.second > b.second; });
        
        std::vector<std::vector<int>> suggestions;
        for (int i = 0; i < std::min(max_suggestions, (int)pattern_scores.size()); ++i) {
            suggestions.push_back(pattern_scores[i].first);
        }
        
        return suggestions;
    }
    
    /**
     * @brief Get pattern usage statistics
     */
    void print_pattern_statistics(std::ostream& os = std::cout) const {
        os << "\n🎼 Musical Pattern Memory Statistics:\n";
        os << "  Total stored patterns: " << stored_patterns_.size() << "\n";
        
        for (const auto& category : pattern_categories_) {
            os << "  " << category.first << " patterns: " << category.second << "\n";
        }
        
        if (!stored_patterns_.empty()) {
            auto most_used = std::max_element(stored_patterns_.begin(), stored_patterns_.end(),
                [](const StoredPattern& a, const StoredPattern& b) { return a.usage_count < b.usage_count; });
            
            os << "  Most used pattern: " << most_used->pattern_name 
               << " (used " << most_used->usage_count << " times)\n";
        }
    }
    
    // Getters
    size_t pattern_count() const { return stored_patterns_.size(); }
    
private:
    double calculate_pattern_similarity(const std::vector<int>& pattern1,
                                       const std::vector<int>& pattern2) {
        // Calculate interval similarity (rhythm-independent)
        std::vector<int> intervals1, intervals2;
        
        for (size_t i = 1; i < pattern1.size(); ++i) {
            intervals1.push_back(pattern1[i] - pattern1[i-1]);
        }
        for (size_t i = 1; i < pattern2.size(); ++i) {
            intervals2.push_back(pattern2[i] - pattern2[i-1]);
        }
        
        if (intervals1.empty() || intervals2.empty()) return 0.0;
        
        // Simple similarity based on matching intervals
        int matches = 0;
        int comparisons = std::min(intervals1.size(), intervals2.size());
        
        for (int i = 0; i < comparisons; ++i) {
            if (intervals1[i] == intervals2[i]) matches++;
        }
        
        return (double)matches / comparisons;
    }
    
    double calculate_context_compatibility(const CompositionContext& context1,
                                          const CompositionContext& context2) {
        double compatibility = 1.0;
        
        // Key signature compatibility
        if (context1.key_signature != context2.key_signature) {
            compatibility *= 0.7; // Different key reduces compatibility
        }
        
        // Time signature compatibility
        if (context1.time_signature_num != context2.time_signature_num ||
            context1.time_signature_den != context2.time_signature_den) {
            compatibility *= 0.8; // Different meter reduces compatibility
        }
        
        // Tempo compatibility
        int tempo_diff = std::abs(context1.current_tempo - context2.current_tempo);
        if (tempo_diff > 20) {
            compatibility *= 0.6; // Very different tempo reduces compatibility
        }
        
        return compatibility;
    }
};

/**
 * @brief Compositional memory manager with musical intelligence
 */
class CompositionalMemoryManager {
private:
    // Memory layers
    std::deque<MusicalDecision> immediate_memory_;      // Last 8-16 decisions
    std::vector<MusicalDecision> short_term_memory_;    // Last phrase/segment decisions
    std::map<std::string, double> long_term_preferences_; // Learned compositional style
    MusicalPatternMemory pattern_memory_;               // Musical motif and pattern storage
    
    // Memory configuration
    size_t immediate_memory_size_;
    size_t short_term_memory_size_;
    double preference_adaptation_rate_;
    
    // Statistical tracking
    int total_decisions_made_;
    std::unordered_map<std::string, int> decision_reason_counts_;
    std::map<int, int> interval_choice_frequency_;
    
public:
    explicit CompositionalMemoryManager(size_t immediate_size = 12,
                                       size_t short_term_size = 64,
                                       double adaptation_rate = 0.1)
        : immediate_memory_size_(immediate_size),
          short_term_memory_size_(short_term_size),
          preference_adaptation_rate_(adaptation_rate),
          total_decisions_made_(0) {
        
        // Initialize default compositional preferences
        long_term_preferences_["stepwise_motion"] = 0.7;
        long_term_preferences_["consonant_intervals"] = 0.8;
        long_term_preferences_["rhythmic_regularity"] = 0.6;
        long_term_preferences_["motivic_development"] = 0.5;
        long_term_preferences_["harmonic_stability"] = 0.7;
    }
    
    /**
     * @brief Record a musical decision in memory
     */
    void record_decision(const MusicalDecision& decision) {
        total_decisions_made_++;
        
        // Add to immediate memory (with size limit)
        immediate_memory_.push_back(decision);
        if (immediate_memory_.size() > immediate_memory_size_) {
            immediate_memory_.pop_front();
        }
        
        // Add to short-term memory
        short_term_memory_.push_back(decision);
        if (short_term_memory_.size() > short_term_memory_size_) {
            // Move oldest to long-term preference learning
            update_long_term_preferences(short_term_memory_[0]);
            short_term_memory_.erase(short_term_memory_.begin());
        }
        
        // Update decision statistics
        decision_reason_counts_[decision.decision_reason]++;
        
        if (!immediate_memory_.empty() && immediate_memory_.size() >= 2) {
            // Track interval preferences
            const auto& prev_decision = immediate_memory_[immediate_memory_.size() - 2];
            int interval = decision.chosen_value - prev_decision.chosen_value;
            interval_choice_frequency_[interval]++;
        }
    }
    
    /**
     * @brief Generate musical suggestions based on memory
     */
    std::vector<std::pair<int, std::string>> suggest_next_choices(
        const DualSolutionStorage& current_storage,
        const CompositionContext& current_context,
        int max_suggestions = 5) {
        
        std::vector<std::pair<int, std::string>> suggestions;
        
        // 1. Pattern-based suggestions from pattern memory
        if (current_storage.length() >= 3) {
            std::vector<int> recent_notes;
            for (int i = std::max(0, current_storage.length() - 4); 
                 i < current_storage.length(); ++i) {
                recent_notes.push_back(current_storage.absolute(i));
            }
            
            auto pattern_suggestions = pattern_memory_.suggest_similar_patterns(
                recent_notes, current_context, 2);
            
            for (const auto& pattern : pattern_suggestions) {
                if (!pattern.empty()) {
                    suggestions.emplace_back(pattern[0], 
                        "Pattern memory: Continue similar musical motif");
                }
            }
        }
        
        // 2. Preference-based suggestions
        if (current_storage.length() > 0) {
            int last_pitch = current_storage.absolute(current_storage.length() - 1);
            
            // Stepwise motion suggestion (if preferred)
            if (long_term_preferences_.at("stepwise_motion") > 0.6) {
                suggestions.emplace_back(last_pitch + 1, 
                    "Memory preference: Stepwise ascending motion");
                suggestions.emplace_back(last_pitch - 1, 
                    "Memory preference: Stepwise descending motion");
            }
            
            // Consonant interval suggestions
            if (long_term_preferences_.at("consonant_intervals") > 0.7) {
                suggestions.emplace_back(last_pitch + 4, 
                    "Memory preference: Consonant major third");
                suggestions.emplace_back(last_pitch + 7, 
                    "Memory preference: Consonant perfect fifth");
            }
        }
        
        // 3. Frequency-based suggestions from decision history
        if (!interval_choice_frequency_.empty()) {
            // Find most frequently used intervals
            auto most_frequent = std::max_element(interval_choice_frequency_.begin(),
                interval_choice_frequency_.end(),
                [](const std::pair<int, int>& a, const std::pair<int, int>& b) { return a.second < b.second; });
            
            if (current_storage.length() > 0) {
                int last_pitch = current_storage.absolute(current_storage.length() - 1);
                suggestions.emplace_back(last_pitch + most_frequent->first,
                    "Memory frequency: Most used interval (" + 
                    std::to_string(most_frequent->first) + " semitones)");
            }
        }
        
        // Limit suggestions to requested maximum
        if (suggestions.size() > (size_t)max_suggestions) {
            suggestions.resize(max_suggestions);
        }
        
        return suggestions;
    }
    
    /**
     * @brief Store a musical pattern in pattern memory
     */
    void store_musical_pattern(const std::vector<int>& notes, 
                              const std::string& pattern_name,
                              const CompositionContext& context,
                              const std::string& category = "motif") {
        pattern_memory_.store_pattern(notes, pattern_name, context, category);
    }
    
    /**
     * @brief Analyze compositional style from memory
     */
    void analyze_compositional_style(std::ostream& os = std::cout) const {
        os << "\n🎯 Compositional Style Analysis:\n";
        os << "  Total decisions recorded: " << total_decisions_made_ << "\n";
        os << "  Immediate memory size: " << immediate_memory_.size() 
           << "/" << immediate_memory_size_ << "\n";
        os << "  Short-term memory size: " << short_term_memory_.size() 
           << "/" << short_term_memory_size_ << "\n";
        
        os << "\n  Long-term Musical Preferences:\n";
        for (const auto& pref : long_term_preferences_) {
            os << "    " << pref.first << ": " 
               << std::fixed << std::setprecision(2) << pref.second << "\n";
        }
        
        os << "\n  Most Common Decision Reasons:\n";
        std::vector<std::pair<std::string, int>> reason_pairs(
            decision_reason_counts_.begin(), decision_reason_counts_.end());
        std::sort(reason_pairs.begin(), reason_pairs.end(),
                 [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) { return a.second > b.second; });
        
        for (size_t i = 0; i < std::min((size_t)3, reason_pairs.size()); ++i) {
            os << "    " << reason_pairs[i].first << ": " 
               << reason_pairs[i].second << " times\n";
        }
        
        os << "\n  Preferred Intervals:\n";
        std::vector<std::pair<int, int>> interval_pairs(
            interval_choice_frequency_.begin(), interval_choice_frequency_.end());
        std::sort(interval_pairs.begin(), interval_pairs.end(),
                 [](const std::pair<int, int>& a, const std::pair<int, int>& b) { return a.second > b.second; });
        
        for (size_t i = 0; i < std::min((size_t)5, interval_pairs.size()); ++i) {
            os << "    " << interval_pairs[i].first << " semitones: " 
               << interval_pairs[i].second << " times\n";
        }
    }
    
    /**
     * @brief Get current preference for a specific musical aspect
     */
    double get_preference(const std::string& preference_type) const {
        auto it = long_term_preferences_.find(preference_type);
        return (it != long_term_preferences_.end()) ? it->second : 0.5;
    }
    
    /**
     * @brief Update a musical preference manually
     */
    void update_preference(const std::string& preference_type, double new_value) {
        long_term_preferences_[preference_type] = std::min(1.0, std::max(0.0, new_value));
    }
    
    /**
     * @brief Get pattern memory statistics
     */
    void print_pattern_memory_stats(std::ostream& os = std::cout) const {
        pattern_memory_.print_pattern_statistics(os);
    }
    
    /**
     * @brief Clear all memory (reset compositional memory)
     */
    void clear_memory() {
        immediate_memory_.clear();
        short_term_memory_.clear();
        decision_reason_counts_.clear();
        interval_choice_frequency_.clear();
        total_decisions_made_ = 0;
        
        // Reset preferences to defaults
        long_term_preferences_["stepwise_motion"] = 0.7;
        long_term_preferences_["consonant_intervals"] = 0.8;
        long_term_preferences_["rhythmic_regularity"] = 0.6;
        long_term_preferences_["motivic_development"] = 0.5;
        long_term_preferences_["harmonic_stability"] = 0.7;
        
        std::cout << "🧹 Compositional memory cleared and reset to defaults\n";
    }
    
    // Getters
    size_t get_total_decisions() const { return total_decisions_made_; }
    size_t get_immediate_memory_size() const { return immediate_memory_.size(); }
    size_t get_short_term_memory_size() const { return short_term_memory_.size(); }
    size_t get_pattern_count() const { return pattern_memory_.pattern_count(); }

private:
    /**
     * @brief Update long-term preferences based on decision patterns
     */
    void update_long_term_preferences(const MusicalDecision& old_decision) {
        // Analyze decision to update preferences
        if (old_decision.decision_reason.find("stepwise") != std::string::npos) {
            double& stepwise_pref = long_term_preferences_["stepwise_motion"];
            stepwise_pref += (old_decision.confidence_level - stepwise_pref) * preference_adaptation_rate_;
        }
        
        if (old_decision.decision_reason.find("consonant") != std::string::npos) {
            double& consonant_pref = long_term_preferences_["consonant_intervals"];
            consonant_pref += (old_decision.confidence_level - consonant_pref) * preference_adaptation_rate_;
        }
        
        if (old_decision.decision_reason.find("pattern") != std::string::npos ||
            old_decision.decision_reason.find("motif") != std::string::npos) {
            double& motivic_pref = long_term_preferences_["motivic_development"];
            motivic_pref += (old_decision.confidence_level - motivic_pref) * preference_adaptation_rate_;
        }
        
        // Clamp all preferences to valid range
        for (auto& pref : long_term_preferences_) {
            pref.second = std::min(1.0, std::max(0.0, pref.second));
        }
    }
};

} // namespace MusicalConstraints

#endif // COMPOSITIONAL_MEMORY_HH