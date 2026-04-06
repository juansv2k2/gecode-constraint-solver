/**
 * @file cluster_engine_heuristic_implementation.cpp
 * @brief Implementation of Enhanced Heuristic Rules System
 * 
 * Implements the sophisticated heuristic rule engine for intelligent
 * musical constraint solving with massive search performance improvements.
 */

#include "cluster_engine_heuristic.hh"
#include "cluster_engine_core.hh"
#include "cluster_engine_interface.hh"
#include <numeric>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>

namespace ClusterEngine {

// =============================================================================
// MusicalAnalysisContext Implementation
// =============================================================================

MusicalAnalysisContext::MusicalAnalysisContext(ClusterEngineCore* core)
    : core_(core), sequences_cached_(false), last_update_checksum_(0) {
    if (core_) {
        int num_engines = core_->get_num_engines();
        pitch_sequences_.resize(num_engines);
        rhythm_sequences_.resize(num_engines);
        onset_sequences_.resize(num_engines);
        metric_sequences_.resize(num_engines);
        total_pitch_counts_.resize(num_engines, 0);
        total_note_counts_.resize(num_engines, 0);
        total_duration_counts_.resize(num_engines, 0);
    }
}

void MusicalAnalysisContext::update_musical_sequences() {
    if (!core_) return;
    
    // Calculate checksum to see if update is needed
    int current_checksum = 0;
    for (int engine = 0; engine < core_->get_num_engines(); ++engine) {
        current_checksum += core_->get_engine(engine).get_index();
        current_checksum += static_cast<int>(core_->get_engine(engine).get_domain().get_candidates().size());
    }
    
    if (current_checksum == last_update_checksum_ && sequences_cached_) {
        return; // No update needed
    }
    
    // Clear existing sequences
    for (int engine = 0; engine < core_->get_num_engines(); ++engine) {
        pitch_sequences_[engine].clear();
        rhythm_sequences_[engine].clear();
        onset_sequences_[engine].clear();
        metric_sequences_[engine].clear();
        total_pitch_counts_[engine] = 0;
        total_note_counts_[engine] = 0;
        total_duration_counts_[engine] = 0;
    }
    
    // Extract musical sequences from current solution
    for (int engine = 0; engine < core_->get_num_engines(); ++engine) {
        const auto& musical_engine = core_->get_engine(engine);
        const auto& domain = musical_engine.get_domain();
        
        int cumulative_notes = 0;
        double cumulative_time = 0.0;
        
        for (int index = 0; index <= musical_engine.get_index(); ++index) {
            if (index < static_cast<int>(domain.get_candidates().size())) {
                const auto& candidate = domain.get_candidates()[index];
                
                // Extract based on engine type
                if (musical_engine.get_type() == EngineType::RHYTHM) {
                    // Rhythm engine - extract durations and timing
                    rhythm_sequences_[engine].push_back(std::abs(candidate.absolute_value));
                    onset_sequences_[engine].push_back(cumulative_time);
                    cumulative_time += std::abs(candidate.absolute_value / 1000.0); // Convert to seconds
                    
                    if (candidate.absolute_value > 0) {
                        cumulative_notes++; // Count non-rest notes
                    }
                    total_duration_counts_[engine]++;
                } else if (musical_engine.get_type() == EngineType::PITCH) {
                    // Pitch engine - extract pitches
                    pitch_sequences_[engine].push_back(candidate.absolute_value);
                    if (candidate.absolute_value > 0) {
                        cumulative_notes++; // Count non-rest pitches
                    }
                    total_pitch_counts_[engine]++;
                } else if (musical_engine.get_type() == EngineType::METRIC) {
                    // Metric engine - extract time signatures
                    metric_sequences_[engine].push_back(candidate.absolute_value);
                }
                
                total_note_counts_[engine] = cumulative_notes;
            }
        }
    }
    
    sequences_cached_ = true;
    last_update_checksum_ = current_checksum;
}

void MusicalAnalysisContext::clear_cache() {
    sequences_cached_ = false;
    last_update_checksum_ = 0;
}

bool MusicalAnalysisContext::is_cache_valid() const {
    return sequences_cached_;
}

std::vector<int> MusicalAnalysisContext::get_pitch_sequence(int engine) const {
    if (engine >= 0 && engine < static_cast<int>(pitch_sequences_.size())) {
        return pitch_sequences_[engine];
    }
    return std::vector<int>();
}

std::vector<int> MusicalAnalysisContext::get_rhythm_sequence(int engine) const {
    if (engine >= 0 && engine < static_cast<int>(rhythm_sequences_.size())) {
        return rhythm_sequences_[engine];
    }
    return std::vector<int>();
}

std::vector<double> MusicalAnalysisContext::get_onset_sequence(int engine) const {
    if (engine >= 0 && engine < static_cast<int>(onset_sequences_.size())) {
        return onset_sequences_[engine];
    }
    return std::vector<double>();
}

std::vector<std::pair<int, int>> MusicalAnalysisContext::get_rhythm_pitch_pairs(int rhythm_engine, int pitch_engine) const {
    std::vector<std::pair<int, int>> pairs;
    
    const auto& rhythms = get_rhythm_sequence(rhythm_engine);
    const auto& pitches = get_pitch_sequence(pitch_engine);
    
    size_t min_size = std::min(rhythms.size(), pitches.size());
    for (size_t i = 0; i < min_size; ++i) {
        pairs.emplace_back(rhythms[i], pitches[i]);
    }
    
    return pairs;
}

std::vector<std::vector<int>> MusicalAnalysisContext::get_harmonic_progression(const std::vector<int>& pitch_engines) const {
    std::vector<std::vector<int>> progression;
    
    if (pitch_engines.empty()) return progression;
    
    // Find the maximum sequence length among all pitch engines
    size_t max_length = 0;
    for (int engine : pitch_engines) {
        const auto& sequence = get_pitch_sequence(engine);
        max_length = std::max(max_length, sequence.size());
    }
    
    // Create harmonic progression (vertical slices)
    for (size_t i = 0; i < max_length; ++i) {
        std::vector<int> chord;
        for (int engine : pitch_engines) {
            const auto& sequence = get_pitch_sequence(engine);
            if (i < sequence.size()) {
                chord.push_back(sequence[i]);
            }
        }
        if (!chord.empty()) {
            progression.push_back(chord);
        }
    }
    
    return progression;
}

int MusicalAnalysisContext::get_total_pitch_count(int engine) const {
    if (engine >= 0 && engine < static_cast<int>(total_pitch_counts_.size())) {
        return total_pitch_counts_[engine];
    }
    return 0;
}

int MusicalAnalysisContext::get_total_note_count(int engine) const {
    if (engine >= 0 && engine < static_cast<int>(total_note_counts_.size())) {
        return total_note_counts_[engine];
    }
    return 0;
}

int MusicalAnalysisContext::get_total_duration_count(int engine) const {
    if (engine >= 0 && engine < static_cast<int>(total_duration_counts_.size())) {
        return total_duration_counts_[engine];
    }
    return 0;
}

int MusicalAnalysisContext::get_pitch_at_count(int engine, int pitch_count) const {
    const auto& sequence = get_pitch_sequence(engine);
    if (pitch_count >= 0 && pitch_count < static_cast<int>(sequence.size())) {
        return sequence[pitch_count];
    }
    return 0; // Default pitch if out of range
}

int MusicalAnalysisContext::get_duration_at_count(int engine, int note_count) const {
    const auto& sequence = get_rhythm_sequence(engine);
    if (note_count >= 0 && note_count < static_cast<int>(sequence.size())) {
        return sequence[note_count];
    }
    return 1000; // Default duration (quarter note) if out of range
}

double MusicalAnalysisContext::get_onset_at_count(int engine, int onset_count) const {
    const auto& sequence = get_onset_sequence(engine);
    if (onset_count >= 0 && onset_count < static_cast<int>(sequence.size())) {
        return sequence[onset_count];
    }
    return 0.0; // Default onset if out of range
}

int MusicalAnalysisContext::count_notes_in_current_candidate(int engine, int candidate_index) const {
    if (!core_ || engine >= core_->get_num_engines()) return 0;
    
    const auto& musical_engine = core_->get_engine(engine);
    const auto& domain = musical_engine.get_domain();
    const auto& candidates = domain.get_candidates();
    
    if (candidate_index >= 0 && candidate_index < static_cast<int>(candidates.size())) {
        const auto& candidate = candidates[candidate_index];
        // For rhythm engines, count non-negative values (notes)
        // For pitch engines, count non-zero values (pitches)
        return (candidate.absolute_value > 0) ? 1 : 0;
    }
    
    return 0;
}

double MusicalAnalysisContext::get_current_candidate_duration(int engine, int candidate_index) const {
    if (!core_ || engine >= core_->get_num_engines()) return 0.0;
    
    const auto& musical_engine = core_->get_engine(engine);
    const auto& domain = musical_engine.get_domain();
    const auto& candidates = domain.get_candidates();
    
    if (candidate_index >= 0 && candidate_index < static_cast<int>(candidates.size())) {
        const auto& candidate = candidates[candidate_index];
        return std::abs(candidate.absolute_value / 1000.0); // Convert to seconds
    }
    
    return 0.0;
}

std::vector<int> MusicalAnalysisContext::get_current_candidate_pitches(int engine, int candidate_index) const {
    std::vector<int> pitches;
    
    if (!core_ || engine >= core_->get_num_engines()) return pitches;
    
    const auto& musical_engine = core_->get_engine(engine);
    const auto& domain = musical_engine.get_domain();
    const auto& candidates = domain.get_candidates();
    
    if (candidate_index >= 0 && candidate_index < static_cast<int>(candidates.size())) {
        const auto& candidate = candidates[candidate_index];
        if (candidate.absolute_value > 0) { // Non-rest
            pitches.push_back(candidate.absolute_value);
        }
    }
    
    return pitches;
}

// =============================================================================
// HeuristicRule Base Class Implementation
// =============================================================================

HeuristicRule::HeuristicRule(const std::vector<int>& engines, HeuristicRuleType type, 
                             const std::string& name, double weight)
    : target_engines_(engines), type_(type), rule_name_(name), base_weight_(weight), enabled_(true) {
}

double HeuristicRule::calculate_average_weight(const std::vector<double>& weights) const {
    if (weights.empty()) return 0.0;
    
    double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
    return sum / static_cast<double>(weights.size());
}

double HeuristicRule::normalize_weight(double raw_weight, double min_val, double max_val) const {
    if (max_val <= min_val) return 0.0;
    return (raw_weight - min_val) / (max_val - min_val);
}

// =============================================================================
// SingleEnginePitchRule Implementation
// =============================================================================

SingleEnginePitchRule::SingleEnginePitchRule(int engine, 
                                              std::function<double(const std::vector<int>&)> pattern_func,
                                              const std::string& name,
                                              int window_size,
                                              double weight)
    : HeuristicRule({engine}, HeuristicRuleType::SINGLE_ENGINE_PITCH, name, weight),
      pitch_pattern_function_(pattern_func),
      analysis_window_size_(window_size),
      target_engine_(engine) {
}

double SingleEnginePitchRule::evaluate_candidate(const MusicalAnalysisContext& context, 
                                                 int engine, int candidate_index) const {
    if (!enabled_ || engine != target_engine_) return 0.0;
    
    // Get current pitch sequence
    auto pitch_sequence = context.get_pitch_sequence(target_engine_);
    
    // Add the candidate pitches to create analysis window
    auto candidate_pitches = context.get_current_candidate_pitches(engine, candidate_index);
    for (int pitch : candidate_pitches) {
        pitch_sequence.push_back(pitch);
    }
    
    // Extract analysis window
    if (pitch_sequence.size() < static_cast<size_t>(analysis_window_size_)) {
        return 0.0; // Not enough data for analysis
    }
    
    std::vector<double> window_weights;
    
    // Analyze all possible windows of the specified size
    for (size_t i = 0; i <= pitch_sequence.size() - analysis_window_size_; ++i) {
        std::vector<int> window(pitch_sequence.begin() + i, 
                                pitch_sequence.begin() + i + analysis_window_size_);
        
        double window_weight = pitch_pattern_function_(window);
        window_weights.push_back(window_weight);
    }
    
    return base_weight_ * calculate_average_weight(window_weights);
}

std::function<double(const std::vector<int>&)> SingleEnginePitchRule::create_stepwise_motion_preference() {
    return [](const std::vector<int>& pitches) -> double {
        if (pitches.size() < 2) return 0.0;
        
        double total_weight = 0.0;
        int stepwise_moves = 0;
        
        for (size_t i = 1; i < pitches.size(); ++i) {
            int interval = std::abs(pitches[i] - pitches[i-1]);
            if (interval >= 1 && interval <= 2) { // Stepwise motion (1-2 semitones)
                stepwise_moves++;
                total_weight += 1.0;
            } else if (interval >= 3 && interval <= 7) { // Small leaps (3-7 semitones)
                total_weight += 0.5;
            } else if (interval > 7) { // Large leaps
                total_weight -= 0.5;
            }
        }
        
        return total_weight / static_cast<double>(pitches.size() - 1);
    };
}

std::function<double(const std::vector<int>&)> SingleEnginePitchRule::create_consonant_intervals_preference() {
    return [](const std::vector<int>& pitches) -> double {
        if (pitches.size() < 2) return 0.0;
        
        double total_weight = 0.0;
        
        for (size_t i = 1; i < pitches.size(); ++i) {
            int interval = std::abs(pitches[i] - pitches[i-1]) % 12;
            
            // Consonant intervals: unison, perfect 4th, perfect 5th, octave
            if (interval == 0 || interval == 5 || interval == 7) {
                total_weight += 1.0; // Perfect consonance
            } else if (interval == 3 || interval == 4 || interval == 8 || interval == 9) {
                total_weight += 0.7; // Imperfect consonance (3rds and 6ths)
            } else if (interval == 2 || interval == 10) {
                total_weight += 0.3; // Mild dissonance (2nds and 7ths)
            } else {
                total_weight -= 0.2; // Strong dissonance (tritone, etc.)
            }
        }
        
        return total_weight / static_cast<double>(pitches.size() - 1);
    };
}

std::function<double(const std::vector<int>&)> SingleEnginePitchRule::create_melodic_contour_preference() {
    return [](const std::vector<int>& pitches) -> double {
        if (pitches.size() < 3) return 0.0;
        
        double total_weight = 0.0;
        int direction_changes = 0;
        
        for (size_t i = 2; i < pitches.size(); ++i) {
            int prev_direction = (pitches[i-1] > pitches[i-2]) ? 1 : (pitches[i-1] < pitches[i-2]) ? -1 : 0;
            int curr_direction = (pitches[i] > pitches[i-1]) ? 1 : (pitches[i] < pitches[i-1]) ? -1 : 0;
            
            if (prev_direction != 0 && curr_direction != 0 && prev_direction != curr_direction) {
                direction_changes++;
            }
        }
        
        // Prefer moderate contour complexity (not too static, not too chaotic)
        double complexity_ratio = static_cast<double>(direction_changes) / (pitches.size() - 2);
        if (complexity_ratio >= 0.3 && complexity_ratio <= 0.7) {
            total_weight = 1.0;
        } else if (complexity_ratio < 0.3) {
            total_weight = 0.5; // Too static
        } else {
            total_weight = 0.3; // Too chaotic
        }
        
        return total_weight;
    };
}

std::function<double(const std::vector<int>&)> SingleEnginePitchRule::create_pitch_stability_preference() {
    return [](const std::vector<int>& pitches) -> double {
        if (pitches.size() < 2) return 0.0;
        
        // Calculate pitch variance to measure stability
        double mean = std::accumulate(pitches.begin(), pitches.end(), 0.0) / pitches.size();
        double variance = 0.0;
        
        for (int pitch : pitches) {
            variance += (pitch - mean) * (pitch - mean);
        }
        variance /= pitches.size();
        
        double std_dev = std::sqrt(variance);
        
        // Prefer moderate stability (not too static, not too wide)
        if (std_dev >= 3.0 && std_dev <= 8.0) {
            return 1.0; // Good stability
        } else if (std_dev < 3.0) {
            return 0.6; // Too static
        } else {
            return 0.4; // Too wide range
        }
    };
}

} // namespace ClusterEngine

namespace ClusterEngine {

// =============================================================================
// SingleEngineRhythmRule Implementation
// =============================================================================

SingleEngineRhythmRule::SingleEngineRhythmRule(int engine,
                                               std::function<double(const std::vector<int>&)> pattern_func,
                                               const std::string& name,
                                               int window_size,
                                               double weight)
    : HeuristicRule({engine}, HeuristicRuleType::SINGLE_ENGINE_RHYTHM, name, weight),
      rhythm_pattern_function_(pattern_func),
      analysis_window_size_(window_size),
      target_engine_(engine) {
}

double SingleEngineRhythmRule::evaluate_candidate(const MusicalAnalysisContext& context, 
                                                  int engine, int candidate_index) const {
    if (!enabled_ || engine != target_engine_) return 0.0;
    
    // Get current rhythm sequence
    auto rhythm_sequence = context.get_rhythm_sequence(target_engine_);
    
    // Add the candidate duration
    double candidate_duration = context.get_current_candidate_duration(engine, candidate_index);
    rhythm_sequence.push_back(static_cast<int>(candidate_duration * 1000)); // Convert back to milliseconds
    
    // Extract analysis window
    if (rhythm_sequence.size() < static_cast<size_t>(analysis_window_size_)) {
        return 0.0; // Not enough data for analysis
    }
    
    std::vector<double> window_weights;
    
    // Analyze all possible windows of the specified size
    for (size_t i = 0; i <= rhythm_sequence.size() - analysis_window_size_; ++i) {
        std::vector<int> window(rhythm_sequence.begin() + i, 
                               rhythm_sequence.begin() + i + analysis_window_size_);
        
        double window_weight = rhythm_pattern_function_(window);
        window_weights.push_back(window_weight);
    }
    
    return base_weight_ * calculate_average_weight(window_weights);
}

std::function<double(const std::vector<int>&)> SingleEngineRhythmRule::create_rhythmic_regularity_preference() {
    return [](const std::vector<int>& durations) -> double {
        if (durations.size() < 2) return 0.0;
        
        // Calculate how regular the durations are
        std::map<int, int> duration_counts;
        for (int duration : durations) {
            duration_counts[duration]++;
        }
        
        // Find the most common duration
        int max_count = 0;
        for (const auto& pair : duration_counts) {
            max_count = std::max(max_count, pair.second);
        }
        
        double regularity = static_cast<double>(max_count) / durations.size();
        
        // Prefer moderate regularity (not too repetitive, not too chaotic)
        if (regularity >= 0.4 && regularity <= 0.8) {
            return 1.0;
        } else if (regularity < 0.4) {
            return 0.5; // Too chaotic
        } else {
            return 0.3; // Too repetitive
        }
    };
}

std::function<double(const std::vector<int>&)> SingleEngineRhythmRule::create_syncopation_avoidance() {
    return [](const std::vector<int>& durations) -> double {
        if (durations.size() < 2) return 0.0;
        
        double total_weight = 0.0;
        
        for (size_t i = 1; i < durations.size(); ++i) {
            // Prefer durations that are simple ratios
            int ratio = durations[i-1] / std::gcd(durations[i-1], durations[i]);
            
            if (ratio == 1 || ratio == 2 || ratio == 4) {
                total_weight += 1.0; // Simple ratios
            } else if (ratio == 3 || ratio == 6) {
                total_weight += 0.5; // Moderate complexity
            } else {
                total_weight -= 0.3; // Complex syncopation
            }
        }
        
        return total_weight / static_cast<double>(durations.size() - 1);
    };
}

std::function<double(const std::vector<int>&)> SingleEngineRhythmRule::create_rhythmic_variety_preference() {
    return [](const std::vector<int>& durations) -> double {
        if (durations.size() < 2) return 0.0;
        
        std::set<int> unique_durations(durations.begin(), durations.end());
        double variety = static_cast<double>(unique_durations.size()) / durations.size();
        
        // Prefer moderate variety
        if (variety >= 0.3 && variety <= 0.7) {
            return 1.0;
        } else if (variety < 0.3) {
            return 0.4; // Too repetitive
        } else {
            return 0.6; // Good variety
        }
    };
}

std::function<double(const std::vector<int>&)> SingleEngineRhythmRule::create_metric_alignment_preference() {
    return [](const std::vector<int>& durations) -> double {
        if (durations.empty()) return 0.0;
        
        double total_weight = 0.0;
        
        for (int duration : durations) {
            // Prefer durations that align with common beat subdivisions
            if (duration == 1000 || duration == 500 || duration == 250) {
                total_weight += 1.0; // Strong beats
            } else if (duration == 750 || duration == 375) {
                total_weight += 0.7; // Dotted notes
            } else if (duration == 125 || duration == 2000) {
                total_weight += 0.5; // Sixteenth notes, half notes
            } else {
                total_weight += 0.2; // Other durations
            }
        }
        
        return total_weight / durations.size();
    };
}

// =============================================================================
// DualEngineRhythmPitchRule Implementation
// =============================================================================

DualEngineRhythmPitchRule::DualEngineRhythmPitchRule(int rhythm_engine, int pitch_engine,
                                                     std::function<double(const std::vector<std::pair<int, int>>&)> coord_func,
                                                     const std::string& name,
                                                     int window_size,
                                                     double weight)
    : HeuristicRule({rhythm_engine, pitch_engine}, HeuristicRuleType::DUAL_ENGINE_PITCH_RHYTHM, name, weight),
      coordination_function_(coord_func),
      rhythm_engine_(rhythm_engine),
      pitch_engine_(pitch_engine),
      analysis_window_size_(window_size) {
}

double DualEngineRhythmPitchRule::evaluate_candidate(const MusicalAnalysisContext& context, 
                                                     int engine, int candidate_index) const {
    if (!enabled_) return 0.0;
    
    // Only evaluate if this is one of our target engines
    if (engine != rhythm_engine_ && engine != pitch_engine_) return 0.0;
    
    // Get rhythm-pitch pairs including the candidate
    auto pairs = context.get_rhythm_pitch_pairs(rhythm_engine_, pitch_engine_);
    
    // Add the candidate contribution
    if (engine == rhythm_engine_) {
        double candidate_duration = context.get_current_candidate_duration(engine, candidate_index);
        // Find corresponding pitch (assume current pitch sequence end)
        auto pitch_sequence = context.get_pitch_sequence(pitch_engine_);
        int corresponding_pitch = pitch_sequence.empty() ? 60 : pitch_sequence.back(); // Default to middle C
        pairs.emplace_back(static_cast<int>(candidate_duration * 1000), corresponding_pitch);
    } else if (engine == pitch_engine_) {
        auto candidate_pitches = context.get_current_candidate_pitches(engine, candidate_index);
        if (!candidate_pitches.empty()) {
            // Find corresponding rhythm (assume current rhythm sequence end)
            auto rhythm_sequence = context.get_rhythm_sequence(rhythm_engine_);
            int corresponding_rhythm = rhythm_sequence.empty() ? 1000 : rhythm_sequence.back(); // Default to quarter note
            pairs.emplace_back(corresponding_rhythm, candidate_pitches[0]);
        }
    }
    
    if (pairs.size() < static_cast<size_t>(analysis_window_size_)) {
        return 0.0; // Not enough data for analysis
    }
    
    std::vector<double> window_weights;
    
    // Analyze all possible windows
    for (size_t i = 0; i <= pairs.size() - analysis_window_size_; ++i) {
        std::vector<std::pair<int, int>> window(pairs.begin() + i,
                                               pairs.begin() + i + analysis_window_size_);
        
        double window_weight = coordination_function_(window);
        window_weights.push_back(window_weight);
    }
    
    return base_weight_ * calculate_average_weight(window_weights);
}

std::function<double(const std::vector<std::pair<int, int>>&)> DualEngineRhythmPitchRule::create_strong_beat_emphasis() {
    return [](const std::vector<std::pair<int, int>>& pairs) -> double {
        if (pairs.empty()) return 0.0;
        
        double total_weight = 0.0;
        
        for (const auto& pair : pairs) {
            int duration = pair.first;
            int pitch = pair.second;
            
            // Strong beat durations get emphasis on important pitches
            if (duration >= 1000) { // Half note or longer
                if (pitch % 12 == 0 || pitch % 12 == 7) { // C or G (tonic/dominant)
                    total_weight += 1.5;
                } else {
                    total_weight += 1.0;
                }
            } else if (duration >= 500) { // Quarter note
                total_weight += 0.8;
            } else {
                total_weight += 0.5; // Shorter durations
            }
        }
        
        return total_weight / pairs.size();
    };
}

std::function<double(const std::vector<std::pair<int, int>>&)> DualEngineRhythmPitchRule::create_rhythmic_pitch_correlation() {
    return [](const std::vector<std::pair<int, int>>& pairs) -> double {
        if (pairs.size() < 2) return 0.0;
        
        double total_weight = 0.0;
        
        for (size_t i = 1; i < pairs.size(); ++i) {
            int rhythm_change = std::abs(pairs[i].first - pairs[i-1].first);
            int pitch_change = std::abs(pairs[i].second - pairs[i-1].second);
            
            // Prefer coordinated changes (both change together or both stay stable)
            if ((rhythm_change > 100 && pitch_change > 2) || (rhythm_change <= 100 && pitch_change <= 2)) {
                total_weight += 1.0; // Good coordination
            } else {
                total_weight += 0.3; // Poor coordination
            }
        }
        
        return total_weight / static_cast<double>(pairs.size() - 1);
    };
}

std::function<double(const std::vector<std::pair<int, int>>&)> DualEngineRhythmPitchRule::create_cadential_pattern_preference() {
    return [](const std::vector<std::pair<int, int>>& pairs) -> double {
        if (pairs.size() < 2) return 0.0;
        
        double total_weight = 0.0;
        
        // Look for cadential patterns (longer notes with resolution)
        for (size_t i = 1; i < pairs.size(); ++i) {
            int prev_duration = pairs[i-1].first;
            int curr_duration = pairs[i].first;
            int prev_pitch = pairs[i-1].second;
            int curr_pitch = pairs[i].second;
            
            // Cadential pattern: longer duration followed by resolution
            if (prev_duration >= 750 && curr_duration >= 500) {
                int pitch_interval = std::abs(curr_pitch - prev_pitch);
                if (pitch_interval >= 1 && pitch_interval <= 7) {
                    total_weight += 1.5; // Strong cadential motion
                } else {
                    total_weight += 0.8; // Moderate cadential motion
                }
            } else {
                total_weight += 0.5;
            }
        }
        
        return total_weight / static_cast<double>(pairs.size() - 1);
    };
}

// =============================================================================
// MultiEngineHarmonicRule Implementation  
// =============================================================================

MultiEngineHarmonicRule::MultiEngineHarmonicRule(const std::vector<int>& pitch_engines,
                                                 std::function<double(const std::vector<std::vector<int>>&)> harm_func,
                                                 const std::string& name,
                                                 int window_size,
                                                 double weight)
    : HeuristicRule(pitch_engines, HeuristicRuleType::MULTI_ENGINE_HARMONIC, name, weight),
      harmonic_function_(harm_func),
      pitch_engines_(pitch_engines),
      analysis_window_size_(window_size) {
}

double MultiEngineHarmonicRule::evaluate_candidate(const MusicalAnalysisContext& context, 
                                                   int engine, int candidate_index) const {
    if (!enabled_) return 0.0;
    
    // Only evaluate if this is one of our target engines
    if (std::find(pitch_engines_.begin(), pitch_engines_.end(), engine) == pitch_engines_.end()) {
        return 0.0;
    }
    
    // Get harmonic progression including the candidate
    auto progression = context.get_harmonic_progression(pitch_engines_);
    
    // Add the candidate's contribution to the progression
    auto candidate_pitches = context.get_current_candidate_pitches(engine, candidate_index);
    if (!candidate_pitches.empty()) {
        // Create new harmonic moment with candidate
        std::vector<int> new_chord;
        for (int pitch_engine : pitch_engines_) {
            if (pitch_engine == engine) {
                new_chord.push_back(candidate_pitches[0]);
            } else {
                // Use current pitch from other engines
                auto sequence = context.get_pitch_sequence(pitch_engine);
                if (!sequence.empty()) {
                    new_chord.push_back(sequence.back());
                } else {
                    new_chord.push_back(60); // Default to middle C
                }
            }
        }
        progression.push_back(new_chord);
    }
    
    if (progression.size() < static_cast<size_t>(analysis_window_size_)) {
        return 0.0; // Not enough data for analysis
    }
    
    std::vector<double> window_weights;
    
    // Analyze all possible windows
    for (size_t i = 0; i <= progression.size() - analysis_window_size_; ++i) {
        std::vector<std::vector<int>> window(progression.begin() + i,
                                            progression.begin() + i + analysis_window_size_);
        
        double window_weight = harmonic_function_(window);
        window_weights.push_back(window_weight);
    }
    
    return base_weight_ * calculate_average_weight(window_weights);
}

std::function<double(const std::vector<std::vector<int>>&)> MultiEngineHarmonicRule::create_consonance_preference() {
    return [](const std::vector<std::vector<int>>& progression) -> double {
        if (progression.empty()) return 0.0;
        
        double total_weight = 0.0;
        
        for (const auto& chord : progression) {
            if (chord.size() < 2) {
                total_weight += 0.5; // Single notes get neutral weight
                continue;
            }
            
            double chord_weight = 0.0;
            int interval_count = 0;
            
            // Analyze all intervals in the chord
            for (size_t i = 0; i < chord.size(); ++i) {
                for (size_t j = i + 1; j < chord.size(); ++j) {
                    int interval = std::abs(chord[j] - chord[i]) % 12;
                    
                    // Rate consonance
                    if (interval == 0 || interval == 7) {
                        chord_weight += 1.0; // Perfect consonance (unison, fifth)
                    } else if (interval == 3 || interval == 4 || interval == 8 || interval == 9) {
                        chord_weight += 0.8; // Imperfect consonance (thirds, sixths)
                    } else if (interval == 5) {
                        chord_weight += 0.6; // Perfect fourth
                    } else if (interval == 2 || interval == 10) {
                        chord_weight += 0.3; // Mild dissonance (seconds, sevenths)
                    } else {
                        chord_weight -= 0.3; // Strong dissonance (tritone)
                    }
                    interval_count++;
                }
            }
            
            if (interval_count > 0) {
                total_weight += chord_weight / interval_count;
            }
        }
        
        return total_weight / progression.size();
    };
}

std::function<double(const std::vector<std::vector<int>>&)> MultiEngineHarmonicRule::create_voice_leading_smoothness() {
    return [](const std::vector<std::vector<int>>& progression) -> double {
        if (progression.size() < 2) return 0.0;
        
        double total_weight = 0.0;
        
        for (size_t i = 1; i < progression.size(); ++i) {
            const auto& prev_chord = progression[i-1];
            const auto& curr_chord = progression[i];
            
            if (prev_chord.empty() || curr_chord.empty()) continue;
            
            double chord_motion_weight = 0.0;
            size_t voice_count = std::min(prev_chord.size(), curr_chord.size());
            
            // Analyze voice leading for each voice
            for (size_t voice = 0; voice < voice_count; ++voice) {
                int motion = std::abs(curr_chord[voice] - prev_chord[voice]);
                
                if (motion == 0) {
                    chord_motion_weight += 1.0; // Common tone (best)
                } else if (motion <= 2) {
                    chord_motion_weight += 0.9; // Stepwise motion (very good)
                } else if (motion <= 7) {
                    chord_motion_weight += 0.6; // Small leap (acceptable)
                } else {
                    chord_motion_weight += 0.2; // Large leap (poor)
                }
            }
            
            if (voice_count > 0) {
                total_weight += chord_motion_weight / voice_count;
            }
        }
        
        return total_weight / static_cast<double>(progression.size() - 1);
    };
}

std::function<double(const std::vector<std::vector<int>>&)> MultiEngineHarmonicRule::create_chord_progression_logic() {
    return [](const std::vector<std::vector<int>>& progression) -> double {
        if (progression.size() < 2) return 0.0;
        
        double total_weight = 0.0;
        
        for (size_t i = 1; i < progression.size(); ++i) {
            const auto& prev_chord = progression[i-1];
            const auto& curr_chord = progression[i];
            
            if (prev_chord.empty() || curr_chord.empty()) continue;
            
            // Analyze root motion (assume lowest note is root)
            int prev_root = *std::min_element(prev_chord.begin(), prev_chord.end());
            int curr_root = *std::min_element(curr_chord.begin(), curr_chord.end());
            int root_motion = std::abs(curr_root - prev_root) % 12;
            
            // Rate root motion quality
            if (root_motion == 7 || root_motion == 5) {
                total_weight += 1.5; // Dominant motion (fifth/fourth)
            } else if (root_motion == 3 || root_motion == 4) {
                total_weight += 1.2; // Third motion
            } else if (root_motion == 2 || root_motion == 10) {
                total_weight += 0.8; // Second motion
            } else if (root_motion == 0) {
                total_weight += 0.5; // Static root
            } else {
                total_weight += 0.3; // Other motions
            }
        }
        
        return total_weight / static_cast<double>(progression.size() - 1);
    };
}

std::function<double(const std::vector<std::vector<int>>&)> MultiEngineHarmonicRule::create_parallel_motion_avoidance() {
    return [](const std::vector<std::vector<int>>& progression) -> double {
        if (progression.size() < 2) return 0.0;
        
        double total_weight = 0.0;
        int motion_count = 0;
        
        for (size_t i = 1; i < progression.size(); ++i) {
            const auto& prev_chord = progression[i-1];
            const auto& curr_chord = progression[i];
            
            if (prev_chord.size() < 2 || curr_chord.size() < 2) continue;
            
            size_t voice_count = std::min(prev_chord.size(), curr_chord.size());
            
            // Check for parallel motion between all voice pairs
            for (size_t j = 0; j < voice_count; ++j) {
                for (size_t k = j + 1; k < voice_count; ++k) {
                    int prev_interval = prev_chord[k] - prev_chord[j];
                    int curr_interval = curr_chord[k] - curr_chord[j];
                    
                    int j_motion = curr_chord[j] - prev_chord[j];
                    int k_motion = curr_chord[k] - prev_chord[k];
                    
                    // Check for parallel motion
                    bool parallel_motion = (j_motion != 0 && k_motion != 0 && 
                                          ((j_motion > 0 && k_motion > 0) || (j_motion < 0 && k_motion < 0)));
                    
                    if (parallel_motion) {
                        int interval_class = std::abs(prev_interval) % 12;
                        if (interval_class == 0 || interval_class == 7) {
                            total_weight -= 1.0; // Parallel unisons/fifths (forbidden)
                        } else {
                            total_weight += 0.3; // Other parallel motion (acceptable)
                        }
                    } else {
                        total_weight += 1.0; // Good independent motion
                    }
                    motion_count++;
                }
            }
        }
        
        return motion_count > 0 ? total_weight / motion_count : 0.0;
    };
}

// =============================================================================
// SwitchHeuristicRule Implementation
// =============================================================================

SwitchHeuristicRule::SwitchHeuristicRule(const std::vector<int>& engines,
                                         std::function<bool(const MusicalAnalysisContext&, int, int)> condition,
                                         double weight,
                                         const std::string& name)
    : HeuristicRule(engines, HeuristicRuleType::SWITCH_RULE, name, weight),
      condition_function_(condition),
      switch_weight_(weight) {
}

double SwitchHeuristicRule::evaluate_candidate(const MusicalAnalysisContext& context, 
                                               int engine, int candidate_index) const {
    if (!enabled_) return 0.0;
    
    // Check if this engine is in our target list
    if (std::find(target_engines_.begin(), target_engines_.end(), engine) == target_engines_.end()) {
        return 0.0;
    }
    
    bool condition_met = condition_function_(context, engine, candidate_index);
    return condition_met ? switch_weight_ : 0.0;
}

std::function<bool(const MusicalAnalysisContext&, int, int)> SwitchHeuristicRule::create_avoid_repetition_condition() {
    return [](const MusicalAnalysisContext& context, int engine, int candidate_index) -> bool {
        auto candidate_pitches = context.get_current_candidate_pitches(engine, candidate_index);
        if (candidate_pitches.empty()) return true; // Allow rests
        
        auto pitch_sequence = context.get_pitch_sequence(engine);
        if (pitch_sequence.empty()) return true; // No previous pitches to compare
        
        int candidate_pitch = candidate_pitches[0];
        int last_pitch = pitch_sequence.back();
        
        return candidate_pitch != last_pitch; // Avoid immediate repetition
    };
}

std::function<bool(const MusicalAnalysisContext&, int, int)> SwitchHeuristicRule::create_range_enforcement_condition(int min_pitch, int max_pitch) {
    return [min_pitch, max_pitch](const MusicalAnalysisContext& context, int engine, int candidate_index) -> bool {
        auto candidate_pitches = context.get_current_candidate_pitches(engine, candidate_index);
        if (candidate_pitches.empty()) return true; // Allow rests
        
        int candidate_pitch = candidate_pitches[0];
        return candidate_pitch >= min_pitch && candidate_pitch <= max_pitch;
    };
}

std::function<bool(const MusicalAnalysisContext&, int, int)> SwitchHeuristicRule::create_metric_position_condition() {
    return [](const MusicalAnalysisContext& context, int engine, int candidate_index) -> bool {
        auto onset_sequence = context.get_onset_sequence(engine);
        if (onset_sequence.empty()) return true;
        
        double current_time = onset_sequence.back();
        double beat_position = std::fmod(current_time, 1.0); // Assuming 1.0 = one beat
        
        // Prefer candidates that align with strong metric positions
        return beat_position < 0.1 || beat_position > 0.9; // Near beat boundaries
    };
}

// =============================================================================
// HeuristicRuleManager Implementation
// =============================================================================

HeuristicRuleManager::HeuristicRuleManager(ClusterEngineCore* core)
    : core_(core), weights_cache_valid_(false), last_weights_checksum_(0),
      rule_evaluations_count_(0), total_evaluation_time_(0.0) {
    
    if (core_) {
        analysis_context_ = std::make_unique<MusicalAnalysisContext>(core_);
        
        int num_engines = core_->get_num_engines();
        rules_by_engine_.resize(num_engines);
        cached_weights_.resize(num_engines);
        
        for (int engine = 0; engine < num_engines; ++engine) {
            cached_weights_[engine].clear();
        }
    }
}

HeuristicRuleManager::~HeuristicRuleManager() = default;

void HeuristicRuleManager::add_rule(std::unique_ptr<HeuristicRule> rule) {
    if (!rule) return;
    
    const auto& target_engines = rule->get_target_engines();
    
    if (target_engines.size() == 1) {
        // Single-engine rule
        int engine = target_engines[0];
        if (engine >= 0 && engine < static_cast<int>(rules_by_engine_.size())) {
            rules_by_engine_[engine].push_back(std::move(rule));
        }
    } else {
        // Multi-engine rule goes to global rules
        global_rules_.push_back(std::move(rule));
    }
    
    invalidate_weights_cache();
}

void HeuristicRuleManager::remove_rule(const std::string& rule_name) {
    // Remove from engine-specific rules
    for (auto& engine_rules : rules_by_engine_) {
        engine_rules.erase(
            std::remove_if(engine_rules.begin(), engine_rules.end(),
                          [&rule_name](const std::unique_ptr<HeuristicRule>& rule) {
                              return rule && rule->get_name() == rule_name;
                          }),
            engine_rules.end());
    }
    
    // Remove from global rules
    global_rules_.erase(
        std::remove_if(global_rules_.begin(), global_rules_.end(),
                      [&rule_name](const std::unique_ptr<HeuristicRule>& rule) {
                          return rule && rule->get_name() == rule_name;
                      }),
        global_rules_.end());
    
    invalidate_weights_cache();
}

void HeuristicRuleManager::enable_rule(const std::string& rule_name) {
    // Search in engine-specific rules
    for (auto& engine_rules : rules_by_engine_) {
        for (auto& rule : engine_rules) {
            if (rule && rule->get_name() == rule_name) {
                rule->enable();
                invalidate_weights_cache();
                return;
            }
        }
    }
    
    // Search in global rules
    for (auto& rule : global_rules_) {
        if (rule && rule->get_name() == rule_name) {
            rule->enable();
            invalidate_weights_cache();
            return;
        }
    }
}

void HeuristicRuleManager::disable_rule(const std::string& rule_name) {
    // Search in engine-specific rules
    for (auto& engine_rules : rules_by_engine_) {
        for (auto& rule : engine_rules) {
            if (rule && rule->get_name() == rule_name) {
                rule->disable();
                invalidate_weights_cache();
                return;
            }
        }
    }
    
    // Search in global rules
    for (auto& rule : global_rules_) {
        if (rule && rule->get_name() == rule_name) {
            rule->disable();
            invalidate_weights_cache();
            return;
        }
    }
}

void HeuristicRuleManager::apply_heuristics_to_domain(int engine) {
    if (!core_ || engine < 0 || engine >= core_->get_num_engines()) return;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Update analysis context
    update_analysis_context();
    
    // Calculate candidate weights
    auto weights = calculate_candidate_weights(engine);
    
    // Apply weights to domain candidates
    auto& musical_engine = core_->get_engine(engine);
    auto& domain = musical_engine.get_domain();
    auto& candidates = domain.get_candidates();
    
    if (weights.size() == candidates.size()) {
        for (size_t i = 0; i < candidates.size(); ++i) {
            domain.set_heuristic_weight(i, weights[i]);
        }
        
        // Sort domain by heuristic weights
        domain.sort_by_heuristic_weights();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    total_evaluation_time_ += duration.count() / 1000.0; // Convert to milliseconds
    rule_evaluations_count_++;
}

std::vector<double> HeuristicRuleManager::calculate_candidate_weights(int engine) {
    std::vector<double> weights;
    
    if (!core_ || engine < 0 || engine >= core_->get_num_engines()) {
        return weights;
    }
    
    const auto& musical_engine = core_->get_engine(engine);
    const auto& domain = musical_engine.get_domain();
    const auto& candidates = domain.get_candidates();
    
    weights.resize(candidates.size(), 1.0); // Default weight
    
    // Apply engine-specific rules
    if (engine < static_cast<int>(rules_by_engine_.size())) {
        for (const auto& rule : rules_by_engine_[engine]) {
            if (!rule || !rule->is_enabled()) continue;
            
            for (size_t candidate_idx = 0; candidate_idx < candidates.size(); ++candidate_idx) {
                double rule_weight = rule->evaluate_candidate(*analysis_context_, engine, 
                                                            static_cast<int>(candidate_idx));
                weights[candidate_idx] += rule_weight;
                
                // Track rule usage
                rule_usage_stats_[rule->get_name()]++;
            }
        }
    }
    
    // Apply global rules
    for (const auto& rule : global_rules_) {
        if (!rule || !rule->is_enabled()) continue;
        
        // Check if this rule applies to this engine
        const auto& target_engines = rule->get_target_engines();
        if (std::find(target_engines.begin(), target_engines.end(), engine) != target_engines.end()) {
            for (size_t candidate_idx = 0; candidate_idx < candidates.size(); ++candidate_idx) {
                double rule_weight = rule->evaluate_candidate(*analysis_context_, engine, 
                                                            static_cast<int>(candidate_idx));
                weights[candidate_idx] += rule_weight;
                
                // Track rule usage
                rule_usage_stats_[rule->get_name()]++;
            }
        }
    }
    
    // Ensure all weights are positive
    double min_weight = *std::min_element(weights.begin(), weights.end());
    if (min_weight < 0.1) {
        for (double& weight : weights) {
            weight += (0.1 - min_weight); // Shift to ensure minimum weight of 0.1
        }
    }
    
    return weights;
}

void HeuristicRuleManager::sort_domain_by_heuristics(int engine) {
    if (!core_ || engine < 0 || engine >= core_->get_num_engines()) return;
    
    auto& musical_engine = core_->get_engine(engine);
    auto& domain = musical_engine.get_domain();
    domain.sort_by_heuristic_weights();
}

void HeuristicRuleManager::invalidate_weights_cache() {
    weights_cache_valid_ = false;
    last_weights_checksum_ = 0;
    
    for (auto& engine_weights : cached_weights_) {
        engine_weights.clear();
    }
}

void HeuristicRuleManager::update_analysis_context() {
    if (analysis_context_) {
        analysis_context_->update_musical_sequences();
    }
}

void HeuristicRuleManager::add_default_pitch_rules(int engine) {
    add_rule(HeuristicRuleFactory::create_stepwise_motion_rule(engine, 1.5));
    add_rule(HeuristicRuleFactory::create_consonant_intervals_rule(engine, 1.2));
    add_rule(HeuristicRuleFactory::create_pitch_stability_rule(engine, 1.0));
    add_rule(HeuristicRuleFactory::create_no_repetition_switch(engine, -2.0));
}

void HeuristicRuleManager::add_default_rhythm_rules(int engine) {
    add_rule(HeuristicRuleFactory::create_rhythmic_regularity_rule(engine, 1.3));
}

void HeuristicRuleManager::add_default_harmonic_rules(const std::vector<int>& pitch_engines) {
    if (pitch_engines.size() >= 2) {
        add_rule(HeuristicRuleFactory::create_harmonic_consonance_rule(pitch_engines, 2.0));
        add_rule(HeuristicRuleFactory::create_voice_leading_rule(pitch_engines, 1.8));
    }
}

void HeuristicRuleManager::add_default_coordination_rules(int rhythm_engine, int pitch_engine) {
    add_rule(HeuristicRuleFactory::create_rhythm_pitch_coordination_rule(rhythm_engine, pitch_engine, 1.5));
}

double HeuristicRuleManager::get_average_evaluation_time() const {
    return rule_evaluations_count_ > 0 ? total_evaluation_time_ / rule_evaluations_count_ : 0.0;
}

void HeuristicRuleManager::print_rule_statistics() const {
    std::cout << "\n🔍 Heuristic Rule Statistics:\n";
    std::cout << "   Total rule evaluations: " << rule_evaluations_count_ << "\n";
    std::cout << "   Average evaluation time: " << std::fixed << std::setprecision(3) 
              << get_average_evaluation_time() << "ms\n";
    std::cout << "   Total evaluation time: " << total_evaluation_time_ << "ms\n";
    
    std::cout << "\n📊 Rule Usage Statistics:\n";
    for (const auto& stat : rule_usage_stats_) {
        std::cout << "   " << stat.first << ": " << stat.second << " evaluations\n";
    }
}

void HeuristicRuleManager::reset_statistics() {
    rule_evaluations_count_ = 0;
    total_evaluation_time_ = 0.0;
    rule_usage_stats_.clear();
}

void HeuristicRuleManager::balance_rule_weights_automatically() {
    // Simple automatic balancing based on rule effectiveness
    std::map<std::string, double> rule_effectiveness;
    
    // Calculate effectiveness based on usage and current performance
    for (const auto& stat : rule_usage_stats_) {
        if (stat.second > 0) {
            rule_effectiveness[stat.first] = 1.0 / std::sqrt(static_cast<double>(stat.second));
        }
    }
    
    // Adjust weights based on effectiveness
    for (auto& engine_rules : rules_by_engine_) {
        for (auto& rule : engine_rules) {
            if (rule && rule_effectiveness.count(rule->get_name()) > 0) {
                double new_weight = rule->get_weight() * rule_effectiveness[rule->get_name()];
                rule->set_weight(std::max(0.1, std::min(5.0, new_weight))); // Clamp between 0.1 and 5.0
            }
        }
    }
    
    for (auto& rule : global_rules_) {
        if (rule && rule_effectiveness.count(rule->get_name()) > 0) {
            double new_weight = rule->get_weight() * rule_effectiveness[rule->get_name()];
            rule->set_weight(std::max(0.1, std::min(5.0, new_weight))); // Clamp between 0.1 and 5.0
        }
    }
    
    invalidate_weights_cache();
}

void HeuristicRuleManager::optimize_rule_parameters() {
    // Simple parameter optimization - adjust weights based on performance
    balance_rule_weights_automatically();
}

std::vector<std::string> HeuristicRuleManager::get_most_effective_rules() const {
    std::vector<std::pair<std::string, int>> rule_usage(rule_usage_stats_.begin(), rule_usage_stats_.end());
    
    // Sort by usage count
    std::sort(rule_usage.begin(), rule_usage.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });
    
    std::vector<std::string> effective_rules;
    for (const auto& rule : rule_usage) {
        effective_rules.push_back(rule.first);
    }
    
    return effective_rules;
}

void HeuristicRuleManager::export_rule_configuration(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) return;
    
    file << "# Heuristic Rule Configuration\n";
    file << "# Format: RuleName,Weight,Enabled\n";
    
    // Export engine-specific rules
    for (size_t engine = 0; engine < rules_by_engine_.size(); ++engine) {
        for (const auto& rule : rules_by_engine_[engine]) {
            if (rule) {
                file << rule->get_name() << "," << rule->get_weight() << "," 
                     << (rule->is_enabled() ? "1" : "0") << "\n";
            }
        }
    }
    
    // Export global rules
    for (const auto& rule : global_rules_) {
        if (rule) {
            file << rule->get_name() << "," << rule->get_weight() << "," 
                 << (rule->is_enabled() ? "1" : "0") << "\n";
        }
    }
}

void HeuristicRuleManager::import_rule_configuration(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return;
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue; // Skip comments and empty lines
        
        std::istringstream iss(line);
        std::string name, weight_str, enabled_str;
        
        if (std::getline(iss, name, ',') &&
            std::getline(iss, weight_str, ',') &&
            std::getline(iss, enabled_str)) {
            
            double weight = std::stod(weight_str);
            bool enabled = (enabled_str == "1");
            
            // Find and configure the rule
            bool found = false;
            
            // Search engine-specific rules
            for (auto& engine_rules : rules_by_engine_) {
                for (auto& rule : engine_rules) {
                    if (rule && rule->get_name() == name) {
                        rule->set_weight(weight);
                        if (enabled) rule->enable();
                        else rule->disable();
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
            
            // Search global rules if not found
            if (!found) {
                for (auto& rule : global_rules_) {
                    if (rule && rule->get_name() == name) {
                        rule->set_weight(weight);
                        if (enabled) rule->enable();
                        else rule->disable();
                        break;
                    }
                }
            }
        }
    }
    
    invalidate_weights_cache();
}
// =============================================================================
// HeuristicRuleFactory Implementation
// =============================================================================

std::unique_ptr<HeuristicRule> HeuristicRuleFactory::create_stepwise_motion_rule(int engine, double weight) {
    return std::make_unique<SingleEnginePitchRule>(
        engine,
        SingleEnginePitchRule::create_stepwise_motion_preference(),
        "Stepwise Motion Preference",
        3, // window size
        weight
    );
}

std::unique_ptr<HeuristicRule> HeuristicRuleFactory::create_consonant_intervals_rule(int engine, double weight) {
    return std::make_unique<SingleEnginePitchRule>(
        engine,
        SingleEnginePitchRule::create_consonant_intervals_preference(),
        "Consonant Intervals Preference",
        2, // window size
        weight
    );
}

std::unique_ptr<HeuristicRule> HeuristicRuleFactory::create_rhythmic_regularity_rule(int engine, double weight) {
    return std::make_unique<SingleEngineRhythmRule>(
        engine,
        SingleEngineRhythmRule::create_rhythmic_regularity_preference(),
        "Rhythmic Regularity Preference",
        4, // window size
        weight
    );
}

std::unique_ptr<HeuristicRule> HeuristicRuleFactory::create_pitch_stability_rule(int engine, double weight) {
    return std::make_unique<SingleEnginePitchRule>(
        engine,
        SingleEnginePitchRule::create_pitch_stability_preference(),
        "Pitch Stability Preference",
        4, // window size
        weight
    );
}

std::unique_ptr<HeuristicRule> HeuristicRuleFactory::create_rhythm_pitch_coordination_rule(
    int rhythm_engine, int pitch_engine, double weight) {
    return std::make_unique<DualEngineRhythmPitchRule>(
        rhythm_engine,
        pitch_engine,
        DualEngineRhythmPitchRule::create_strong_beat_emphasis(),
        "Rhythm-Pitch Coordination",
        3, // window size
        weight
    );
}

std::unique_ptr<HeuristicRule> HeuristicRuleFactory::create_harmonic_consonance_rule(
    const std::vector<int>& pitch_engines, double weight) {
    return std::make_unique<MultiEngineHarmonicRule>(
        pitch_engines,
        MultiEngineHarmonicRule::create_consonance_preference(),
        "Harmonic Consonance",
        2, // window size
        weight
    );
}

std::unique_ptr<HeuristicRule> HeuristicRuleFactory::create_voice_leading_rule(
    const std::vector<int>& pitch_engines, double weight) {
    return std::make_unique<MultiEngineHarmonicRule>(
        pitch_engines,
        MultiEngineHarmonicRule::create_voice_leading_smoothness(),
        "Voice Leading Smoothness",
        2, // window size
        weight
    );
}

std::unique_ptr<HeuristicRule> HeuristicRuleFactory::create_no_repetition_switch(
    int engine, double penalty_weight) {
    return std::make_unique<SwitchHeuristicRule>(
        std::vector<int>{engine},
        SwitchHeuristicRule::create_avoid_repetition_condition(),
        penalty_weight,
        "No Repetition Switch"
    );
}

std::unique_ptr<HeuristicRule> HeuristicRuleFactory::create_range_enforcement_switch(
    int engine, int min_pitch, int max_pitch, double penalty_weight) {
    return std::make_unique<SwitchHeuristicRule>(
        std::vector<int>{engine},
        SwitchHeuristicRule::create_range_enforcement_condition(min_pitch, max_pitch),
        penalty_weight,
        "Range Enforcement Switch"
    );
}

std::unique_ptr<HeuristicRule> HeuristicRuleFactory::create_classical_style_rule(
    int rhythm_engine, int pitch_engine, double weight) {
    return std::make_unique<DualEngineRhythmPitchRule>(
        rhythm_engine,
        pitch_engine,
        DualEngineRhythmPitchRule::create_cadential_pattern_preference(),
        "Classical Style Rule",
        4, // window size
        weight
    );
}

std::unique_ptr<HeuristicRule> HeuristicRuleFactory::create_jazz_style_rule(
    int rhythm_engine, int pitch_engine, double weight) {
    return std::make_unique<DualEngineRhythmPitchRule>(
        rhythm_engine,
        pitch_engine,
        DualEngineRhythmPitchRule::create_rhythmic_pitch_correlation(),
        "Jazz Style Rule",
        3, // window size
        weight
    );
}

std::unique_ptr<HeuristicRule> HeuristicRuleFactory::create_minimalist_style_rule(
    int engine, double weight) {
    return std::make_unique<SingleEngineRhythmRule>(
        engine,
        SingleEngineRhythmRule::create_rhythmic_variety_preference(),
        "Minimalist Style Rule",
        6, // larger window for pattern detection
        weight
    );
}

void HeuristicRuleFactory::add_classical_music_rules(HeuristicRuleManager& manager, 
                                                     const std::vector<int>& rhythm_engines,
                                                     const std::vector<int>& pitch_engines) {
    // Add stepwise motion preference for all pitch engines
    for (int pitch_engine : pitch_engines) {
        manager.add_rule(create_stepwise_motion_rule(pitch_engine, 2.0));
        manager.add_rule(create_consonant_intervals_rule(pitch_engine, 1.5));
        manager.add_rule(create_pitch_stability_rule(pitch_engine, 1.2));
        manager.add_rule(create_no_repetition_switch(pitch_engine, -3.0));
        manager.add_rule(create_range_enforcement_switch(pitch_engine, 48, 84, -10.0)); // C3 to C6
    }
    
    // Add rhythmic regularity for rhythm engines
    for (int rhythm_engine : rhythm_engines) {
        manager.add_rule(create_rhythmic_regularity_rule(rhythm_engine, 1.8));
    }
    
    // Add coordination rules between rhythm and pitch engines
    for (size_t i = 0; i < std::min(rhythm_engines.size(), pitch_engines.size()); ++i) {
        manager.add_rule(create_classical_style_rule(rhythm_engines[i], pitch_engines[i], 1.5));
        manager.add_rule(create_rhythm_pitch_coordination_rule(rhythm_engines[i], pitch_engines[i], 1.3));
    }
    
    // Add harmonic rules if multiple pitch engines
    if (pitch_engines.size() >= 2) {
        manager.add_rule(create_harmonic_consonance_rule(pitch_engines, 2.5));
        manager.add_rule(create_voice_leading_rule(pitch_engines, 2.0));
    }
}

void HeuristicRuleFactory::add_jazz_music_rules(HeuristicRuleManager& manager,
                                                const std::vector<int>& rhythm_engines,
                                                const std::vector<int>& pitch_engines) {
    // Jazz allows more chromatic motion and rhythmic complexity
    for (int pitch_engine : pitch_engines) {
        manager.add_rule(create_stepwise_motion_rule(pitch_engine, 1.2)); // Less emphasis on stepwise
        manager.add_rule(create_consonant_intervals_rule(pitch_engine, 1.0)); // Allow more dissonance
        manager.add_rule(create_no_repetition_switch(pitch_engine, -2.0));
        manager.add_rule(create_range_enforcement_switch(pitch_engine, 36, 96, -10.0)); // Wider range
    }
    
    // Jazz rhythms are more complex
    for (int rhythm_engine : rhythm_engines) {
        manager.add_rule(create_rhythmic_regularity_rule(rhythm_engine, 1.0)); // Less regular
    }
    
    // Jazz-specific coordination
    for (size_t i = 0; i < std::min(rhythm_engines.size(), pitch_engines.size()); ++i) {
        manager.add_rule(create_jazz_style_rule(rhythm_engines[i], pitch_engines[i], 1.5));
    }
    
    // Jazz harmonic rules (more permissive)
    if (pitch_engines.size() >= 2) {
        manager.add_rule(create_harmonic_consonance_rule(pitch_engines, 1.5)); // Less strict
        manager.add_rule(create_voice_leading_rule(pitch_engines, 1.3));
    }
}

void HeuristicRuleFactory::add_minimal_rules(HeuristicRuleManager& manager, int num_engines) {
    // Add only essential rules for basic functionality
    for (int engine = 0; engine < num_engines; ++engine) {
        if (engine % 2 == 0) {
            // Rhythm engines
            manager.add_rule(create_rhythmic_regularity_rule(engine, 1.2));
        } else {
            // Pitch engines
            manager.add_rule(create_stepwise_motion_rule(engine, 1.5));
            manager.add_rule(create_no_repetition_switch(engine, -1.0));
        }
    }
}

} // namespace ClusterEngine
